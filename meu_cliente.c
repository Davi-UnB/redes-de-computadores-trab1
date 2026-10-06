#include <stdio.h>
#include <stdlib.h>

#define PORTA_DNS 53

int main(int argc, char *argv[]) {

  // Caso o usuário insira os parâmetros errado
  if (argc != 3) {
    fprintf(stderr,
            "Erro: Numero de argumentos inválido\n"
            "Uso: %s <dominio> <ip_servidor> \n",
            argv[0]);
    return EXIT_FAILURE;
  }

  // Caso o usuário insira os parâmetros de forma correta

  char *dominio = argv[1];
  char *ip = argv[2];

  return EXIT_SUCCESS;
}