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
#include "signal.c"

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
    long        arg1, arg2;
    ssize_t     n;
    char        line[MAXLINE];

    for ( ; ; ) {
        if ( (n = readline(sockfd, line, MAXLINE)) == 0)
            return;        /* connection closed by other end */

        if (sscanf(line, "%ld%ld", &arg1, &arg2) == 2)
            snprintf(line, sizeof(line), "%ld\n", arg1 + arg2);
        else
            snprintf(line, sizeof(line), "input error\n");

        n = strlen(line);
        writen(sockfd, line, n);
    }
}

int
main(int argc, char **argv)
{
    int                    listenfd, connfd;
    pid_t                childpid;
    socklen_t            clilen; /* socket length */
    struct sockaddr_in    cliaddr, servaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(SERV_PORT);

    bind(listenfd, (SA *) &servaddr, sizeof(servaddr));

    listen(listenfd, LISTENQ);

    Signal(SIGCHLD, sig_chld);
    Signal(SIGTERM, sig_term);

    for ( ; ; ) {
        clilen = sizeof(cliaddr);
        if ( (connfd = accept(listenfd, (SA *) &cliaddr, &clilen)) < 0) {
            if (errno == EINTR)
                continue;
            else
                printf("server accept error!");
        }

        if ( (childpid = fork()) == 0) {    /* child process */
            close(listenfd);    /* close listening socket */
            str_echo(connfd);    /* process the request */
            exit(0);
        }
        close(connfd);            /* parent closes connected socket */
    }
}

