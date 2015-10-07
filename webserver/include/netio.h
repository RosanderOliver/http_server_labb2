
#ifndef NETIO_H
#define NETIO_H

#define HTTPVER "1.0"
#define SERVER "VOhoo/1.0"

#define OK 200
#define BAD_REQUEST 400
#define NOT_FOUND 404
#define INTERNAL_SERVER_ERROR 500
#define NOT_IMPLEMENTED 501

#define BUFLEN 1024

typedef enum {STATUS_LINE, HEADER} part;

struct requestparams {
        char *method;
        char *path;
        char *httpver;
};

struct response {
        int stsc;
        char *tx_stsl;
        int in_fd;
        intmax_t sz; 
};

int send_all(int sockfd, char *msg, size_t len, int flag);
int recv_all(int sockfd, char *buf, size_t len, int flag, part pt);

int interpret_stsl(char *rxheader, struct requestparams *rqp);
int set_msg(struct response *rsp, char *path);
int serve(int sockfd, struct loginfo *li);

#endif
