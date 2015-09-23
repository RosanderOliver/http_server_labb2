
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

#define MYPORT "3490"  // the port users will be connecting to
#define BACKLOG 10     // how many pending connections queue will hold
#define BUFLEN 1024


int send_all(int sockfd, void *msg, size_t len, int flag)
{
        char *ptr = (char*) msg;
        while (len > 0) {
                int rec = send(sockfd, ptr, len, flag);
                printf("Sent: %d.\n", rec);
                if (rec < 1)
                        return 0;
                ptr += rec;
                len -= rec;
        }

        return -1;
}

void get_request(int sockfd, char *token)
{
        int in_fd;
        char buf[BUFLEN];
        char *filename;
        struct stat stat_buf;      /* argument to fstat */

        if ((token = strtok(NULL, " ")) == NULL)
                perror("token 2");
        
        if ((filename = realpath(token, buf)))
                printf("filename: %s\n", filename);
        else {
                perror("realpath");
                // code 4xx
        }

        if ((in_fd = open(filename, O_RDONLY)) == -1)
                perror("open");

        fstat(in_fd, &stat_buf);

        char *bestmsg = "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n\r\n";

        off_t offset = 0;
        int sender = send(sockfd, bestmsg, strlen(bestmsg), 0);
        int rc = sendfile(sockfd, in_fd, &offset, stat_buf.st_size);

        close(in_fd);
}

void head_request(int sockfd, char *token)
{
        int a = 1;
}

void accept_request(int sockfd)
{
        char buf[BUFLEN];

        if (recv(sockfd, buf, BUFLEN-1, 0) < 1)
               perror("recv");

        const char *delim = " ";
        char *token;

        if ((token = strtok(buf, delim)) == NULL)
                perror("arg");

        if (strcmp(token, "GET") == 0) {
                get_request(sockfd, token);
        } else if (strcmp(token, "HEAD") == 0) {
                head_request(sockfd, token);
        } else
                perror("not a method");
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{

        struct sockaddr_storage their_addr;
        socklen_t addr_size;
        struct addrinfo hints, *res;
        int sockfd, new_fd, status, len, bytes_sent;
        char s[INET6_ADDRSTRLEN];

        if (chdir("../../www") != 0)
                perror("chdir");

        if (chroot("../../www") != 0)
                perror("chroot");

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        if ((status = getaddrinfo(NULL, MYPORT, &hints, &res)) != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
                return 1;
        }

        if ((sockfd = socket(res->ai_family, res->ai_socktype, 
            res->ai_protocol)) == -1) {
                perror("socket");
        }

        if (bind(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
                close(sockfd);
                perror("bind");
        }

        if (listen(sockfd, BACKLOG) != 0)
                perror("listen");

        while(1) {
                addr_size = sizeof their_addr;    
                if ((new_fd = accept(sockfd, (struct sockaddr *) &their_addr, 
                    &addr_size)) == -1) {
                        perror("accept");
                        continue;
                }

                inet_ntop(their_addr.ss_family, 
                    get_in_addr((struct sockaddr *) &their_addr), s, sizeof s);

                printf("Server: got connection from %s\n", s);

                if (fork() == 0) { // this is the child process
                        close(sockfd); // child doesn't need the listener
                        accept_request(new_fd);
                        close(new_fd);
                        exit(0);
                }
                close(new_fd);  // parent doesn't need this
        }
        
        return 0;
}
