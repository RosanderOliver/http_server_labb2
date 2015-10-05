
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <time.h>

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
                strftime(fmt, sizeof fmt, "%d/%b/%Y:%H:%M:%S %z", tm);

        char *start = li->header;
        char *end = start + strlen(start) - 1;
        while(end > start && isspace(*end)) 
                end--;
        *(end+1) = '\0';

        asprintf(&str, "%s - "SERVER" [%s] \"%s\" %d %jd\n", 
            li->ipaddr, fmt, li->header, li->code, li->sz);

        printf("%s\n", str);

        if (uselogf) {
                FILE *fp;
                if ((fp = fopen("loggen.log", "a")) != NULL) {
                        fprintf(fp, "%s\n", str);
                        fclose(fp);
                }
        }

        syslog(LOG_INFO, "%s", str);

        free(str);
}

void log_err(char *msg, int priority)
{
        if (uselogf) {
                FILE *fp;
                if ((fp = fopen(errlog_path, "a")) != NULL) {
                        fprintf(fp, "%s\n", msg);
                        fclose(fp);
                }
        }

        syslog(priority, msg);
}
