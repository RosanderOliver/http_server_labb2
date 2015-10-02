#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <limits.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <time.h>

#include "configparser.h"

#define SERVICE "3490"
#define BACKLOG 10

#define DIE(str) {perror(str); exit(1);}

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

// debugfunktion
void printall(char *ptr)
{
        char *p = ptr;
        while (*p != '\0') {
                printf("%03d %c\n", *p, *p);
                p++;
        }
        printf("\n\n");
}

void displayhelp() {



}

int arghandler(int *argc, char **argv[], char **port, int *uselogf, 
    FILE **logfile, int *runasd, handling_type *handling)
{
        int c;
        while ((c = getopt(*argc, *argv, ":hp:dl:s:")) != -1) {

                 char *logpath = NULL;
                 long int pn;

                 switch (c) {
                 case 'h':
                         displayhelp();
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
                         if ((*logfile = fopen(optarg, "a+")) == NULL)
                                 DIE("fopen");
                         *uselogf = 1;
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

int main(int argc, char *argv[])
{
        char *port = NULL, *path = NULL;
        handling_type handling;

        int runasd = 0, uselogf = 0;
        FILE *logfile = NULL;

	pid_t pid;
	struct sockaddr_storage their_addr;
        socklen_t addr_size;
        struct addrinfo hints, *res;
        int sockfd, new_fd, status;
        char s[INET6_ADDRSTRLEN];

        // TODO: Gör felmeddelandena kompatibla med perror()?

        if (configparser(&port, &handling, &path) != 0)
                exit(EXIT_FAILURE);

        printf("port %s\n", port);

        if (arghandler(&argc, &argv, &port, &uselogf, &logfile, &runasd, &handling) != 0)
                exit(EXIT_FAILURE);

	openlog("Webserver", LOG_PID, LOG_USER); //LOG_LRP

        // TODO: Egen funktion?
        if (runasd) {
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

        // setuid? Till root om under 1024

        if (chdir(path) != 0)
                DIE("chdir");

        if (path)
                free(path);

        if (chroot("./") != 0)
                DIE("chroot");
        
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0) {
                herror(gai_strerror(status));
                exit(EXIT_FAILURE);
        }

        if (port)
                free(port);

        // fram till hit pga SERVICE

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

        if (uselogf)
                fseek(logfile, 0, SEEK_END); //Sätter filpekaren till slutet av filen pga fork()?

        while(1) {
                addr_size = sizeof(their_addr);    
                if ((new_fd = accept(sockfd, (struct sockaddr *) &their_addr, 
                    &addr_size)) == -1) {
                        perror("accept");
                        continue;
                }

                // TODO: Ta bort om det inte behövs i loggningen.
                inet_ntop(their_addr.ss_family, 
                    get_in_addr((struct sockaddr *) &their_addr), s, sizeof(s));
	
                if ((pid = fork()) == -1) {
                        syslog(LOG_ERR, "fork() error in request handling");
                        set_msg(new_fd, 500, NULL);
                } else if (pid == 0) {
                        close(sockfd);
                        freeaddrinfo(res);
                        accept_request(new_fd);
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
	fclose(logfile);
        return 0;
}
