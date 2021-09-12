#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define TCP_PORT 5100

int main(int argc, char** argv){
	int ssock;
	socklen_t clen;		//socket descript definition
	int n;
	struct sockaddr_in servaddr, cliaddr;		//address struct
	char mesg[BUFSIZ];
	//create a server socket
	if((ssock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket()");
		return -1;
	}
	//Appoint address to address struct
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(TCP_PORT);
	
	//set up a socket addres using bind func
	if(bind(ssock, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
		perror("bind()");
		return -1;
	}
	//set up ready queue to process clients accessing at same time
	if(listen(ssock, 8) < 0){
		perror("listen()");
		return -1;
	}
	clen = sizeof(cliaddr);
	do{
		//allow client's access and create a socket for client 	
	int n, csock = accept(ssock, (struct sockaddr*)&cliaddr, &clen);
	
	//change network address to string
	inet_ntop(AF_INET, &cliaddr.sin_addr, mesg, BUFSIZ);
	printf("Client is connected : %s\n", mesg);
	
	if((n = read(csock, mesg, BUFSIZ)) <= 0)
	perror("read()");
	
	printf("Received data : %s", mesg);
	
	//send string from 'buf' to client
	if(write(csock, mesg, n) <= 0)
	perror("write()");
	
	close(csock);
}while(strncmp(mesg, "q", 1));

close(ssock);

return 0;
}
