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

typedef	void Sigfunc(int); /* for signal handlers */

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

Sigfunc *
signal(int signo, Sigfunc *func)
{
    struct sigaction	act, oact;

    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (signo == SIGALRM) {
#ifdef	SA_INTERRUPT
        act.sa_flags |= SA_INTERRUPT;	/* SunOS 4.x */
#endif
    } else {
#ifdef	SA_RESTART
        act.sa_flags |= SA_RESTART;		/* SVR4, 44BSD */
#endif
    }
    if (sigaction(signo, &act, &oact) < 0)
        return(SIG_ERR);
    return(oact.sa_handler);
}
/* end signal */

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

    signal(SIGCHLD, sig_chld);

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

