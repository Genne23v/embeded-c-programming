#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

//func to process threads
static void *clnt_connection(void *arg);
int sendData(FILE *fp, char *ct, char *filename);
void sendOk(FILE *fp);
void sendError(FILE *fp);

int main(int argc, char** argv){
	int ssock;
	pthread_t thread;
	struct sockaddr_in servaddr, cliaddr;
	unsigned int len;
	
	//get a port# for server at program initiation 
	if(argc != 2){
		printf("usage : %s <port>\n", argv[0]);
		return -1;
	}
	
	//create a socket for server
	ssock = socket(AF_INET, SOCK_STREAM, 0);
	if(ssock == -1){
		perror("socket()");
		return -1;
	}
	
	//register service in ops using input port #
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = (argc != 2)?htons(8000):htons(atoi(argv[1]));
	if(bind(ssock, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
		perror("bind()");
		return -1;
	}
	//create a queue to process max 10 clients simultaneously
	if(listen(ssock, 10) == -1){
		perror("listen()");
		return -1;
	}
	while(1){
		char mesg[BUFSIZ];
		int csock;
		//wait for client request
		len = sizeof(cliaddr);
		csock = accept(ssock, (struct sockaddr*)&cliaddr, &len);
		//change network address to string
		inet_ntop(AF_INET, &cliaddr.sin_addr, mesg, BUFSIZ);
		printf("Client IP : %s:%d\n", mesg, ntohs(cliaddr.sin_port));
		
		//create a thread when client requests and process the request
		pthread_create(&thread, NULL, clnt_connection, &csock);
		//pthread_join(thread, NULL);
	}
	return 0;
}
void *clnt_connection(void *arg){
	//convert arg received via thread to int type fd
	int csock = *((int*)arg);
	FILE *clnt_read, *clnt_write;
	char reg_line[BUFSIZ], reg_buf[BUFSIZ];
	char method[BUFSIZ], type[BUFSIZ];
	char filename[BUFSIZ], *ret;
	
	//convert fd into file stream
	clnt_read = fdopen(csock, "r");
	clnt_write = fdopen(dup(csock), "w");
	
	//read one line of string and store to reg_line
	fgets(reg_line, BUFSIZ, clnt_read);
	//print reg_line string on screen
	fputs(reg_line, stdout);
	//
	ret = strtok(reg_line, "/ ");
	strcpy(method, (ret != NULL)?ret:"");
	if(strcmp(method, "POST") == 0){		//for POST method
		sendOk(clnt_write);		//send ok to client
		goto END;
	}else if(strcmp(method, "GET") != 0){		//if not GET method
		sendError(clnt_write);
		goto END;
	}
	ret = strtok(NULL, " ");		//get a path from request line
	strcpy(filename, (ret != NULL)?ret:"");
	if(filename[0] == '/'){			//if path starts with /, remove it
		for(int i=0, j=0; i< BUFSIZ; i++, j++){
			if(filename[0] == '/') j++;
			filename[i] = filename[j];
			if(filename[j] == '\0') break;
		}
	}
	//read msg header and print on screen and ignore the other
	do{
		fgets(reg_line, BUFSIZ, clnt_read);
		fputs(reg_line, stdout);
		strcpy(reg_buf, reg_line);
		char* type = strchr(reg_buf, ':');
	}while(strncmp(reg_line, "\r\n", 2));		//request header ends with \r\n
	
	//send file contents to client using file's name
	sendData(clnt_write, type, filename);
	
	END:
	fclose(clnt_read);		//close file's stream
	fclose(clnt_write);
	
	pthread_exit(0);
	
	return (void*)NULL;
}
int sendData(FILE* fp, char *ct, char *filename){
	//response msg for successful delivery to client
	char protocol[] = "HTTP/1.1 200 OK\r\n";
	char server[] = "Server:Netscape-Enterprise/6.0\r\n";
	char cnt_type[] = "Content-Type:text/html\r\n";
	char end[] = "\r\n";
	char buf[BUFSIZ];
	int fd, len;
	
	fputs(protocol, fp);
	fputs(server, fp);
	fputs(cnt_type, fp);
	fputs(end, fp);
	
	fd = open(filename, O_RDWR);		//open a file
	do{
		len = read(fd, buf, BUFSIZ);
		fputs(buf, fp);
	}while(len == BUFSIZ);
	
	close(fd);
	return 0;
}
	void sendOk(FILE *fp){
		//http response msg for success to client
		char protocol[] = "HTTP/1.1 200 Ok\r\n";
		char server[] = "Server: Netscape-Enterprise/6.0\r\n\r\n";
		fputs(protocol, fp);
		fputs(server, fp);
		fflush(fp);
	}
	void sendError(FILE *fp){
		char protocol[] = "HTTP/1.1 400 Bad Request\r\n";
		char server[] = "Server: Netscape-Enterprise/6.0\r\n";
		char cnt_len[] = "Content-Length:1024\r\n";
		char cnt_type[] = "Content-Type:text/html\r\n\r\n";
		
		//html content to display
		char content1[] = "<html><head><title>BAD Connection</title></head>";
		char content2[] = "<body><font size=+5>Bad Request</font></body></html>";
		
		printf("send_error\n");
		fputs(protocol, fp);
		fputs(server, fp);
		fputs(cnt_len, fp);
		fputs(cnt_type, fp);
		fputs(content1, fp);
		fputs(content2, fp);
		fflush(fp);
	}
	
