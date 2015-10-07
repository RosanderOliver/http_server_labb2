
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/time.h>

#include "logger.h"
#include "netio.h" // SERVER var

void log_act(struct loginfo *li)
{
        char fmt[64];
        struct timeval tv;
        struct tm *tm;

        char *str = NULL;

        gettimeofday(&tv, NULL);
        if((tm = localtime(&tv.tv_sec)) != NULL)
                strftime(fmt, sizeof(fmt), "%d/%b/%Y:%H:%M:%S %z", tm);

        asprintf(&str, "%s - "SERVER" [%s] \"%s\" %d %jd", 
            li->ipaddr, fmt, li->status_line, li->code, li->sz);

        if (uselogf)
                fprintf(actlog, "%s\n", str);

        syslog(LOG_INFO, "%s", str);

        free(str);
}

void log_err(char *src, char *errmsg, char *errparam, int priority)
{
        if (uselogf) {
                //err() { echo "[$(date +'%Y-%m-%dT%H:%M:%S%z')]: $@" >&2; usage; }

                char fmt[64];
                struct timeval tv;
                struct tm *tm;

                gettimeofday(&tv, NULL);
                if((tm = localtime(&tv.tv_sec)) != NULL)
                    strftime(fmt, sizeof(fmt), "%Y-%m-%d %H:%S%z", tm);

                fprintf(errlog, "[%s] %s: %s \"%s\"\n", fmt, src, errmsg, errparam);
        }

        syslog(priority, errmsg);
}
