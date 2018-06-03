#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>	/* sockaddr_in{} and other Internet defns */
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/wait.h>
#include "signal.c"

#define INFTIM -1
#define	SA struct sockaddr
#define	LISTENQ	1024 /* 2nd argument to listen() */
#define	SERV_PORT 9877 /* TCP and UDP */
#define	MAXLINE	4096 /* max text line length */
#define OPEN_MAX 1024

void
sig_chld(int signo)
{
    pid_t pid;
    int stat;

    // pid = wait(&stat);
    while ( (pid = waitpid(-1, &stat, WNOHANG)) > 0)
        printf("child %d terminated\n", pid);
    return;
}

void
sig_term(int signo){
    printf("Get SIGTERM signo %d\n", signo);
}

ssize_t                        /* Write "n" bytes to a descriptor. */
writen(int fd, const void *vptr, size_t n)
{
    size_t        nleft;
    ssize_t        nwritten;
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
        } else if (rc == 0) {
            *ptr = 0;
            return(n - 1);    /* EOF, n - 1 bytes were read */
        } else {
            if (errno == EINTR)
                goto again;
            return(-1);        /* error, errno set by read() */
        }
    }

    *ptr = 0;    /* null terminate like fgets() */
    return(n);
}

void
str_echo(int sockfd)
{
    ssize_t n;
    char buf[MAXLINE];

    while ( (n = readline(sockfd, buf, MAXLINE)) > 0)
        writen(sockfd, buf, n);

    if (n == -1)
        printf("str_echo: read error");
    else if (n == 0)
        printf("finish.");
}

int
main(int argc, char **argv)
{
    int       i, maxi, listenfd, connfd, sockfd;
    int       nready;
    ssize_t   n;
    // fd_set    rset, allset;
    char      buf[MAXLINE], buff[MAXLINE];
    socklen_t clilen, len;
    struct pollfd client[OPEN_MAX];
    struct sockaddr_in cliaddr, servaddr, ss;
    len = sizeof(ss);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(SERV_PORT);

    bind(listenfd, (SA *) &servaddr, sizeof(servaddr));

    listen(listenfd, LISTENQ);

    // Signal(SIGCHLD, sig_chld);
    Signal(SIGTERM, sig_term);

    printf("OPEN_MAX is %d\n", OPEN_MAX);
    client[0].fd = listenfd;
    client[0].events = POLLRDNORM;
    maxi = 0;
    for (i=1; i < OPEN_MAX; i++)
        client[i].fd = -1;

    for ( ; ; ) {
        // rset = allset;
        nready = poll(client, maxi + 1, INFTIM);

        if (client[0].revents & POLLRDNORM){
            clilen = sizeof(cliaddr);
            connfd = accept(listenfd, (SA *) &cliaddr, &clilen);
            printf("connect from %s, port %d\n",
                inet_ntop(AF_INET, &cliaddr.sin_addr, buff, sizeof(buff)),
                ntohs(cliaddr.sin_port));

            for (i = 1; i < OPEN_MAX; i++)
                if (client[i].fd < 0) {
                    client[i].fd = connfd;
                    break;
                }
            printf("put to client ok\n");
            if (i == OPEN_MAX) {
                printf("too many clients");
                exit(1);
            }
            client[i].events = POLLRDNORM;
            printf("set sonnfd events ok: %d\n",connfd);
            if (i > maxi)
                maxi = i;
            if (--nready <= 0)
                continue;
        }

        for (i = 1; i<= maxi; i++) {
            if ( (sockfd = client[i].fd) < 0)
                continue;
            if (client[i].revents & (POLLRDNORM | POLLERR)) {
                if ( (n = read(sockfd, buf, MAXLINE)) == 0) {
                    close(sockfd);
                    client[i].fd = -1;
                } else if (n < 0){
                    if (errno == ECONNRESET){
                        close(sockfd);
                        client[i].fd = -1;
                    } else{
                        printf("read error\n");
                        exit(1);
                    }
                } else {
                    if (getpeername(sockfd, (SA *) &ss, &len) < 0) {
                        printf("get peer name error\n");
                        exit(1);
                    }
                    printf("sent date to %s, port %d\n",
                        inet_ntop(AF_INET, &ss.sin_addr, buff, sizeof(buff)),
                        ntohs(ss.sin_port));
                    writen(sockfd, buf, n);
                }
                if (--nready <=0)
                    break;
            }
        }
    }
}

