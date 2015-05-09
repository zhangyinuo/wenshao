#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char**argv)
{
	int sockfd,n;
	struct sockaddr_in servaddr,cliaddr;
	char sendline[1000];
	char recvline[1000];

	if (argc != 2)
	{
		printf("usage:  udpcli <IP address>\n");
		exit(1);
	}

	sockfd=socket(AF_INET,SOCK_DGRAM,0);

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr=inet_addr(argv[1]);
	servaddr.sin_port=htons(1234);

	while (fgets(sendline, 64,stdin) != NULL)
	{
		sendto(sockfd,sendline,strlen(sendline),0,
				(struct sockaddr *)&servaddr,sizeof(servaddr));
		n=recvfrom(sockfd,recvline,64,0,NULL,NULL);
		recvline[n]=0;
		fputs(recvline,stdout);
	}
}
