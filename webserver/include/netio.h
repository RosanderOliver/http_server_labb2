
#ifndef NETIO_H
#define NETIO_H

#define HTTPVER "1.0"
#define SERVER "VOhoo 1.0"

#define BUFLEN 1024

struct requestparams {
        char *method;
        char *path;
};

int send_all(int sockfd, void *msg, size_t len, int flag);
int recv_all(int sockfd, void *buf, size_t len, int flag);

int set_msg(int sockfd, int status_code, struct requestparams *rqp);
int accept_request(int sockfd);

#endif
