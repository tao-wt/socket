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
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include "signal.c"

#define	max(a,b) ((a) > (b) ? (a) : (b))
#define	SA struct sockaddr
#define	LISTENQ	1024 /* 2nd argument to listen() */
#define	SERV_PORT 9877 /* TCP and UDP */
#define	MAXLINE	4096 /* max text line length */

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
    int    udpfd, nready, maxfdp1, listenfd, connfd;
    char   mesg[MAXLINE];
    pid_t  childpid;
    fd_set rset;
    ssize_t n;
    socklen_t    len; /* socket length */
    const int on = 0;
    struct sockaddr_in    cliaddr, servaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(SERV_PORT);

    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    bind(listenfd, (SA *) &servaddr, sizeof(servaddr));

    listen(listenfd, LISTENQ);

    udpfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    bind(udpfd, (SA *)&servaddr, sizeof(servaddr));

    Signal(SIGCHLD, sig_chld);
    Signal(SIGTERM, sig_term);


    FD_ZERO(&rset);
    maxfdp1 = max(listenfd, udpfd) + 1;
    for ( ; ; ) {
        FD_SET(listenfd, &rset);
        FD_SET(udpfd, &rset);
        if ( (nready = select(maxfdp1, &rset, NULL, NULL, NULL)) < 0){
            if (errno == EINTR)
                continue;
            else{
                printf("select error\n");
                exit(1);
            }
        }

        if (FD_ISSET(listenfd, &rset)) {
        len = sizeof(cliaddr);
        if ( (connfd = accept(listenfd, (SA *) &cliaddr, &len)) < 0) {
            if (errno == EINTR)
                continue;
            else
                printf("server accept error!\n");
        }

        if ( (childpid = fork()) == 0) {    /* child process */
            close(listenfd);    /* close listening socket */
            str_echo(connfd);    /* process the request */
            exit(0);
        }
        close(connfd);            /* parent closes connected socket */
        }

        if (FD_ISSET(udpfd, &rset)){
            len = sizeof(cliaddr);
            n = recvfrom(udpfd, mesg, MAXLINE, 0, (SA *)&cliaddr, &len);

            sendto(udpfd, mesg, n, 0, (SA *) &cliaddr, len);
        }
    }
}

