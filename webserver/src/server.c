
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <limits.h>

#define SERVICE "3490"
#define BACKLOG 10
#define BUFLEN 1024
#define MAXLEN 1
#define HTTPVER "1.0"
#define SERVER "VOhoo 1.0"

struct valid
{
        char method[256];
        char path[256];
};

typedef struct valid Valid;

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
        while (len > 0) {
                int rec = recv(sockfd, ptr, len, 0);
                if (strncmp(ptr, "\r\n", 2) == 0)
                        return 0;
                ptr += rec;
                len -= rec;
        }
        // error buffer full
        return -1;
}

int validate(char *path, char *filename)
{
        // undersök om fil är utanför rot, ex /../../
        // return 403

        char *tmp;

        if (strcmp(path, "/") == 0) {
                strcpy(filename, "/index.html");
                return 200;
        } else if (realpath(path, tmp) == NULL) {
                perror("realpath");
                return 404;
        }

        strcpy(filename, tmp);
        return 200;
}

int set_msg(int sockfd, int status_code, Valid *bob)
{
        char buf[BUFLEN];
        char status[50];
        char filename[250];
        int in_fd;
        struct stat stat_buf;

        if (status_code == 200) {
                status_code = validate(bob->path, filename);
        }

        switch(status_code) {
        case 500:
                strcpy(filename, "500_internal_server_error.html");
                strcpy(status, "500 Internal Server Error");
                break;
        case 200:
                strcpy(status, "200 OK");
                break;
        case 400:
                strcpy(filename, "400_bad_request.html");
                strcpy(status, "400 Bad Request");
                break;
        case 403:
                strcpy(filename, "403_forbidden.html");
                strcpy(status, "403 Forbidden");
                break;
        case 404:
                strcpy(filename, "404_not_found.html");
                strcpy(status, "404 Not Found");
                break;
        case 501:
                strcpy(filename, "501_not_implemented.html");
                strcpy(status, "501 Not Implemented");
                break;
        default:
                fprintf(stderr, "bad_request: Not an error code! SNH-FL");
                free(bob);
                return 1;
        }

        if ((in_fd = open(filename, O_RDONLY)) == -1) {
                perror("open");
                free(bob);
                return set_msg(sockfd, 500, NULL);
        }

        fstat(in_fd, &stat_buf);

        //time_t t = time(NULL);
        //struct tm tm = *localtime(&t);

        char *date = "September";

        snprintf(buf, sizeof(buf),
            "HTTP/"HTTPVER" %s\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: %d\r\n"
            "Date: %s\r\n"
            "Server: "SERVER"\r\n\r\n",
            status, stat_buf.st_size, date);

        // sendall
        if (send(sockfd, buf, strlen(buf), 0) == -1) {
                perror("sendall");
                close(in_fd);
                free(bob);
                return -1;
        }

        //off_t offset = 0;
        if (!strcmp(bob->method, "HEAD") == 0) {
                if (sendfile(sockfd, in_fd, 0, stat_buf.st_size) == -1) {
                        perror("sendfile");
                        close(in_fd);
                        free(bob);
                        return -1;
                }
        }
        free(bob);
        close(in_fd);
        return 0;
}

int accept_request(int sockfd)
{
        char buf[BUFLEN];
        char *token;

        const char *delim = " ";

        if (recv_all(sockfd, buf, BUFLEN-1, 0) != 0) {
               perror("recv");       
               // error av något slag, int ist void?
        }

        if ((token = strtok(buf, delim)) == NULL) {
                perror("strtok");
                return set_msg(sockfd, 400, NULL);
        }

        if (strcmp(token, "GET") == 0 || strcmp(token, "HEAD") == 0) {
                Valid *bob = malloc(sizeof(Valid));
                strcpy(bob->method, token);

                if ((token = strtok(NULL, " ")) == NULL) {
                        return set_msg(sockfd, 400, NULL);
                }

                strcpy(bob->path, token);
                return set_msg(sockfd, 200, bob);
        } else if (strcmp(token, "POST") == 0)
                return set_msg(sockfd, 501, NULL);

        return set_msg(sockfd, 400, NULL);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
        if (sa->sa_family == AF_INET)
                return &(((struct sockaddr_in*)sa)->sin_addr);

        return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void main(void)
{
        struct sockaddr_storage their_addr;
        socklen_t addr_size;
        struct addrinfo hints, *res;
        int sockfd, new_fd, status;
        pid_t pid;
        char s[INET6_ADDRSTRLEN];


        // setuid? Till root om under 1024

        if (chdir("../../www") != 0) {
                perror("chdir");
                exit(1);
        }

        if (chroot("./") != 0) {
                perror("chroot");
                exit(1);
        }

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        if ((status = getaddrinfo(NULL, SERVICE, &hints, &res)) != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
                exit(1);
        }

        // fram till hit pga SERVICE

        if ((sockfd = socket(res->ai_family, res->ai_socktype, 
            res->ai_protocol)) == -1) {
                perror("socket");
                exit(1);
        }

        if (bind(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
                close(sockfd);
                perror("bind");
                exit(1);
        }

        if (listen(sockfd, BACKLOG) != 0) {
                close(sockfd);
                perror("listen");
                exit(1);
        }

        while(1) {
                addr_size = sizeof(their_addr);    
                if ((new_fd = accept(sockfd, (struct sockaddr *) &their_addr, 
                    &addr_size)) == -1) {
                        perror("accept");
                        continue;
                }

                inet_ntop(their_addr.ss_family, 
                    get_in_addr((struct sockaddr *) &their_addr), s, sizeof(s));

                printf("Server: got connection from %s\n", s);

                if ((pid = fork()) == -1) {
                        perror("fork");
                        set_msg(new_fd, 500, NULL);
                } else if (pid == 0) {
                        close(sockfd);
                        accept_request(new_fd);
                        close(new_fd);
                        exit(0);
                }
                close(new_fd);
        }
        
        return;
}
