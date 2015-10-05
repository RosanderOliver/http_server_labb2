
#ifndef LOGGERR_H
#define LOGGERR_H

#include <stdint.h>

int uselogf;
char *actlog_path;
char *errlog_path;

struct loginfo {
        char *ipaddr;
        char *uid_cl;
        char *uid;
        char *date;
        char *header;
        int code;
        intmax_t sz;
};

void log_act(struct loginfo *li);
void log_err(char *msg, int priority);

#endif
