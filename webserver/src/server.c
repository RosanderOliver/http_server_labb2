
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

int send_all(int sockfd, void *msg, size_t len, int flag)
{
        char *ptr = (char*) msg;
        while (len > 0) {
                int rec = send(sockfd, ptr, len, flag);
                //printf("Sent: %d.\n", rec);
                if (rec < 1)
                        return 0;
                ptr += rec;
                len -= rec;
        }

        return -1;
}

int bad(const int sockfd, const int status_code)
{
        char *msg_400 = "HTTP/1.1 400 Bad Request\r\n\r\n";
        char *msg_403 = "HTTP/1.1 403 Forbidden\r\n\r\n";
        char *msg_404 = "HTTP/1.1 404 Not Found\r\n\r\n";
        char *msg_500 = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
        char *msg_501 = "HTTP/1.1 501 Not Implemnted\r\n\r\n";

        char *filename, *msg;
        int in_fd;
        struct stat stat_buf;

        switch(status_code) {
        case 500:
                filename = "500_internal_server_error.html";
                msg = msg_500;
                break;
        case 400:
                filename = "400_bad_request.html";
                msg = msg_400;
                break;
        case 403:
                filename = "403_forbidden.html";
                msg = msg_403;
                break;
        case 404:
                filename = "404_not_found.html";
                msg = msg_404;
                break;
        case 501:
                filename = "501_not_implemented.html";
                msg = msg_501;
                break;
        default:
                fprintf(stderr, "bad_request: Not an error code! SNH-FL");
                return 1;
        }

        if ((in_fd = open(filename, O_RDONLY)) == -1) {
                perror("open");
                return bad(sockfd, 500);
        }

        fstat(in_fd, &stat_buf);

        //off_t offset = 0;
        // sendall
        if (send(sockfd, msg, strlen(msg), 0) == -1) {
                perror("sendall");
                close(in_fd);
                return -1;
        }
        if (sendfile(sockfd, in_fd, 0, stat_buf.st_size) == -1) {
                perror("sendfile");
                close(in_fd);
                return -1;
        }

        close(in_fd);
        return 0;
}

int valid_method(int sockfd, char *token)
{
        char *msg_200 = "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n\r\n";
        int in_fd;
        char buf[BUFLEN];
        char *filename;
        struct stat stat_buf;

        char *method = token;

        if ((token = strtok(NULL, " ")) == NULL) {
                perror("token 2");
                bad(sockfd, 400);
                return -1;
        }

        // undersök om fil är utanför rot, ex /../../

        if (strcmp(token, "/") == 0)
                filename = "/index.html";
        else if ((filename = realpath(token, buf)) == NULL) {
                perror("realpath");
                return bad(sockfd, 404);
        }

        if ((in_fd = open(filename, O_RDONLY)) == -1) {
                perror("open");
                return bad(sockfd, 500);
        }

        fstat(in_fd, &stat_buf);
        //int *fsize = stat_buf.st_size;
        //off_t offset = 0;

        // sendall
        if (send(sockfd, msg_200, strlen(msg_200), 0) == -1) {
                perror("send");
                close(in_fd);
                return -1;
        }

        if (strcmp(method, "GET") == 0) {
                if (sendfile(sockfd, in_fd, 0, stat_buf.st_size) == -1) {
                        perror("sendfile");
                        close(in_fd);
                        return -1;
                }
        }

        close(in_fd);
        return 0;
}

int accept_request(int sockfd)
{
        char buf[BUFLEN];

        if (recv(sockfd, buf, BUFLEN-1, 0) < 1) {
               perror("recv");
               // error av något slag, int ist void?
        }

        const char *delim = " ";
        char *token;

        if ((token = strtok(buf, delim)) == NULL) {
                perror("strtok");
                return bad(sockfd, 400);
        }

        if (strcmp(token, "GET") == 0 || strcmp(token, "HEAD") == 0)
                valid_method(sockfd, token);
        else if (strcmp(token, "POST") == 0)
                return bad(sockfd, 501);
        else
                return bad(sockfd, 400);

        return 0;
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
                        bad(new_fd, 500);
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
