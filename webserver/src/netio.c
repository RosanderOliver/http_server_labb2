
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h> // intmax_t

#include "logger.h"
#include "setpageinfo.h"
#include "netio.h"

#include <errno.h> // test 

#define LASTMODLEN 35

int send_all(int sockfd, char *msg, size_t len, int flag)
{
        char *ptr = msg;
        while (len > 0) {
                int sent = send(sockfd, ptr, len, flag);
                if (sent < 1)
                        return -1;
                ptr += sent;
                len -= sent;
        }
        return 0;
}

// could be made more general with end string and multiplier args, 
// recv args could be struct
int recv_all(int sockfd, char *buf, size_t len, int flag, part pt)
{

        char *ptr = buf;
        int rx = 0;

        char *apa = pt == STATUS_LINE ? "\r\n" : "\r\n\r\n";

        int overflow = 0;
        int recvend = 0;

        while (!overflow && !recvend) {
                if ((rx = recv(sockfd, ptr, len, 0)) == -1)
                        return -1;

                if (!(overflow = (len -= rx) < 0)) {
                        ptr += rx;
                        *ptr = '\0';
                        recvend = strstr(buf, apa) != NULL 
                            || strcmp(buf, "\r\n") == 0;
                }
        }
        // set error msg buffer full

        return overflow;
}


int set_msg(struct response *rsp, char *path)
{
        int res = 0;

        struct pageinfo pi;

        struct stat sb;
        char last_modifed[LASTMODLEN];

        if (path)
                pi.filename = strdup(path);

        if (setpageinfo(rsp->stsc, &pi) != 0) {
                fprintf(stderr, "setpageinfo: Not an error code! SNH-FL");
                //res = 1;
                //goto cleanup0;
                return 1;
        }

        if ((rsp->in_fd = open(pi.filename, O_RDONLY)) == -1) {
                syslog(LOG_WARNING, " ");

                if (rsp->stsc == INTERNAL_SERVER_ERROR) {
                        res = 1;
                } else {
                        rsp->stsc = INTERNAL_SERVER_ERROR;
                        set_msg(rsp, NULL);
                }   

                goto cleanup;
        }

        if (fstat(rsp->in_fd, &sb) != 0) {
                syslog(LOG_WARNING, " ");

                if (rsp->stsc == INTERNAL_SERVER_ERROR) {
                        res = 1;
                } else {
                        rsp->stsc = INTERNAL_SERVER_ERROR;
                        set_msg(rsp, NULL);
                }

                goto cleanup;
        }

        rsp->sz = sb.st_size;

        strftime(last_modifed, LASTMODLEN, "%a, %d %b %Y %H:%M:%S %Z",
            localtime(&(sb.st_ctime)));

        // warning implicit
        asprintf(&rsp->tx_stsl,
            "HTTP/"HTTPVER" %s\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: %jd\r\n"
            "Last-Modified: %s\r\n"
            "Server: "SERVER"\r\n\r\n",
            pi.status, rsp->sz, last_modifed);

cleanup:
        if (pi.filename)    free(pi.filename);
        if (pi.status)      free(pi.status);

        return res;
}

int interpret_stsl(char *rxheader, struct requestparams *rqp)
{
        char *token = NULL;

        rqp->method = (token = strtok(rxheader, " ")) != NULL
            ? strdup(token)
            : NULL;

        rqp->path = (token = strtok(NULL, " ")) != NULL
            ? strdup(token)
            : NULL;

        rqp->httpver = (token = strtok(NULL, " ")) != NULL
            ? strdup(token)
            : NULL;

        if (strcmp(rqp->method, "POST") == 0) {
                return NOT_IMPLEMENTED;
        } else if (!(strcmp(rqp->method, "GET") == 0 || 
            strcmp(rqp->method, "HEAD") == 0)) {
                return BAD_REQUEST;
        }

        if (rqp->path == NULL)
                return BAD_REQUEST;
        else if (strcmp(rqp->path, "/") == 0) 
                rqp->path = strdup("/index.html");
        else if ((rqp->path = realpath(rqp->path, NULL)) == NULL)
                return NOT_FOUND;

        // TODO: Tolka HTTP version

        return OK; 
}

int serve(int sockfd, struct loginfo *li)
{
        int res = 1;

        char buf[BUFLEN];
        part pt;

        char *ptr = NULL;
        int all_in_one = 0;

        char *stsl = NULL, *header = NULL;

        struct requestparams rqp;

        struct response rsp;

        pt = STATUS_LINE;
        if (recv_all(sockfd, buf, BUFLEN - 1, 0, pt) != 0) {
                syslog(LOG_WARNING, " ");
                return 1;
        }

        if ((ptr = strstr(buf, "\r\n")) != NULL) {
                if (strstr(ptr, "\r\n\r\n") != NULL) {
                        all_in_one = 1;
                        header = strdup(ptr + 2);
                }
                *ptr = '\0';
                stsl = strdup(buf);
                li->status_line = strdup(stsl);
        }

        rsp.stsc = interpret_stsl(stsl, &rqp);

        if (!all_in_one && rsp.stsc != BAD_REQUEST) {
                pt = HEADER;
                if (recv_all(sockfd, buf, BUFLEN - 1, 0, pt) != 0) {
                        log_err("recv_all()", strerror(errno), NULL, LOG_CRIT);
                        goto cleanup;
                }
                header = strdup(buf);
        }

        if (set_msg(&rsp, rqp.path) != 0) {
                syslog(LOG_WARNING, " ");
                goto cleanup;
        }

        if (send_all(sockfd, rsp.tx_stsl, strlen(rsp.tx_stsl), 0) == -1) {
                syslog(LOG_WARNING, " ");
                goto cleanup;
        }

        if (strcmp(rqp.method, "HEAD") != 0) {
                if (sendfile(sockfd, rsp.in_fd, 0, rsp.sz) == -1) {
                        syslog(LOG_WARNING, " ");
                        goto cleanup;
                }
        }

        li->sz = rsp.sz;
        li->code = rsp.stsc;
        res = 0;

cleanup:
        if (stsl)         free(stsl);
        if (header)       free(header);
        if (rqp.method)   free(rqp.method);
        if (rqp.path)     free(rqp.path);
        if (rqp.httpver)  free(rqp.httpver);
        if (rsp.tx_stsl)  free(rsp.tx_stsl);
        close(rsp.in_fd);

        return res;
}

