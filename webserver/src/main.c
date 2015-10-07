
// TODO: Rensa.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <signal.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <limits.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>

// waitpid
#include <sys/types.h>
#include <sys/wait.h>

// isprint
#include <ctype.h>

// umask
#include <sys/types.h>
#include <sys/stat.h>

#include "logger.h"
#include "configparser.h"
#include "netio.h"

#include <errno.h> // test 

#define SERVICE "3490"
#define BACKLOG 10

#define DIE(str) {perror(str); exit(1);}

// define DIE(str) {logerror(str); exit(1);}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
        if (sa->sa_family == AF_INET)
                return &(((struct sockaddr_in*)sa)->sin_addr);

        return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void handle_sigchld(int sig)
{
/*
-1 innebär att den väntar på alla pids, WNOHANG sätter inte processen i 
väntan. waitpid() returnerar 0 om ingen blivit terminerad. 
*/
        while(waitpid(-1, 0, WNOHANG) > 0) {
        }
}

void usage(void) {

// TODO: Skriv trevlig.

}

int arghandler(int *argc, char **argv[], char **port, int *runasd, 
    handling_type *handling)
{
        int c;
        while ((c = getopt(*argc, *argv, ":hp:dl:s:")) != -1) {

                 long int pn;

                 switch (c) {
                 case 'h':
                         usage();
                         return 1;
                 case 'p':
                         if ((pn = strtol(optarg, NULL, 10)) > 1024 
                             && pn < 6400) {
                                 *port = optarg;
                         } else {
                                 fprintf(stderr, "Invalid port!\n");
                                 exit(EXIT_FAILURE);
                         }
                         break;
                 case 'd':
                         *runasd = 1;
                         break;
                 case 'l':
                         {
                                 uselogf = 1;
                                 if ((actlog = fopen(optarg, "a+")) == NULL)
                                         DIE("fopen");

                                 char *fn = malloc(strlen(optarg + 5));
                                 sprintf(fn, "%s.err", optarg);

                                 if ((errlog = fopen(fn, "a+")) == NULL) {
                                         free(fn);
                                         DIE("fopen");
                                 }

                                 free(fn);
                         }
                         break;
                 case 's':
                         if (strcmp(optarg, "fork") == 0) {
                                 *handling = FORK;
                         } else if (strcmp(optarg, "mux") == 0) {
                                 *handling = MUX;
                         } else {
                                 fprintf(stderr, "Not a method!\n");
                                 exit(EXIT_FAILURE);
                         }
                         break;
                 case '?':
                         if (optopt == 's' || optopt == 'l' || optopt == 's') // fungerar ej
                                 fprintf (stderr, 
                                     "Option -%c requires an argument.\n", 
                                      optopt);
                         else if (isprint (optopt))
                                 fprintf (stderr, "Unknown option `-%c'.\n", 
                                      optopt);
                         else
                                 fprintf (stderr,
                                      "Unknown option character `\\x%x'.\n",
                                      optopt);
                 default:
                         return 1;
                 }
        }

        return 0;
}

void demonize()
{
        pid_t pid;

        if ((pid=fork()) < 0)
                exit(EXIT_FAILURE);
        else if (pid > 0)
                exit(EXIT_SUCCESS);

        umask(0);

        if (setsid() < 0)
                exit(EXIT_FAILURE);

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
}

int main(int argc, char *argv[])
{
        char *port = NULL, *path = NULL;
        handling_type handling;

        uselogf = 0;
        int runasd = 0;

        int sockfd = -1, new_fd = -1;

        struct addrinfo hints, *res;
        char s[INET6_ADDRSTRLEN];
        
        pid_t pid;
        struct sockaddr_storage their_addr;
        socklen_t addr_size;

        if (configparser(&port, &handling, &path) != 0)
                DIE("arghandler");

        printf("port %s\n", port);

        if (arghandler(&argc, &argv, &port, &runasd, &handling) != 0)
                DIE("arghandler");

        //openlog("Webserver", LOG_PID, LOG_USER); //LOG_LRP

        if (runasd) {
                printf("Starting daemon...\n");
                demonize();
        }

        // TODO: setuid?

        if (chdir(path) != 0)
                DIE("chdir");

        if (path)
                free(path);

        if (chroot("./") != 0) // en chroot i demonize, sen en för path?
                DIE("chroot");
        
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        int status;
        if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0) {
                herror(gai_strerror(status));
                exit(EXIT_FAILURE);
        }

        if (port)
                free(port);

        if ((sockfd = socket(res->ai_family, res->ai_socktype, 
            res->ai_protocol)) == -1)
                DIE("socket");

        if (bind(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
                close(sockfd);
                DIE("bind");
        }

        if (listen(sockfd, BACKLOG) != 0) {
                close(sockfd);
                DIE("listen");
        }

        // Kan kanske göras i argumenthanteraren.
        //if (uselogf)
        //        fseek(logfile, 0, SEEK_END); //Sätter filpekaren till slutet av filen pga fork()?

        while(1) {

                addr_size = sizeof(their_addr);    
                if ((new_fd = accept(sockfd, (struct sockaddr *) &their_addr, 
                    &addr_size)) == -1) {
                        perror("accept");
                        continue;
                }

                if ((pid = fork()) == -1) {
                        //log_err("monster", LOG_CRIT);
                        syslog(LOG_ERR, "fork() error in request handling");
                } else if (pid == 0) {
                        close(sockfd);
                        freeaddrinfo(res);

                        struct loginfo li;

                        inet_ntop(their_addr.ss_family, 
                            get_in_addr((struct sockaddr *) &their_addr), 
                            s, sizeof(s));

                        li.ipaddr = strdup(s);

                        if (serve(new_fd, &li) != 0) {
                                // cleanup
                                exit(EXIT_FAILURE);
                        }

                        log_act(&li);

                        if (li.ipaddr)        free(li.ipaddr);
                        if (li.status_line) free(li.status_line);

                        fclose(errlog);
                        fclose(actlog);
                        closelog();
                        close(new_fd);
                        exit(EXIT_SUCCESS);
                }
                close(new_fd);

                if (signal(SIGCHLD, SIG_IGN) == SIG_ERR){
                        syslog(LOG_ERR, "SIG_ERR in request handling");
                        exit(EXIT_FAILURE);
                }

        }

        freeaddrinfo(res);
        close(sockfd);
        closelog();
        //fclose(logfile);
        return 0;
}
