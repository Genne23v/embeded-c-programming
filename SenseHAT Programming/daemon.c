#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/resource.h>

int main(int argc, char** argv){
	struct sigaction sa;
	struct rlimit rl;
	int fd0, fd1, fd2, i;
	pid_t pid;
	
	if(argc < 2){
		printf("Usage : %s command\n", argv[0]);
		return -1;
	}
	//mast set to 0 to create a file
	umask(0);
	
	//max fd# that can be used
	if(getrlimit(RLIMIT_NOFILE, &rl) < 0){
		perror("getlimit()");
	}
	if((pid = fork()) < 0){
		perror("error()");
	}else if(pid != 0){
		return 0;
	}
	
	setsid();		//become a session leader to disable terminal control
	
	//ignore signal for terminal control 
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if(sigaction(SIGHUP, &sa, NULL) < 0){
		perror("sigaction() : Can't ignore SIGHUP");
	}
	//set process's working directory to '/'
	if(chdir("/") < 0){
		perror("cd()");
	}
	if(rl.rlim_max == RLIM_INFINITY){
		rl.rlim_max = 1024;
	}
	for(i=0; i<rl.rlim_max; i++){
		close(i);
	}
	
	//connect fd 0, 1 and 2 with /dev/null
	fd0 = open("/dev/null", O_RDWR);
	fd1 = dup(0);
	fd2 = dup(0);
	
	//open a file log to print log
	openlog(argv[1], LOG_CONS, LOG_DAEMON);
	if(fd0 != 0 || fd1 != 1 || fd2 != 2){
		syslog(LOG_ERR, "unexpected file descriptors %d %d %d", fd0, fd1, fd2);
		return -1;
	}
	//print log in log file
	syslog(LOG_INFO, "Daemon Process");
	while(1){
		//repeat job to do with daemon process
	}
		closelog();
		return 0;
	}
