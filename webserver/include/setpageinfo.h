#ifndef PAGEINFO_H
#define PAGEINFO_H

struct pageinfo {
        char *filename;
        char *status;
};

int setpageinfo(int status_code, struct pageinfo *pi);

#endif

