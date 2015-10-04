
#ifndef LOGGERR_H
#define LOGGERR_H

// 127.0.0.1 user-identifier frank [10/Oct/2000:13:55:36 -0700] "GET /apache_pb.gif HTTP/1.0" 200 2326

struct loginfo {
        char *ipaddr;
        char *uid_cl;
        char *uid;
        char *date;
        char *header;
        int code;
        intmax_t sz;
};


#endif
