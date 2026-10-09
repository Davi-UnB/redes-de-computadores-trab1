#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define PORTA_DNS 53
#define TAM_MAX_DNS 512    // Tamanho máximo de uma mensagem DNS via UDP (RFC 1035, seção 2.3.4)
#define MAX_TENTATIVAS 3   // Enunciado: até 3 tentativas de resolução
#define TIMEOUT_SEGUNDOS 2 // Enunciado: aguardar 2 segundos pela resposta

static void escreve_16(uint8_t *buf, size_t *pos, uint16_t v) {
  buf[*pos] = (uint8_t)(v >> 8);
  buf[*pos + 1] = (uint8_t)v;
  *pos += 2;
}

static int escreve_nome(uint8_t *buf, size_t *pos, size_t tam_buf, const char *dominio) {
  size_t p = *pos;       // Posição de escrita local
  size_t idx_tam;        // Onde fica o byte de tamanho do rótulo atual
  size_t tam_rotulo = 0; // Letras contadas no rótulo atual

  if (p >= tam_buf)
    return -1;
  idx_tam = p++; // Reserva o byte de tamanho do primeiro rótulo

  for (const char *c = dominio;; c++) {
    if (*c == '.' || *c == '\0') {
      if (tam_rotulo == 0) {
        // Rótulo vazio só é aceito como ponto final ("unb.br."):
        // nesse caso o byte reservado vira o terminador
        if (*c == '\0' && idx_tam != *pos) {
          buf[idx_tam] = 0;
          break;
        }
        return -1; // Domínio vazio, ponto no início ou ".."
      }
      buf[idx_tam] = (uint8_t)tam_rotulo; // Preenche o tamanho que estava reservado
      if (p >= tam_buf)
        return -1;
      idx_tam = p++; // Reserva o byte do próximo rótulo (ou do terminador)
      tam_rotulo = 0;
      if (*c == '\0') {
        buf[idx_tam] = 0; // Fim do domínio: byte zero que encerra o nome
        break;
      }
    } else {
      if (++tam_rotulo > 63 || p >= tam_buf)
        return -1;            // Rótulo maior que o limite da RFC, ou buffer cheio
      buf[p++] = (uint8_t)*c; // Copia a letra
    }
  }

  if (p - *pos > 255)
    return -1; // Nome inteiro maior que o limite da RFC (seção 3.1)

  *pos = p; // Só atualiza a posição de quem chamou se deu tudo certo
  return 0;
}

static size_t monta_consulta(uint8_t *buf, size_t tam_buf, uint16_t id, const char *dominio) {
  size_t pos = 0;

  // Header DNS (12 bytes)
  escreve_16(buf, &pos, id);     // Identificador
  escreve_16(buf, &pos, 0x0100); // Flags
  escreve_16(buf, &pos, 1);      // QDCOUNT = 1
  escreve_16(buf, &pos, 0);      // ANCOUNT = 0
  escreve_16(buf, &pos, 0);      // NSCOUNT = 0
  escreve_16(buf, &pos, 0);      // ARCOUNT = 0

  // Question section
  if (escreve_nome(buf, &pos, tam_buf - 4, dominio) != 0)
    return 0;                // QNAME
  escreve_16(buf, &pos, 15); // QTYPE - MX
  escreve_16(buf, &pos, 1);  // QCLASS - IN

  return pos;
}

// Lê um número de 16 bits e avança para o próximo
static int le_16(const uint8_t *buf, size_t tam, size_t *pos) {
  // Confirma se *pos + 1 < tamanho do buffer
  if (*pos + 1 >= tam) {
    return -1; // Erro
  }

  uint16_t valor = (uint16_t)(buf[*pos] << 8); // Coloca byte atual no byte alto
  valor |= (uint16_t)buf[*pos + 1];            // Coloca próximo byte no byte baixo
  *pos += 2;                                   // Avança para o próximo número de 16 bits (2 bytes)

  return valor;
}

// Interpreta a resposta e imprime o resultado formatado
static int interpreta_resposta(const uint8_t *resp, size_t tam, const char *dominio) {
  size_t pos = 0;

  // ID (2 bytes):
  pos += 2; // Pulamos

  // Flags (2 bytes):
  int flags = le_16(resp, tam, &pos);

  if (flags < 0) {
    printf("Nao foi possível coletar entrada MX para %s\n", dominio);
    return EXIT_FAILURE;
  }

  int rcode = flags & 0x000F; // Pega últimos 4 bits

  // RCODE 3: o domínio não existe
  if (rcode == 3) {
    printf("Dominio %s nao encontrado\n", dominio);
    return EXIT_FAILURE;
  }

  // Qualquer outro RCODE diferente de 0 é uma falha do servidor
  if (rcode != 0) {
    printf("Nao foi possível coletar entrada MX para %s\n", dominio);
    return EXIT_FAILURE;
  }

  // RCODE 0: sucesso
  return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {

  // Verificador de argumentos
  if (argc != 3) {
    fprintf(stderr,
            "Erro: Numero de argumentos inválido\n"
            "Uso: %s <dominio> <ip_servidor> \n",
            argv[0]);
    return EXIT_FAILURE;
  }

  const char *dominio = argv[1];
  const char *ip = argv[2];

  // Prepara o endereço do servidor DNS
  struct sockaddr_in servidor;
  memset(&servidor, 0, sizeof(servidor)); // Zera o struct para eliminar lixo de memória
  servidor.sin_family = AF_INET;          // https://www.ibm.com/docs/pt-br/i/7.6.0?topic=family-af-inet-address
  servidor.sin_port = htons(PORTA_DNS);   // Converte o número da porta para a ordem de bytes da rede

  if (inet_pton(AF_INET, ip, &servidor.sin_addr) != 1) {
    fprintf(stderr, "Erro: IP invalido: %s\n", ip);
    return EXIT_FAILURE;
  }

  // Montar Consulta DNS

  // Geração de um número de identificação de 16 bits aleatório
  srand(time(NULL));
  uint16_t id = (uint16_t)(rand() & 0xFFFF);

  // Buffer da consulta, inicializado com zeros
  uint8_t consulta[TAM_MAX_DNS] = {0};

  size_t tam_consulta = monta_consulta(consulta, sizeof(consulta), id, dominio);
  if (tam_consulta == 0) {
    fprintf(stderr, "Erro: dominio invalido: %s\n", dominio);
    return EXIT_FAILURE;
  }

  // Cria Socket UDP
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket");
    return EXIT_FAILURE;
  }

  // Define o tempo máximo de espera do recvfrom
  struct timeval timeout = {.tv_sec = TIMEOUT_SEGUNDOS, .tv_usec = 0};
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    perror("setsockopt");
    close(sock);
    return EXIT_FAILURE;
  }

  uint8_t resposta[TAM_MAX_DNS] = {0}; // Buffer da resposta
  ssize_t recebidos = -1;

  for (int tentativa = 0; tentativa < MAX_TENTATIVAS; tentativa++) {
    // Envia (ou reenvia) a consulta
    ssize_t enviados = sendto(sock, consulta, tam_consulta, 0, (struct sockaddr *)&servidor, sizeof(servidor));
    if (enviados < 0) {
      perror("sendto");
      close(sock);
      return EXIT_FAILURE;
    }

    // Espera a resposta por até TIMEOUT_SEGUNDOS
    recebidos = recvfrom(sock, resposta, sizeof(resposta), 0, NULL, NULL);
    if (recebidos < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        continue; // Tempo esgotado: tenta de novo
      perror("recvfrom");
      close(sock);
      return EXIT_FAILURE;
    }

    // Descarta respostas menores que um cabeçalho ou de outra consulta
    uint16_t id_resposta = (uint16_t)((resposta[0] << 8) | resposta[1]);
    if (recebidos < 12 || id_resposta != id) {
      recebidos = -1;
      continue;
    }

    break; // Resposta válida recebida
  }

  close(sock); // O socket não é mais necessário

  if (recebidos < 0) {
    printf("Nao foi possível coletar entrada MX para %s\n", dominio);
    return EXIT_FAILURE;
  }

  return interpreta_resposta(resposta, (size_t)recebidos, dominio);
}