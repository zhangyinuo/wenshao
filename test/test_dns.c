#include <stdio.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h> 
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

char *dict_host[] = {"mail", "ftp", "www", "smtp", "news", "noexist", "110", "aq"};

char *dict_port[] = {"80", "21", "25", "8080", "22"};

int get_ip_by_domain(char *serverip, char *domain)
{
	char **pptr;
	char                    str[128] = {0x0};
	struct hostent  *hptr;
	if ( (hptr = gethostbyname(domain)) == NULL) 
		return -1;

	switch (hptr->h_addrtype) {
		case AF_INET:
#ifdef  AF_INET6
		case AF_INET6:
#endif
			pptr = hptr->h_addr_list;
			for ( ; *pptr != NULL; pptr++)
			{
				inet_ntop(hptr->h_addrtype, *pptr, str, sizeof(str));
				strcpy(serverip, str);
				return 0;
			}
			break;

		default:
			return -1;
			break;
	}
	return -1;
}

int createsocket(char *ip, int port)
{
	int					sockfd;
	struct sockaddr_in	servaddr;

	struct timeval timeo = {5, 0};
	socklen_t len = sizeof(timeo);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		return -1;
	}
	setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeo, len);
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &servaddr.sin_addr);

	int rc = connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	if (rc)
	{
		close(sockfd);
		return -1;
	}
	return sockfd;
}

int scan_port(char *domain, char *ip)
{
	int c = sizeof(dict_port)/sizeof(char*);

	int i = 0;
	for (; i < c; i++)
	{
		int cfd = createsocket(ip, atoi(dict_port[i]));
		if (cfd > 0)
		{
			fprintf(stdout, "[%s] [%s] port [%s] open\n", domain, ip, dict_port[i]);
			close(cfd);
		}
		else
			fprintf(stdout, "[%s] [%s] port [%s] no open\n", domain, ip, dict_port[i]);
	}
}

int main()
{
	char *org = "163.com";

	int c = sizeof(dict_host)/sizeof(char *);

	int i = 0;
	for (; i < c; i++)
	{
		char name[128] = {0x0};
		snprintf(name, sizeof(name), "%s.%s", dict_host[i], org);

		char ip[32] = {0x0};

		if (get_ip_by_domain(ip, name))
			fprintf(stdout, "name [%s] no exist!\n", name);
		else
		{
			fprintf(stdout, "name [%s] ip [%s]!\n", name, ip);
			scan_port(name, ip);
		}

	}
	return 0;
}
