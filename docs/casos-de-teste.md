# Casos de teste — respostas DNS de referência

Consultas feitas com `dig` (DiG 9.10.6, macOS) em 06/10/2026 para registrar o formato e o conteúdo das respostas que o cliente precisa interpretar. A tabela abaixo define a saída esperada do programa em cada cenário do enunciado.

## Casos de teste

| # | Comando | Status | ANSWER | Saída esperada do programa |
|---|---------|--------|--------|----------------------------|
| 1 | `dig MX unb.br @8.8.8.8` | NOERROR | 1 (MX) | `unb.br <> unb-br.mail.protection.outlook.com` |
| 2 | `dig MX imagdaskdasdasj.br @1.1.1.1` | NXDOMAIN | 0 | `Dominio imagdaskdasdasj.br nao encontrado` |
| 3 | `dig MX fga.unb.br @8.8.8.8` | NOERROR | 1 (CNAME) | `Dominio fga.unb.br nao possui entrada MX` |
| 4 | `dig MX unb.br @1.2.3.4` | sem resposta | — | `Nao foi possível coletar entrada MX para unb.br` |

## Observações

- Todas as respostas trazem a flag `ad` além de `qr rd ra`, e uma seção OPT (EDNS) contada em ADDITIONAL. As duas coisas vêm de opções que o dig liga por padrão e que o cliente do trabalho não vai enviar.
- No caso 2, o domínio inexistente veio com uma seção AUTHORITY contendo um registro SOA da zona `.br`, mesmo sem nenhuma resposta.
- No caso 3, a seção ANSWER não está vazia: contém um registro do tipo **CNAME** apontando `fga.unb.br` para `fcte.unb.br`, e nenhum registro MX.
- No caso 4, a consulta foi feita sem `+time=2 +tries=3`, então usou os padrões do dig, e não o comportamento pedido no enunciado.


## Saídas completas

### Caso 1 — resolução bem-sucedida

```
$ dig MX unb.br @8.8.8.8

; <<>> DiG 9.10.6 <<>> MX unb.br @8.8.8.8
;; global options: +cmd
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 36355
;; flags: qr rd ra ad; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 1

;; OPT PSEUDOSECTION:
; EDNS: version: 0, flags:; udp: 512
;; QUESTION SECTION:
;unb.br.                                IN      MX

;; ANSWER SECTION:
unb.br.                 5       IN      MX      0 unb-br.mail.protection.outlook.com.

;; Query time: 26 msec
;; SERVER: 8.8.8.8#53(8.8.8.8)
;; WHEN: Tue Oct 06 14:19:19 -03 2026
;; MSG SIZE  rcvd: 85
```

### Caso 2 — domínio inexistente

```
$ dig MX imagdaskdasdasj.br @1.1.1.1

; <<>> DiG 9.10.6 <<>> MX imagdaskdasdasj.br @1.1.1.1
;; global options: +cmd
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NXDOMAIN, id: 45453
;; flags: qr rd ra ad; QUERY: 1, ANSWER: 0, AUTHORITY: 1, ADDITIONAL: 1

;; OPT PSEUDOSECTION:
; EDNS: version: 0, flags:; udp: 1232
;; QUESTION SECTION:
;imagdaskdasdasj.br.            IN      MX

;; AUTHORITY SECTION:
br.                     900     IN      SOA     a.dns.br. hostmaster.registro.br. 2026279418 1800 900 604800 900

;; Query time: 29 msec
;; SERVER: 1.1.1.1#53(1.1.1.1)
;; WHEN: Tue Oct 06 14:30:01 -03 2026
;; MSG SIZE  rcvd: 109
```

### Caso 3 — domínio sem registro MX

```
$ dig MX fga.unb.br @8.8.8.8

; <<>> DiG 9.10.6 <<>> MX fga.unb.br @8.8.8.8
;; global options: +cmd
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 39909
;; flags: qr rd ra ad; QUERY: 1, ANSWER: 1, AUTHORITY: 1, ADDITIONAL: 1

;; OPT PSEUDOSECTION:
; EDNS: version: 0, flags:; udp: 512
;; QUESTION SECTION:
;fga.unb.br.                    IN      MX

;; ANSWER SECTION:
fga.unb.br.             1800    IN      CNAME   fcte.unb.br.

;; AUTHORITY SECTION:
unb.br.                 1800    IN      SOA     dns1.unb.br. hostmaster.unb.br. 2026100502 14400 3600 1209600 3600

;; Query time: 67 msec
;; SERVER: 8.8.8.8#53(8.8.8.8)
;; WHEN: Tue Oct 06 14:30:35 -03 2026
;; MSG SIZE  rcvd: 110
```

### Caso 4 — servidor não responde

```
$ dig MX unb.br @1.2.3.4

; <<>> DiG 9.10.6 <<>> MX unb.br @1.2.3.4
;; global options: +cmd
;; connection timed out; no servers could be reached
```