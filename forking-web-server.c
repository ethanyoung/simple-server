/**
 * forking-web-server - a simple server which could serve one specified file
 * to web browsers on a specified port.
 */

#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>	/* Type definitions used by many programs*/
#include <stdio.h>		/* Standard I/O functions */
#include <stdlib.h>		/* Prototypes of commonly used library functions,
			   	   	   	   plus EXIT_SUCCESS and EXIT_FAILURE constants*/
#include <unistd.h>		/* Prototypes for many system calls */
#include <errno.h>		/* Declares errno and defines error constants */
#include <string.h>		/* Commonly used string-handling functions */

#define BUF_SIZE 4096
#define MAXITEM 4096

/* Create listening socket */
int inetListen(const char *service, int backlog);

/* SIGCHILD handler to reap dead child processes */
static void grimReaper(int sig);

/* Handle the request from a client */
static void handleRequest(int cfd,  char * filename);

/* Send the HTTP header to the client */
int sendHeader(int cfd);

/* Send a canned HTML content to the client */
int sendCannedHTML(int cfd);

/* Send a file to the client */
int sendFile(int cfd,  char * filename);

/* Split a string by white space,
 *  then store each sub string in a string array*/
void split(char * s, char * array[MAXITEM]);

/* Get file types from mime-types.tsv */
int getExtention(char ** types);

/* Check whether the file is existed,
 * if it is readable,
 * and if the file's extension is listed in mime-types.tsv */
int isFileValid(char* filename, int n, char * types[MAXITEM]);


int
inetListen(const char *service, int backlog)
{
	struct addrinfo hints;
	struct addrinfo *result;	/* Store results of getaddrinfo */
	struct addrinfo *rp;		/* Temporary copy of results */
	int sfd;					/* Service socket fd */
	int optval;
	int s;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_UNSPEC;			/* Allows IPv4 or IPv6 */
	hints.ai_flags = AI_PASSIVE;			/* Use wildcard IP address */

	s = getaddrinfo (NULL, service ,&hints, &result);
	if(s != 0)
		return -1;

	/* Walk through returned list until we find an address structure
	 * that can be used to successfully create and bind a socket */

	optval = 1;
	for (rp =result; rp!= NULL; rp = rp->ai_next) {

		/* Create server socket */
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd== -1)
			continue;						/* On error, try next address */

		if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &optval,
			sizeof(optval)) == -1) {
			close(sfd);
			freeaddrinfo(result);
			return -1;
		}

		/* Bind the address to the socket */
		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;							/* Success */

		/* The function bind() failed: close this socket and try next address */
		close(sfd);
	}

	/* Mark the socket as a listening socket */
	if (rp != NULL) {
		if (listen(sfd, backlog) == -1) {
			freeaddrinfo(result);
			return -1;
		}
	}

	return (rp == NULL) ? -1 : sfd;
}

static void
grimReaper(int sig)
{
	int savedErrno;		/* Save 'errno' in case changed here */

	savedErrno = errno;
	while (waitpid(-1, NULL, WNOHANG) > 0)
		continue;
	errno = savedErrno;
}

/* Handle a client request: copy socket input back to socket */

static void
handleRequest(int cfd, char * filename)
{
	char buf[BUF_SIZE];
	ssize_t numRead;

	int optval;

	/* Enable TCP_CORK option on "cfd" - subsequent TCP output is corked
	 * until this option is disabled. This is to increase efficiency. */

	optval = 1;
	if (setsockopt(cfd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval)) == -1){
		syslog(LOG_ERR, "Error from setsockopt(): %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (sendHeader(cfd) == -1)
		exit(EXIT_FAILURE);

	//if (sendCannedHTML(cfd) == -1)
	//	exit(EXIT_FAILURE);

	if (sendFile(cfd, filename) == -1){
		exit(EXIT_FAILURE);
	}

	/* Disable TCP_CORK option on 'cfd' - corked output is now transmitted
	 * in a single TCP segment. */

	optval = 0;
	if (setsockopt(cfd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval)) == -1){
		syslog(LOG_ERR, "Error from setsockopt(): %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

int
sendHeader(int cfd)
{
	/* Content size and type are optional */
	char *header =
			"HTTP/1.1 200 OK\
			Server: GWS/2.0\
			Cache-control: private\
			Set-Cookie: PREF=ID=73d4aef52e57bae9:TM=1042253044:LM=1042253044:S=SMCc_HRPCQiqy\
			X9j; expires=Sun, 17-Jan-2038 19:14:07 GMT; path=/; domain=.google.com\
			Connection: keep-alive\n\n";

	if (send(cfd, header, strlen(header), 0) != strlen(header))
		return -1;
	else
		return 0;
}

int
sendCannedHTML(int cfd)
{
	const char *page =
		"<html>\
		<head>\
		<title>Canned HTML</title>\
		</head>\
		<body>\
		<h1> This is a hard coded HTML file</h1>\
		</body>\
		</html>";

	if (send(cfd, page, strlen(page), 0) != strlen(page))
		return -1;
	else
		return 0;
}

int
sendFile(int cfd, char * filename)
{

	int input_fd;
	struct stat file_info;
	int rval;

	input_fd = open (filename, O_RDONLY);
	if (input_fd == -1){
		printf("File \"%s\" not found\n", filename);
		return -1;
	}
	rval = fstat (input_fd, &file_info);
	
	if(rval == -1){
		syslog(LOG_ERR, "Error from fstat(): %s", strerror(errno));
		return -1;
	}
	
	printf("sending...\n");

	off_t offset = 0;
	rval = sendfile(cfd, input_fd, &offset, file_info.st_size);		
	printf("filesize: %d, bytes sent: %d\n\n", (int)file_info.st_size, rval);
	if(rval != file_info.st_size) {
		syslog(LOG_ERR, "Error from sendfile(): %s", strerror(errno));
		return -1;
	}

	/* Receive response from the client */
	char buf[BUF_SIZE];
	if (recv(cfd, buf, BUF_SIZE, 0) == -1) {
		printf("Communication error\n");
		return -1;
	}
	printf("Received from client:\n%s", buf);

	close(input_fd);
	return 0;
}

void
split(char * s, char * array[MAXITEM])
{
	int i;

	/* Clear the array */
	for(i=0; i<MAXITEM;i++)
		array[i] = NULL;
	char * p =  strtok(s, " ");
	int k = 0;
	while(p)
	{
		array[k] = calloc(strlen(p)+1,1);

		/* Use strcpy to copy string */
		strcpy(array[k], p);
		k++;
		p = strtok(NULL, " ");
	}

}

int
getExtention( char** types)
{
	FILE * fp = fopen("mime-types.tsv", "r");
	if (fp == NULL){
		printf("mime types file not found");
		return -1;
	}
	
	int i = 0;
	char * line;
	
	/* Skip the first line which contains the number of types */
	line = malloc(10);
	fgets(line, 10, fp);

	while (!feof(fp))
	{
		/* Get each line from mime types file */
		line = malloc(sizeof(char) * 100);
		fgets(line, 100, fp);
		if (strlen(line) == 0)
			break;

		/* Split line by white-space */
		char * temp[MAXITEM];
		split(line, temp);
		types[i] = malloc(strlen(temp[0]));
		strcpy(types[i],temp[0]);
		i++;
	}
	

	fclose(fp);
	return i;
}

int
isFileValid(char* filename, int n, char * types[MAXITEM])
{
	/* Check whether the file exits */
	if(access(filename, F_OK) != 0){
		printf("File not found!\n");
		syslog(LOG_ERR, "Error from access(): %s", strerror(errno));
		return -1;
	}

	/* Check whether the file is excutable */
	else if(access(filename, R_OK) != 0){
		printf("No execute permission!\n");
		syslog(LOG_ERR, "Error from access(): %s", strerror(errno));
		return -1;
	}

	/* Check if the file's extension is valid */
	char * dot;
	dot = strrchr(filename, '.');
	if (dot == NULL)
		return -1;
	dot++;
	int i;
	for(i=0; i<n; i++)
		if (strcmp(dot, types[i]) == 0)		/* Successfully find */
			return 0;

	/* The extension is not valid */
	printf("Invalid file type\n");
	return -1;
}

int 
main(int argc, char *argv[])
{
	/* Get the number of types from mime-types.tsv */
	char * count;
	count = malloc(10);
	FILE * fp = fopen("mime-types.tsv", "r");
	if (fp == NULL){
		printf("mime types file not found");
		return -1;
	}
	fgets(count, 10, fp);
	fclose(fp);

	char ** extentions;			/* Contains all the valid extentions */
	extentions = malloc((int) count);	

	int n = getExtention(extentions);	/* Number of extentions avalaible*/
	if ( n == -1){
		printf("Cannot get MIME types\n");
		exit(EXIT_FAILURE);
	}

	/* Check command line arguments */
	if (argc != 3) {
		printf("No filename or port number\n");
		exit(EXIT_FAILURE);
	}

	/* Check the file extention */
	if (isFileValid(argv[1], n, extentions) == -1) {
		exit(EXIT_FAILURE);
	}


	int lfd;			/* Listening socket*/
	int cfd;			/* Connected socket*/

	/* Reap dead child processes */
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = grimReaper;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		syslog(LOG_ERR, "Error from segaction(): %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	/* Creat listenning socket */
	lfd = inetListen((char*)argv[2], 10);
	if (lfd == -1) {
		syslog(LOG_ERR, "Could not create server socket (%s)", strerror(errno));
		exit(EXIT_FAILURE);
	}


	printf("Serving start...\n"
			"Press \"Ctrl + C\" to exit.\n");

	/* Handle each client request in a new child process */
	for(;;) {
		cfd = accept(lfd, NULL, NULL);	/* Create connected socket */
		if(cfd == -1) {
			syslog(LOG_ERR, "Failure in accept(): %s", strerror(errno));
			exit(EXIT_FAILURE);
		}


		switch(fork()) {
			case -1:
				syslog(LOG_ERR, "Can't create child (%s)", strerror(errno));
				close(cfd);	/* Give up on this client */
				break;
	
			case 0:			/* Success, for a child */
				close(lfd);	/* Unneeded copy of listening socket */
				handleRequest(cfd, argv[1]);
				_exit(EXIT_SUCCESS);
				break;		/* This line is not necessary actually*/

			default:		/* Parent */
				close(cfd);	/* Unneeded copy of connected socket */
				break;		/* Loop to accept next connection */
		}
	}

	return 0;
}
