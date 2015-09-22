
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MYPORT "3490"  // the port users will be connecting to
#define BACKLOG 10     // how many pending connections queue will hold

void error(const char *msg)
{
        fprintf(stderr, msg);
        exit(1);
}

int main(void)
{
        struct sockaddr_storage their_addr;
        socklen_t addr_size;
        struct addrinfo hints, *res;
        int sockfd, new_fd, status, len, bytes_sent;
        char *msg = "Viktor was here!";

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        if ((status = getaddrinfo(NULL, MYPORT, &hints, &res)) != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
                return 1;
        }

        if ((sockfd = 
            socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
                error("Error opening socket!\n");
        }

        if (bind(sockfd, res->ai_addr, res->ai_addrlen) != 0)
                error("Bind failed!\n");

        if (listen(sockfd, BACKLOG) != 0)
                error("Listen failed!\n");

        addr_size = sizeof their_addr;
        if ((new_fd = 
            accept(sockfd, (struct sockaddr *)&their_addr, &addr_size)) == -1) {
                error("Accept failed!\n");
        }

        bytes_sent = send(new_fd, msg, strlen(msg), 0);
        close(new_fd);

        return 0;
}
