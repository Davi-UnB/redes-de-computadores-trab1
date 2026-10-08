#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PORTA_DNS 53
#define TAM_MAX_DNS 512 // Tamanho máximo de uma mensagem DNS via UDP (RFC 1035, seção 2.3.4)


static void escreve_16(uint8_t *buf, size_t *pos, uint16_t v) {
  buf[*pos] = (uint8_t)(v >> 8);
  buf[*pos + 1] = (uint8_t)v;
  *pos += 2;
}

static int escreve_nome(uint8_t *buf, size_t *pos, size_t tam_buf,const char *dominio) {
  size_t p = *pos;         // Posição de escrita local
  size_t idx_tam;          // Onde fica o byte de tamanho do rótulo atual
  size_t tam_rotulo = 0;   // Letras contadas no rótulo atual

  if (p >= tam_buf)
    return -1;
  idx_tam = p++;           // Reserva o byte de tamanho do primeiro rótulo

  for (const char *c = dominio;; c++) {
    if (*c == '.' || *c == '\0') {
      if (tam_rotulo == 0) {
        // Rótulo vazio só é aceito como ponto final ("unb.br."):
        // nesse caso o byte reservado vira o terminador
        if (*c == '\0' && idx_tam != *pos) {
          buf[idx_tam] = 0;
          break;
        }
        return -1;         // Domínio vazio, ponto no início ou ".."
      }
      buf[idx_tam] = (uint8_t)tam_rotulo; // Preenche o tamanho que estava reservado
      if (p >= tam_buf)
        return -1;
      idx_tam = p++;       // Reserva o byte do próximo rótulo (ou do terminador)
      tam_rotulo = 0;
      if (*c == '\0') {
        buf[idx_tam] = 0;  // Fim do domínio: byte zero que encerra o nome
        break;
      }
    } else {
      if (++tam_rotulo > 63 || p >= tam_buf)
        return -1;         // Rótulo maior que o limite da RFC, ou buffer cheio
      buf[p++] = (uint8_t)*c; // Copia a letra
    }
  }

  if (p - *pos > 255)
    return -1;             // Nome inteiro maior que o limite da RFC (seção 3.1)

  *pos = p;                // Só atualiza a posição de quem chamou se deu tudo certo
  return 0;
}

static size_t monta_consulta(uint8_t *buf, size_t tam_buf, uint16_t id,const char *dominio) {
  size_t pos = 0;

  // Header DNS (12 bytes)
  escreve_16(buf, &pos, id);     // Identificador
  escreve_16(buf, &pos, 0x0100); // Flags
  escreve_16(buf, &pos, 1);      // QDCOUNT = 1
  escreve_16(buf, &pos, 0);      // ANCOUNT = 0
  escreve_16(buf, &pos, 0);      // NSCOUNT = 0
  escreve_16(buf, &pos, 0);      // ARCOUNT = 0

  // Question section
  if (escreve_nome(buf, &pos, tam_buf - 4, dominio) != 0) return 0; // QNAME
  escreve_16(buf, &pos, 15); // QTYPE - MX
  escreve_16(buf, &pos, 1); // QCLASS - IN

  return pos;
}

int main(int argc, char *argv[]) {

  // Verificador de argumentos
  if (argc != 3) {
    fprintf(stderr,
            "Erro: Numero de argumentos inválido\n"
            "Uso: %s <dominio> <ip_servidor> \n",argv[0]);
    return EXIT_FAILURE;
  }

  char *dominio = argv[1];
  char *ip = argv[2];

  // Prepara o endereço do servidor DNS
  struct sockaddr_in servidor;
  memset(&servidor, 0, sizeof(servidor)); // Zera o struct para eliminar lixo de memória
  servidor.sin_family = AF_INET; // https://www.ibm.com/docs/pt-br/i/7.6.0?topic=family-af-inet-address
  servidor.sin_port = htons(PORTA_DNS); // Converte o número da porta para a ordem de bytes da rede

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

  return EXIT_SUCCESS;
}