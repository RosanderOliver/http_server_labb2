
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

#define SERVICE "3490"
#define BACKLOG 10
#define BUFLEN 1024
#define MAXLEN 1

#define HTTPVER "1.0"
#define SERVER "VOhoo 1.0"
#define CONFPATH "/home/vph/http_server_labb2/webserver/src/.lab3-config"

#define DIE(str) {perror(str); exit(1);}

typedef enum {FORK, MUX} handling_type;

struct valid
{
        char *method;
        char *path;
};

// TODO: Ta bort?
typedef struct valid Valid;


/* TODO

This does not apply if only one branch of a conditional statement is a single
statement; in the latter case use braces in both branches:

	if (condition) {
		do_this();
		do_that();
	} else {
		otherwise();
	}
*/

// TODO: Fixa denna - lägg till båda s/r-all i eget bibliotek
// (möjligtvis all processkod - getopt, daemon, log, uppstart tar plats)
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

int set_msg(int sockfd, int status_code, Valid *bob)
{
        char *header = NULL, *status = NULL, *filename = NULL;
        int in_fd = -1;
        struct stat stat_buf;

        if (status_code == 200)
                status_code = validate(bob->path, &filename);

        switch(status_code) {
        case 500:
                filename = strdup("500_internal_server_error.html");
                status = strdup("500 Internal Server Error");
                break;
        case 200:
                status = strdup("200 OK");
                break;
        case 400:
                filename = strdup("400_bad_request.html");
                status = strdup("400 Bad Request");
                break;
        case 403:
                filename = strdup("403_forbidden.html");
                status = strdup("403 Forbidden");
                break;
        case 404:
                filename = strdup("404_not_found.html");
                status = strdup("404 Not Found");
                break;
        case 501:
                filename = strdup("501_not_implemented.html");
                status = strdup("501 Not Implemented");
                break;
        default:
                fprintf(stderr, "bad_request: Not an error code! SNH-FL");
                free(bob);
                return 1;
        }

        if ((in_fd = open(filename, O_RDONLY)) == -1) {
                perror("open");
                free(bob);
                free(filename);
                free(status);
                return set_msg(sockfd, 500, NULL);
        }

        fstat(in_fd, &stat_buf);

        //time_t t = time(NULL);
        //struct tm tm = *localtime(&t);

        char *date = "September";

        // lägg till last modified (ist för date)

        asprintf(&header,
            "HTTP/"HTTPVER" %s\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: %d\r\n"
            "Date: %s\r\n"
            "Server: "SERVER"\r\n\r\n",
            status, stat_buf.st_size, date);

        // sendall
        if (send(sockfd, header, strlen(header), 0) == -1) {
                perror("sendall");
                close(in_fd);
                free(header);
                free(bob->method); free(bob->path); // ta hand om i acceptreq
                free(filename);
                free(status);
                return -1;
        }

        //off_t offset = 0;
        if (!(strcmp(bob->method, "HEAD") == 0)) {
                if (sendfile(sockfd, in_fd, 0, stat_buf.st_size) == -1) {
                        perror("sendfile");
                        close(in_fd);
                        free(header);
                        free(bob->method); free(bob->path);
                        free(filename);
                        free(status);
                        return -1;
                }
        }

        free(header);
        free(bob->method); free(bob->path);
        free(filename);
        free(status);
        close(in_fd);
        return 0;
}

int accept_request(int sockfd)
{
        char buf[BUFLEN];
        char *token;

        if (recv_all(sockfd, buf, BUFLEN-1, 0) != 0)
                DIE("recvall");

        // en vanlig recv, sedan en recv all?

        if ((token = strtok(buf, " ")) == NULL)
                return set_msg(sockfd, 400, NULL);

        if (strcmp(token, "GET") == 0|| strcmp(token, "HEAD") == 0) {

                // TODO: Bättra namn än "Valid" och "bob".
                Valid bob;
                bob.method = strdup(token);
                token = strtok(NULL, " ");
                bob.path = strdup(token);

                return set_msg(sockfd, 200, &bob);

                // ta hand om bob här ist

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

void handle_sigchld(int sig){
	while(waitpid(-1, 0, WNOHANG) > 0){}	
// -1 innebär att den väntar på alla pids, WNOHANG sätter inte processen i 
// väntan. waitpid() returnerar 0 om ingen blivit terminerad. 
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

int set_conf(char **port, handling_type *handling, char **path) {
        
        FILE *fp = NULL;

        char *line = NULL;
        size_t len = 0;
        ssize_t read = -1;

        if ((fp = fopen("/home/vph/http_server_labb2/webserver/src/.lab3-config", "r")) == NULL) {
                perror("open");
                return 1;
        }
        
        while ((read = getline(&line, &len, fp)) != -1) {

                // for free to work line must point to the first location,
                // therefore we use another pointer for manipulation
                char *beg = line;

                while (isblank(*beg))
                        beg++;

                if (*beg == '\n')
                        continue;

                // removes trailing white-space, including newline
                char *end = beg + strlen(beg) - 1;
                while(end > beg && isspace(*end)) 
                        end--;
                *(end+1) = '\0';

                char *s = beg;
                
                if ((s = strchr(beg, ' ')) != NULL)
                        *s = '\0';
                ++s;          

                if (strcmp(beg, "path") == 0) {
                        while (isspace(*s))
                                ++s;
                        s = strtok(s, "\"");
                        if ((*path = realpath(s, NULL)) == NULL) {
                                perror("realpath");
                                return 1;
                        }
                } else if (strcmp(beg, "port") == 0) {
                        while (isspace(*s))
                                ++s;
                        long int r;
                        if ((r = strtol(s, NULL, 10)) > 1024 && r < 6400) {
                                 *port = strdup(s);
                        } else {
                                 fprintf(stderr, "Invalid aoeport!\n");
                                 return 1;
                        }
                                
                } else if (strcmp(beg, "handling") == 0) {
                        while (isspace(*s))
                                ++s;
                        s = strtok(s, "\"");
                        if (strcmp(s, "fork") == 0)
                                *handling = FORK;
                        else if (strcmp(s, "mux") == 0)
                                *handling = MUX;
                        else {
                                fprintf(stderr, "Invalid hand in conf-file!\n");
                                return 1;
                        }
                } else {
                        fprintf(stderr, 
                            "Unknown option in the configuration file: %s",
                             beg);
                        return 1;
                }
        }

        fclose(fp);
        if (line) free(line);
        return 0;
}

void main(int argc, char* argv[])
{
        char *port = NULL;
        handling_type handling;
        char *path = NULL;

        int runasd = 0;
        FILE *logfile = NULL;

	pid_t pid;
	struct sockaddr_storage their_addr;
        socklen_t addr_size;
        struct addrinfo hints, *res;
        int sockfd, new_fd, status;
        
        char s[INET6_ADDRSTRLEN];

        if (set_conf(&port, &handling, &path) != 0)
                exit(EXIT_FAILURE);

        printf("port %s\n", port);

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

        if (runasd) {
	        if ((pid=fork()) < 0)
	                exit(EXIT_FAILURE);
	        else if (pid > 0)
	                exit(EXIT_SUCCESS);

	        umask(0);

	//Impliment logging here!

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

        // freeaddrinfo(res); <--- OBS krävs någonstans för parent, 
        // skriv sighandler?

        if (listen(sockfd, BACKLOG) != 0) {
                close(sockfd);
                DIE("listen");
        }

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

                printf("Server: got connection from %s\n", s);

                if ((pid = fork()) == -1) {
                        perror("fork");
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
			perror(0);
			exit(1);
		}

        }

        freeaddrinfo(res);
        close(sockfd);
        return;
}
