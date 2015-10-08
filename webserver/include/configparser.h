#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

typedef enum {FORK, MUX} handling_type;

int configparser(char **port, handling_type *handling, char **path);

#endif
