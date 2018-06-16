#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> /* sockaddr_in{} and other Internet defns */
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#define	max(a,b)    ((a) > (b) ? (a) : (b))
#define SA struct sockaddr
#define LISTENQ 1024 /* 2nd argument to listen() */
#define SERV_PORT 9876 /* TCP and UDP */
#define MAXLINE 4096 /* max text line */
#define DGLEN 1400
#define NDF 2000


void dg_cli(FILE *fp, int sockfd, const SA * pservaddr, socklen_t servlen)
{
    int n, i;
    char sendline[DGLEN];

    for (i=0; i < NDF; i++){
        printf("send %d DG\n", i);
        sendto(sockfd, sendline, DGLEN, 0, pservaddr, servlen);
    }
}


int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in servaddr;
    uint16_t serPort;

    if (argc != 3){
        printf("usage:udpcli ip port\n");
        exit(1);
    }

    sscanf(argv[2], "%d", &serPort);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
    servaddr.sin_port = htons(serPort);

    dg_cli(stdin, sockfd, (SA *) &servaddr, sizeof(servaddr));
    exit(0);
}
