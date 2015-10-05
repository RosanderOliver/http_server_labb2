
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// isspace, isblank
#include <ctype.h>


#include "configparser.h"

int configparser(char **port, handling_type *handling, char **path) {

        FILE *fp = NULL;

        char *line = NULL;
        size_t len = 0;
        ssize_t read = -1;

        // TODO: Klara av ~ (tilde) i sökvägen.
        if ((fp = fopen(CONFPATH, "r")) == NULL) {
                perror("open");
                return 1;
        }
        
        while ((read = getline(&line, &len, fp)) != -1) {

                // for free to work line must point to the first location,
                // therefore we use another pointer for manipulation
                char *start = line;

                while (isblank(*start))
                        start++;

                if (*start == '\n')
                        continue;

                // removes trailing white-space, including newline
                char *end = start + strlen(start) - 1;
                while(end > start && isspace(*end)) 
                        end--;
                *(end+1) = '\0';

                char *optval = start;
                
                if ((optval = strchr(start, ' ')) != NULL)
                        *optval = '\0';
                ++optval;          

                if (strcmp(start, "path") == 0) {
                        while (isspace(*optval))
                                ++optval;
                        optval = strtok(optval, "\"");
                        if ((*path = realpath(optval, NULL)) == NULL) {
                                perror("realpath");
                                return 1;
                        }
                } else if (strcmp(start, "port") == 0) {
                        while (isspace(*optval))
                                ++optval;
                        long int r;
                        if ((r = strtol(optval, NULL, 10)) > 1024 && r < 6400) {
                                 *port = strdup(optval);
                        } else {
                                 fprintf(stderr, "Invalid port!\n");
                                 return 1;
                        }
                                
                } else if (strcmp(start, "handling") == 0) {
                        while (isspace(*optval))
                                ++optval;
                        optval = strtok(optval, "\"");
                        if (strcmp(optval, "fork") == 0) {
                                *handling = FORK;
                        } else if (strcmp(optval, "mux") == 0) {
                                *handling = MUX;
                        } else {
                                fprintf(stderr, "Invalid hand in conf-file!\n");
                                return 1;
                        }
                } else {
                        fprintf(stderr, 
                            "Unknown option in the configuration file: %s",
                             start);
                        return 1;
                }
        }

        fclose(fp);
        if (line) free(line);
        return 0;
}
