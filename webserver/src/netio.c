
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



#include "setpageinfo.h"
#include "netio.h"

int send_all(int sockfd, void *msg, size_t len, int flag)
{
        char *ptr = (char*) msg;
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

int recv_all(int sockfd, void *buf, size_t len, int flag)
{
        char *ptr = (char*) buf;
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
        // undersök om fil är utanför rot, ex /../../
        // return 403

        if (path == NULL)
                return 400;
        else if (strcmp(path, "/") == 0) {
                *filename = strdup("/index.html");
                return 200;
        } else if ((*filename = realpath(path, NULL)) == NULL) // om NULL ist för tmp, dynamisk
                return 404;

        return 200;
}

int set_msg(int sockfd, int status_code, struct requestparams *rqp)
{
        struct pageinfo pi;
        char *header = NULL;
        int in_fd = -1;
        struct stat sb;

        if (status_code == 200)
                status_code = validate(rqp->path, &pi.filename);

        if (setpageinfo(status_code, &pi) != 0)
                fprintf(stderr, "setpageinfo: Not an error code! SNH-FL");

        if ((in_fd = open(pi.filename, O_RDONLY)) == -1) {
                syslog(LOG_WARNING, "Error in set_msg: open()");
                free(pi.filename);
                free(pi.status);
                return set_msg(sockfd, 500, NULL);
        }

        fstat(in_fd, &sb); // TODO: check om lyckas

        //time_t t = time(NULL);
        //struct tm tm = *localtime(&t);

        char *date = "September";


        //  TODO: Last-Modifed.
        /*
        char date[20];
        strftime(date, 20, "%d-%m-%y", localtime(&(sb.st_ctime)));

        printf("date %s\n", date);
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
                close(in_fd);
                free(header);
                free(pi.filename);
                free(pi.status);
                return -1;
        }

        //off_t offset = 0;
        if (!(strcmp(rqp->method, "HEAD") == 0)) {
                if (sendfile(sockfd, in_fd, 0, sb.st_size) == -1) {
                        syslog(LOG_WARNING, "Error in set_msg: sendfile()");
                        close(in_fd);
                        free(header);
                        free(pi.filename);
                        free(pi.status);
                        return -1;
                }
        }

        close(in_fd);
        free(header);
        free(pi.filename);
        free(pi.status);
        return 0;
}

int accept_request(int sockfd)
{
        char buf[BUFLEN];
        char *token = NULL;

        if (recv_all(sockfd, buf, BUFLEN-1, 0) != 0) {
                //DIE("recvall");
                int a = 12;
        }

        // en vanlig recv, sedan en recv all?

        if ((token = strtok(buf, " ")) == NULL)
                return set_msg(sockfd, 400, NULL);

        if (strcmp(token, "GET") == 0|| strcmp(token, "HEAD") == 0) {

                struct requestparams rqp;
                rqp.method = strdup(token);
                token = strtok(NULL, " ");
                rqp.path = strdup(token);

                int rsp = set_msg(sockfd, 200, &rqp);
                
                free(rqp.method);
                free(rqp.path);

                return rsp;

        } else if (strcmp(token, "POST") == 0) {
                return set_msg(sockfd, 501, NULL);
        }

        return set_msg(sockfd, 400, NULL);
}

