#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define SERVER_PORT 8080 /* arbitrary, but client & server must agree */
#define BUFSIZE 4096  /* block transfer size */

/*
  Definição do protocolo:

  Cabeçalho: 5 bytes, sendo eles, em ordem:
  1. Tipo de mensagem (1 byte) (1 = MyGet, 2 = OK, 3 = MyLastAccess, 4 = MyLastAccessOK, 5 = FileNotFound, 6 = MyLastAccessNotOK)
  2. ID da mensagem (4 bytes)

  Protocolo Ficheiro:
  1. Cliente envia uma mensagem MyGet com o ID 0 0 0 0 para o servidor
  2. Servidor responde OKs com um ID gerado (4 bytes) e com a mensagem
  3. Cliente escuta a resposta

  Protocolo LastAccess:
  1. Cliente envia uma mensagem MyLastAccess com o ID do cliente (4 bytes)
  2. Servidor responde MyLastAccessOK com o tempo do último acesso
  3. Cliente escuta a resposta
*/

void AskForFile(int s, char *filename, unsigned int id = 0) {
  printf("Solicitando o ficheiro %s usando ID = %u\n", filename, id);
  char buf[BUFSIZE];
  // Colocar o tipo de mensagem
  buf[0] = 1;
  // Colocar o ID da mensagem
  memcpy(&buf[1], &id, 4);
  // Colocar o nome do ficheiro
  strcpy(&buf[5], filename);
  // Enviar requisição
  write(s, buf, 5 + strlen(filename) + 1);
}

void AskForLastAccess(int s, unsigned int id = 0) {
  printf("Solicitando o tempo do último acesso com ID = %u\n", id);
  char buf[BUFSIZE];
  // Colocar o tipo de mensagem
  buf[0] = 3;
  // Colocar o ID da mensagem
  memcpy(&buf[1], &id, 4);
  // Colocar o nome do ficheiro
  char filename[] = "last_access";
  strcpy(&buf[5], filename);
  // Enviar requisições
  write(s, buf, 5 + strlen(filename) + 1);
}

void ReadMessages(int s, unsigned int *id) {
  unsigned char buf[BUFSIZE];
  while (1) {
    int bytes = read(s, buf, BUFSIZE); /* read from socket */
    if (bytes <= 0) break; /* check for end of file */
    /* Request type is in buf[0] */
    int type = buf[0];
    /* Request ID is in buf[1] through buf[4] */
    unsigned int newId = buf[4] << 24 | buf[3] << 16 | buf[2] << 8 | buf[1];
    if (type == 2 || type == 4 || type == 6 ) {
      // Sets the ID
      *id = newId;
    }
    /* Data is in buf[5] through buf[BUF_SIZE] */
    int aux;
    char data[BUFSIZE];
    for (aux = 5; aux < BUFSIZE; aux++) {
        data[aux - 5] = buf[aux];
        if (buf[aux] == 0) break;
    }
    data[aux - 5] = '\0';
    printf("\n----------------INICIO-PACOTE---------------\n");
    printf("type = %u, id = %u,\n\ndata = %s\n", type, newId, data);
    printf("\n------------------FIM-PACOTE----------------\n");
  }
}

void ConnectToServer(int *s, int *c, struct sockaddr_in *channel, struct hostent *h) {
    // Cria o socket
    *s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*s < 0) {
        printf("Falha ao criar o socket\n");
        exit(-1);
    }

    // Configura o canal (estrutura sockaddr_in) para a conexão
    memset(channel, 0, sizeof(*channel));
    channel->sin_family = AF_INET;
    memcpy(&channel->sin_addr.s_addr, h->h_addr, h->h_length);
    channel->sin_port = htons(SERVER_PORT);

    // Tenta se conectar
    *c = connect(*s, (struct sockaddr *)channel, sizeof(*channel));
    if (*c < 0) {
        printf("Falha na conexão\n");
        exit(-1);
    }
}

int main(int argc, char **argv)
{
  int c, s, bytes;
  struct hostent *h;  /* info about server */
  struct sockaddr_in channel; /* holds IP address */
  if (argc != 2) {printf("Usage: client server-name"); exit(-1);}
  h = gethostbyname(argv[1]); /* look up host’s IP address */
  if (!h) {printf("gethostbyname failed to locate %s", argv[1]); exit(-1);}

  unsigned int id = 0;
  char input = 0;
  char filename[BUFSIZE];

  printf("Sistema de requisição de ficheiros\n");
  printf("\nInsira uma opção: \n 'g' para obter um ficheiro\n 'l' para obter o tempo de acesso ao ficheiro\n 'q' para sair\n");
  
  while (input != 'q') {
    printf("Opção: ");
    scanf("%c", &input);

    if (input == 'g') {
      // Estabelece uma nova conexão a cada requisição
      ConnectToServer(&s, &c, &channel, h);
      // Envia a requisição
      printf("Insira o nome do ficheiro: \n");
      scanf("%s", filename);
      AskForFile(s, filename,id);
      ReadMessages(s, &id);
      // Fecha a conexão após a requisição
      close(s);
    } else if (input == 'l') {
      // Estabelece uma nova conexão a cada requisição
      ConnectToServer(&s, &c, &channel, h);
      // Envia a requisição
      AskForLastAccess(s, id);
      ReadMessages(s, &id);
      // Fecha a conexão após a requisição
      close(s);
    }

    // Flush stdin
    while (getchar() != '\n');
  }

  printf("Saindo... pois foi lido %c\n", input);
  return 0;
}

