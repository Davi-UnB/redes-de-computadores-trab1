# Cliente DNS — Consulta de Registros MX

Trabalho 01 da disciplina **Fundamentos de Redes de Computadores** — FCTE/UnB
Prof. Tiago Alves

Cliente DNS escrito em C que descobre o servidor de e-mail (registro **MX**) de um domínio. A mensagem de consulta é montada byte a byte, conforme as RFC 1034 e RFC 1035, e enviada por UDP diretamente ao servidor DNS informado, sem uso de nenhuma biblioteca de resolução de nomes.

---

## Autores

| Nome | Matrícula |
|------|-----------|
| Davi Severiano Freitas | 241011018 |
| Mateus Rodrigues Barreto | 241011466 |

---

## Ambiente de desenvolvimento

O trabalho foi desenvolvido em dois sistemas operacionais, com o mesmo código-fonte:

| | Integrante 1 | Integrante 2 |
|--|--------------|--------------|
| Sistema operacional | macOS _27.0.1_ | Ubuntu 24.04.4 LTS via WSL2 (Windows) |
| Compilador | Apple clang 21.0.0 | GCC 13.3.0 |
| Build | GNU Make | GNU Make 4.3 |
| Editor | Antigravity IDE | Antigravity IDE |

O código usa apenas a biblioteca padrão de C e a API de sockets POSIX (`sys/socket.h`, `netinet/in.h`, `arpa/inet.h`). Compila sem alterações em Linux e macOS. Não compila no Windows nativo, cuja API de sockets (Winsock) é diferente; no Windows, use o WSL.

---

## Como construir a aplicação

No Ubuntu (ou no WSL), instale as ferramentas de compilação e, para conferir os resultados, o `dig`:

```bash
sudo apt update
sudo apt install build-essential dnsutils
```

Com o Makefile do repositório:

```bash
make
```

O executável `meu_cliente` é gerado na raiz do projeto.

Sem o Makefile, o comando equivalente é:

```bash
cc -Wall -Wextra -O2 -o meu_cliente meu_cliente.c
```

---

## Como executar a aplicação

```bash
./meu_cliente <dominio> <ip_do_servidor_dns>
```

| Argumento | Descrição | Exemplo |
|-----------|-----------|---------|
| `dominio` | Domínio cujo servidor de e-mail se deseja descobrir | `unb.br` |
| `ip_do_servidor_dns` | Endereço IPv4 do servidor DNS a ser consultado | `8.8.8.8` |

A consulta é sempre enviada para a porta **UDP 53** do servidor informado.

---

## Telas e instruções de uso

A aplicação funciona inteiramente pela linha de comando: os dados entram pelos argumentos e o resultado sai na saída padrão, em uma única linha. Os cenários possíveis são os seguintes.

**Resolução bem-sucedida.** O domínio possui registro MX:

```
$ ./meu_cliente unb.br 8.8.8.8
unb.br <> unb-br.mail.protection.outlook.com
```

**Domínio com vários servidores de e-mail.** É exibido o de menor valor de preferência, que é o de maior prioridade segundo a RFC 1035:

```
$ ./meu_cliente gmail.com 8.8.8.8
gmail.com <> gmail-smtp-in.l.google.com
```

**Domínio inexistente.** O servidor respondeu com o código de erro 3 (*name error*):

```
$ ./meu_cliente imagdaskdasdasj.br 1.1.1.1
Dominio imagdaskdasdasj.br nao encontrado
```

**Domínio sem registro MX.** O domínio existe, mas nenhum registro da resposta é do tipo MX. Isso inclui domínios que são apelidos (registro CNAME), como o `fga.unb.br`, que hoje aponta para `fcte.unb.br`:

```
$ ./meu_cliente fga.unb.br 8.8.8.8
Dominio fga.unb.br nao possui entrada MX
```

**Domínio com "null MX".** Alguns domínios publicam um MX apontando para a raiz do DNS (`MX 0 .`, RFC 7505) para declarar que não recebem e-mail. O `example.com` é um deles. O programa mostra o que o servidor respondeu, e o nome do servidor sai vazio:

```
$ ./meu_cliente example.com 8.8.8.8
example.com <> 
```

Pela RFC 1035, esse registro é um MX válido cujo servidor é a raiz do DNS, então o programa o trata como um MX encontrado (código de saída 0). Optamos por não transformá-lo em "nao possui entrada MX" porque essa mensagem sugere ao usuário que o e-mail poderia ser entregue diretamente no domínio, que é a regra usada quando não há MX (RFC 5321). O null MX declara exatamente o contrário.

**Servidor não respondeu.** Nenhuma resposta válida chegou após 3 tentativas de 2 segundos cada (cerca de 6 segundos no total). A mesma mensagem é usada quando o servidor responde com outro código de erro (falha interna ou recusa) ou envia uma resposta malformada:

```
$ ./meu_cliente unb.br 1.2.3.4
Nao foi possível coletar entrada MX para unb.br
```

**Uso incorreto.** Mensagens de erro vão para a saída de erro (`stderr`):

```
$ ./meu_cliente
Erro: Numero de argumentos inválido
Uso: ./meu_cliente <dominio> <ip_servidor>

$ ./meu_cliente unb.br banana
Erro: IP invalido: banana

$ ./meu_cliente unb..br 8.8.8.8
Erro: dominio invalido: unb..br
```

**Código de saída.** O programa retorna 0 quando encontra um registro MX e 1 em todos os outros casos, o que permite usá-lo em scripts.

---

## Funcionamento

Todo o código está em `meu_cliente.c`. O fluxo de uma execução é:

1. **Validação dos argumentos.** O IP é convertido e validado com `inet_pton`.
2. **Montagem da consulta** (`monta_consulta`). O cabeçalho de 12 bytes é preenchido com um ID aleatório de 16 bits, flags `0x0100` (consulta padrão com recursão desejada), QDCOUNT = 1 e os demais contadores zerados. Em seguida vem a seção de pergunta: o domínio no formato de rótulos (cada parte precedida pelo seu tamanho e terminada por um byte zero), o tipo MX (15) e a classe IN (1). Para `unb.br`, a consulta tem 24 bytes.
3. **Envio e espera.** Um socket UDP envia a consulta com `sendto` e aguarda a resposta com `recvfrom`, configurado com tempo limite de 2 segundos por `setsockopt` (`SO_RCVTIMEO`). Sem resposta, a consulta é reenviada, até 3 tentativas.
4. **Validação da resposta.** Respostas com menos de 12 bytes ou com ID diferente do enviado são descartadas.
5. **Interpretação** (`interpreta_resposta`). O código de resposta (RCODE) é lido das flags. A seção de pergunta, que o servidor devolve repetida, é pulada. Cada registro da seção de respostas é lido, e apenas os do tipo MX e classe IN são considerados. O nome do servidor de e-mail é decodificado com suporte à compressão de nomes (ponteiros da RFC 1035, seção 4.1.4), e o registro de menor preferência é guardado. Um registro cujo servidor é a raiz do DNS (null MX) é tratado como qualquer outro, e o nome sai vazio.

### Robustez da leitura

Toda leitura da resposta confere os limites do buffer recebido, de modo que uma resposta truncada ou malformada resulta na mensagem de falha, e não em leitura fora da memória. A decodificação de nomes limita a 20 o número de ponteiros de compressão seguidos, o que impede um laço infinito causado por ponteiros circulares. O campo RDLENGTH de cada registro é usado para posicionar a leitura do registro seguinte, de modo que registros de tipos desconhecidos são pulados com segurança.

---

## Testes realizados

Os cenários do enunciado foram verificados contra respostas reais de servidores públicos e comparados com o `dig`. As respostas de referência estão em `docs/casos-de-teste.md`.

A interpretação também foi testada isoladamente com respostas montadas manualmente, compiladas com o *AddressSanitizer* e o *UndefinedBehaviorSanitizer* ativos, sem nenhum acesso inválido à memória. Os casos cobertos foram: resposta real de `unb.br`; domínio inexistente; CNAME sem MX; vários MX fora de ordem com nomes comprimidos; null MX; resposta sem registros; códigos de erro SERVFAIL e REFUSED; ponteiro de compressão circular; ponteiro para fora da mensagem; registro truncado; RDLENGTH maior que a mensagem; contadores absurdos no cabeçalho; e rótulos com bits reservados.

O mecanismo de tempo limite foi testado com um servidor DNS local simulado, nos cenários: resposta imediata; primeira resposta com ID errado seguida de resposta correta; resposta apenas na terceira tentativa (cerca de 4 segundos); e servidor que nunca responde (3 envios e cerca de 6 segundos até a mensagem de falha).

---

## Limitações conhecidas

- **Apenas IPv4.** O servidor DNS deve ser informado como endereço IPv4. Endereços IPv6 e nomes de servidor não são aceitos.
- **Sem fallback para TCP.** Se o servidor marcar a resposta como truncada (bit TC), o programa interpreta apenas o que chegou, em vez de repetir a consulta por TCP como prevê a RFC 1035.
- **Sem EDNS.** As respostas ficam limitadas aos 512 bytes do DNS clássico sobre UDP.
- **Apenas um servidor de e-mail é exibido.** Havendo vários registros MX, só o de maior prioridade aparece. Em caso de empate na preferência, é exibido o primeiro recebido.
- **Null MX não é interpretado.** Domínios com `MX 0 .` (como o `example.com`) são exibidos com o nome do servidor vazio, em vez de uma mensagem dizendo que o domínio não recebe e-mail.
- **Respostas com ID errado consomem uma tentativa.** Uma resposta descartada por ID divergente conta como uma das 3 tentativas.
- **Origem da resposta não é verificada.** O programa não confere se a resposta veio do mesmo IP para o qual a consulta foi enviada; a única verificação é o ID.
- **ID gerado com `rand()`.** O identificador não é criptograficamente seguro, o que tornaria o cliente vulnerável a respostas forjadas em uma rede hostil.
- **Erros do servidor agrupados.** Códigos como falha interna (SERVFAIL) e recusa (REFUSED) resultam na mesma mensagem de falha de coleta, sem distinção para o usuário.
- **Sem suporte a domínios internacionalizados.** Domínios com acentos ou caracteres não-ASCII precisariam de conversão Punycode, que não é feita.
- **Dependência da rede.** Firewalls que bloqueiam tráfego UDP na porta 53 para servidores externos impedem a consulta, resultando na mensagem de falha.

---

## Estrutura do repositório

```
.
├── meu_cliente.c            # código-fonte completo
├── Makefile                 # compilação
├── README.md                # esta documentação
└── docs/
    └── casos-de-teste.md    # respostas de referência obtidas com dig
```

---

## Referências

- MOCKAPETRIS, P. **RFC 1034** — *Domain Names: Concepts and Facilities*. IETF, 1987.
- MOCKAPETRIS, P. **RFC 1035** — *Domain Names: Implementation and Specification*. IETF, 1987.
- KLENSIN, J. **RFC 5321** — *Simple Mail Transfer Protocol*. IETF, 2008.
- LEVINE, J.; DELANY, M. **RFC 7505** — *A "Null MX" No Service Resource Record for Domains That Accept No Mail*. IETF, 2015.
- MITCHELL, Mark; OLDHAM, Jeffrey; SAMUEL, Alex. *Advanced Linux Programming*. New Riders, 2001.