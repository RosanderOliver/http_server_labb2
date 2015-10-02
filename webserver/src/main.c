#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <limits.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <time.h>

#include "configparser.h"
#include "setpageinfo.h"

#define SERVICE "3490"
#define BACKLOG 10
#define BUFLEN 1024
#define MAXLEN 1

#define HTTPVER "1.0"
#define SERVER "VOhoo 1.0"
#define CONFPATH "/home/vph/http_server_labb2/webserver/src/.lab3-config"

#define DIE(str) {perror(str); exit(1);}

struct requestparams {
        char *method;
        char *path;
};

int send_all(int sockfd, void *msg, size_t len, int flag)
{
        char *ptr = (char*) msg;
        while (len > 0) {
                int sent = send(sockfd, ptr, len, flag);
                if (sent < 1)
                        return 0;
                ptr += sent;
                len -= sent;
        }
        // error buffer full
        return -1;
}

int recv_all(int sockfd, void *buf, size_t len, int flag)
{
        char *ptr = (char*) buf;
        int rx = 0;
        while (len > 0) {
                if (len < BUFLEN-5 && strncmp(ptr-4, "\r\n\r\n", 4) == 0)
                        return 0;
                if ((rx = recv(sockfd, ptr, len, 0)) == -1)
                        return -1;
                ptr += rx;
                len -= rx;
        }
        // set error msg buffer full
        return -1;
}

int validate(char *path, char **filename)
{
        // undersök om fil är utanför rot, ex /../../
        // return 403

        if (path == NULL)
                return 400;
        else if (strcmp(path, "/") == 0) {
                *filename = strdup("/index.html");
                return 200;
        } else if ((*filename = realpath(path, NULL)) == NULL) // om NULL ist för tmp, dynamisk
                return 404;

        return 200;
}

int set_msg(int sockfd, int status_code, struct requestparams *rqp)
{
        struct pageinfo pi;
        char *header = NULL;
        int in_fd = -1;
        struct stat sb;

        if (status_code == 200)
                status_code = validate(rqp->path, &pi.filename);

        if (setpageinfo(status_code, &pi) != 0)
                fprintf(stderr, "setpageinfo: Not an error code! SNH-FL");

        if ((in_fd = open(pi.filename, O_RDONLY)) == -1) {
                syslog(LOG_WARNING, "Error in set_msg: open()");
                free(pi.filename);
                free(pi.status);
                return set_msg(sockfd, 500, NULL);
        }

        fstat(in_fd, &sb); // TODO: check om lyckas

        //time_t t = time(NULL);
        //struct tm tm = *localtime(&t);

        char *date = "September";


        //  TODO: Last-Modifed.
        /*
        char date[20];
        strftime(date, 20, "%d-%m-%y", localtime(&(sb.st_ctime)));

        printf("date %s\n", date);
        */

        asprintf(&header,
            "HTTP/"HTTPVER" %s\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: %d\r\n"
            "Date: %s\r\n"
            "Server: "SERVER"\r\n\r\n",
            pi.status, sb.st_size, date);

        // TODO: Fel formatkod för stat_buf.st_size.

        // sendall
        if (send(sockfd, header, strlen(header), 0) == -1) {
                syslog(LOG_WARNING, "Error in set_msg: send()");
                close(in_fd);
                free(header);
                free(pi.filename);
                free(pi.status);
                return -1;
        }

        //off_t offset = 0;
        if (!(strcmp(rqp->method, "HEAD") == 0)) {
                if (sendfile(sockfd, in_fd, 0, sb.st_size) == -1) {
                        syslog(LOG_WARNING, "Error in set_msg: sendfile()");
                        close(in_fd);
                        free(header);
                        free(pi.filename);
                        free(pi.status);
                        return -1;
                }
        }

        close(in_fd);
        free(header);
        free(pi.filename);
        free(pi.status);
        return 0;
}

int accept_request(int sockfd)
{
        char buf[BUFLEN];
        char *token = NULL;

        if (recv_all(sockfd, buf, BUFLEN-1, 0) != 0)
                DIE("recvall");

        // en vanlig recv, sedan en recv all?

        if ((token = strtok(buf, " ")) == NULL)
                return set_msg(sockfd, 400, NULL);

        if (strcmp(token, "GET") == 0|| strcmp(token, "HEAD") == 0) {

                struct requestparams rqp;
                rqp.method = strdup(token);
                token = strtok(NULL, " ");
                rqp.path = strdup(token);

                int rsp = set_msg(sockfd, 200, &rqp);
                
                free(rqp.method);
                free(rqp.path);

                return rsp;

        } else if (strcmp(token, "POST") == 0) {
                return set_msg(sockfd, 501, NULL);
        }

        return set_msg(sockfd, 400, NULL);
}

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

int main(int argc, char *argv[])
{
        char *port = NULL, *path = NULL;
        handling_type handling;

        int runasd = 0;
        FILE *logfile = NULL;

	pid_t pid;
	struct sockaddr_storage their_addr;
        socklen_t addr_size;
        struct addrinfo hints, *res;
        int sockfd, new_fd, status;
        char s[INET6_ADDRSTRLEN];

        if (configparser(&port, &handling, &path) != 0)
                exit(EXIT_FAILURE);

        printf("port %s\n", port);

        // TODO: Argumenthantering i egen funktion?

        int c;
        while ((c = getopt(argc, argv, ":hp:dl:s:")) != -1) {

                 char *logpath = NULL;
                 long int pn;

                 switch (c) {
                 case 'h':
                         // displayhelp
                         break;
                 case 'p':
                         if ((pn = strtol(optarg, NULL, 10)) > 1024 
                             && pn < 6400) {
                                 port = optarg;
                         } else {
                                 fprintf(stderr, "Invalid port!\n");
                                 exit(EXIT_FAILURE);
                         }
                         break;
                 case 'd':
                         runasd = 1;
                         break;
                 case 'l':
                         if ((logfile = fopen(optarg, "a+")) == NULL)
                                 DIE("fopen");
                         break;
                 case 's':
                         if (strcmp(optarg, "fork") == 0) {
                                 handling = FORK;
                         } else if (strcmp(optarg, "mux") == 0) {
                                 handling = MUX;
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
                         exit(EXIT_FAILURE);
                 }
        }


	char *logName="/home/vph/http_server_labb2/webserver/src/web.log";
	FILE *logFile=fopen(logName, "a+");	
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

        while(1) {
	        fseek(logFile, 0, SEEK_END);	//Sätter filpekaren till slutet av filen pga fork()?
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
			exit(1);
		}

        }

        freeaddrinfo(res);
        close(sockfd);
	closelog();
	fclose(logFile);
        return 0;
}
