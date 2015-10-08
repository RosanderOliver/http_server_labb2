
#ifndef LOGGERR_H
#define LOGGERR_H

#include <stdio.h>
#include <stdint.h>

int uselogf;
FILE *actlog;
FILE *errlog;

struct loginfo {
        char *ipaddr;
        char *uid_cl;
        char *uid;
        char *date;
        char *status_line;
        int code;
        intmax_t sz;
};

void log_act(struct loginfo *li);
void log_err(char *src, char *errmsg, char *errparam, int priority);

#endif
