
#ifndef NETIO_H
#define NETIO_H

#define HTTPVER "1.0"
#define SERVER "VOhoo 1.0"

#define OK 200
#define BAD_REQUEST 400
#define NOT_FOUND 404
#define INTERNAL_SERVER_ERROR 500

#define BUFLEN 1024

struct requestparams {
        char *method;
        char *path;
};

int send_all(int sockfd, char *msg, size_t len, int flag);
int recv_all(int sockfd, char *buf, size_t len, int flag);

int set_msg(int sockfd, int status_code, struct requestparams *rqp);
int accept_request(int sockfd);

#endif
