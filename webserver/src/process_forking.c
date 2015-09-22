#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void){
	int ret_val=0;
	bool server_is_active=true;
	pid_t	pid; 

	/*if((pid=fork())<0) {
		printf("fork error");
		exit(0);
	}
	else if(pid == 0){
		printf("success\n");
		execl("/home/tigerhund/Desktop/LAB_2/http_server_labb2/webserver/src", "Forkexec", (char *) 0);
		sleep(10);		
	} 
	
	if(waitpid(pid, NULL, 0) != pid){ 
		printf("waitpid error"); 
                       exit(0);
	}
	*/

	ret_val=execlp("/home/tigerhund/Desktop/LAB_2/http_server_labb2/webserver/src/Forkexec", "/home/tigerhund/Desktop/LAB_2/http_server_labb2/webserver/src/Forkexec", (char *) 0);
	
	if(ret_val==-1){
		printf("Fail");
	}
	
	exit(0);

}


