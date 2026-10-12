CC ?= cc
CFLAGS = -Wall -Wextra -O2

meu_cliente: meu_cliente.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f meu_cliente

.PHONY: clean
