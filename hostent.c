#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif


int
main(int argc,char **argv)
{
    char *ptr, **pptr;
    char str[INET_ADDRSTRLEN];
    struct hostent *hptr;

    while(--argc > 0){
        ptr = *++argv;
        if ( (hptr = gethostbyname(ptr)) == NULL) {
            printf("gethostname error for host %s:%s\n", ptr, hstrerror(h_errno));
            continue;
        }
        printf("official hostname: %s\n", hptr->h_name);

        for (pptr = hptr->h_aliases; *pptr != NULL; pptr++)
            printf("\talias: %s\n", *pptr);

        switch(hptr->h_addrtype) {
            case AF_INET:
                pptr = hptr->h_addr_list;
                for (; *pptr != NULL; pptr++)
                    printf("\taddress: %s\n",
                        inet_ntop(hptr->h_addrtype, *pptr, str, sizeof(str)));
                break;

            default:
                printf("unknown address type\n");
                break;
        }
    }
    exit(0);
}
