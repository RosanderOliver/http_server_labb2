#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#define CONFPATH "/home/vph/http_server_labb2/webserver/src/.lab3-config"

typedef enum {FORK, MUX} handling_type;

int configparser(char **port, handling_type *handling, char **path);

#endif
