
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

#include "setpageinfo.h"
#include "netio.h"

int send_all(int sockfd, char *msg, size_t len, int flag)
{
        char *ptr = msg;
        while (len > 0) {
                int sent = send(sockfd, ptr, len, flag);
                if (sent < 1)
                        return 0;
                ptr += sent;
                len -= sent;
        }
        // error buffer full
        return -1;
}

int recv_all(int sockfd, char *buf, size_t len, int flag)
{
        char *ptr = buf;
        int rx = 0;
        while (len > 0) {
                if (len < BUFLEN-5 && strncmp(ptr-4, "\r\n\r\n", 4) == 0)
                        return 0;
                if ((rx = recv(sockfd, ptr, len, 0)) == -1)
                        return -1;
                ptr += rx;
                len -= rx;
        }
        // set error msg buffer full
        return -1;
}

int validate(char *path, char **filename)
{

        // TODO: "URL validation"
        // undersök om fil är utanför rot, ex /../../
        // return 403

        if (path == NULL)
                return BAD_REQUEST;
        else if (strcmp(path, "/") == 0) {
                *filename = strdup("/index.html");
                return OK;
        } else if ((*filename = realpath(path, NULL)) == NULL)
                return NOT_FOUND;

        return OK;
}

int set_msg(int sockfd, int status_code, struct requestparams *rqp)
{
        struct pageinfo pi;
        char *header = NULL;

        int in_fd = -1;
        struct stat sb;

        int res = 0;

        if (status_code == OK)
                status_code = validate(rqp->path, &pi.filename);

        if (setpageinfo(status_code, &pi) != 0)
                fprintf(stderr, "setpageinfo: Not an error code! SNH-FL");

        if ((in_fd = open(pi.filename, O_RDONLY)) == -1) {
                syslog(LOG_WARNING, "Error in set_msg: open()");

                // prevents potential code 500 loops
                res = status_code != INTERNAL_SERVER_ERROR
                    ? set_msg(sockfd, INTERNAL_SERVER_ERROR, NULL)
                    : 1;

                goto cleanup1;
        }

        if (fstat(in_fd, &sb) != 0) {
                // log

                // prevents potential code 500 loops
                res = status_code != INTERNAL_SERVER_ERROR
                    ? set_msg(sockfd, INTERNAL_SERVER_ERROR, NULL)
                    : 1;

                goto cleanup2;
        }

        //time_t t = time(NULL);
        //struct tm tm = *localtime(&t);

        char *date = "September";


        //  TODO: Last-Modifed.
        /*
        char date[20];
        strftime(date, 20, "%d-%m-%y", localtime(&(sb.st_ctime)));

        printf("date %s\n", date);
        */

        /* TODO:  "implicit declaration of asprint"
        http://forums.devshed.com/programming-42/implicit-declaration-function-asprintf-283557.html
        */
        asprintf(&header,
            "HTTP/"HTTPVER" %s\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: %d\r\n"
            "Date: %s\r\n"
            "Server: "SERVER"\r\n\r\n",
            pi.status, sb.st_size, date);

        // TODO: Fel formatkod för stat_buf.st_size.

        // sendall
        if (send(sockfd, header, strlen(header), 0) == -1) {
                syslog(LOG_WARNING, "Error in set_msg: send()");
                res = 1;
                goto cleanup3;
        }

        //off_t offset = 0;
        if (!(strcmp(rqp->method, "HEAD") == 0)) {
                if (sendfile(sockfd, in_fd, 0, sb.st_size) == -1) {
                        syslog(LOG_WARNING, "Error in set_msg: sendfile()");
                        res = 1;
                        goto cleanup3;
                }
        }

cleanup3:
        free(header);
cleanup2:
        close(in_fd);
cleanup1:
        free(pi.filename);
        free(pi.status);

        return res;
}

int accept_request(int sockfd)
{
        char buf[BUFLEN];
        char *token = NULL;

        // referens?
        if (recv_all(sockfd, buf, BUFLEN-1, 0) != 0) {
                //DIE("recvall");
        }

        // en vanlig recv, sedan en recv all?

        if ((token = strtok(buf, " ")) == NULL)
                return set_msg(sockfd, 400, NULL);

        if (strcmp(token, "GET") == 0|| strcmp(token, "HEAD") == 0) {

                struct requestparams rqp;
                rqp.method = strdup(token);
                token = strtok(NULL, " ");
                rqp.path = strdup(token);

                // den andra recv skulle i så fall vara här

                int rsp = set_msg(sockfd, 200, &rqp);
                
                free(rqp.method);
                free(rqp.path);

                return rsp;

        } else if (strcmp(token, "POST") == 0) {
                return set_msg(sockfd, 501, NULL);
        }

        return set_msg(sockfd, 400, NULL);
}

