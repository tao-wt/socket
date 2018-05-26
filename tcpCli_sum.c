#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> /* sockaddr_in{} and other Internet defns */
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include "sum.c"

#define SA struct sockaddr
#define LISTENQ 1024 /* 2nd argument to listen() */
#define SERV_PORT 9877 /* TCP and UDP */
#define MAXLINE 4096 /* max text line length */

ssize_t                        /* Write "n" bytes to a descriptor. */
writen(int fd, const void *vptr, size_t n)
{
    size_t        nleft;
    ssize_t       nwritten;
    const char    *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;        /* and call write() again */
            else
                return(-1);            /* error */
        }

        nleft -= nwritten;
        ptr   += nwritten;
    }
    return(n);
}

ssize_t
readline(int fd, void *vptr, size_t maxlen)
{
    ssize_t    n, rc;
    char    c, *ptr;

    ptr = vptr;
    for (n = 1; n < maxlen; n++) {
again:
        if ( (rc = read(fd, &c, 1)) == 1) {
            *ptr++ = c;
            if (c == '\n')
                break;    /* newline is stored, like fgets() */
        } else if (rc == 0 && errno == 0) {
            *ptr = 0;
            return(n - 1);    /* EOF, n - 1 bytes were read */
        } else {
            if (errno == EINTR)
                goto again;
            if (errno == ECONNRESET)
                printf("Connect reset!\n");
            if (errno == ETIMEDOUT)
                printf("Time out!\n");
            if (errno == ENETUNREACH)
                printf("Net unreach\n");
            if (errno == EHOSTUNREACH)
                printf("Host unreach!\n");
            return(-1);        /* error, errno set by read() */
        }
    }

    *ptr = 0;    /* null terminate like fgets() */
    return(n);
}

void
str_cli(FILE *fp, int sockfd)
{
    char            sendline[MAXLINE];
    struct args        args;
    struct result    result;

    while (fgets(sendline, MAXLINE, fp) != NULL) {

        if (sscanf(sendline, "%ld%ld", &args.arg1, &args.arg2) != 2) {
            printf("invalid input: %s", sendline);
            continue;
        }
        writen(sockfd, &args, sizeof(args));

        if (readline(sockfd, &result, sizeof(result)) == 0)
            printf("str_cli: server terminated prematurely");

        printf("%ld\n", result.sum);
    }
}

int
main(int argc, char **argv)
{
    int sockfd;
    uint16_t serPort;
    struct sockaddr_in    servaddr;

    printf("argc is %d\n", argc);
    if (argc != 3){
        printf("usage: tcpcli ip port\n");
        exit(1);
    }
    sscanf(argv[2], "%d", &serPort);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    printf("The socket is %d\n", sockfd);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(serPort);
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

    printf("Start connect...\n");
    if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0){
        printf("connect error,exit.\n");
        exit(1);
    }
    printf("connect ok!\n");

    str_cli(stdin, sockfd);        /* do it all */

    exit(0);
}
