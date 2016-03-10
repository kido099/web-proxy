/*
 * proxy.c - Web proxy for COMPSCI 512
 *
 */

#include <stdio.h>
#include "csapp.h"
#include <pthread.h>

#define   FILTER_FILE   "proxy.filter"
#define   LOG_FILE      "proxy.log"
#define   DEBUG_FILE	"proxy.debug"


/*============================================================
 * function declarations
 *============================================================*/

int  find_target_address(char * uri,
			 char * target_address,
			 char * path,
			 int  * port);


void  format_log_entry(char * logstring,
		       int sock,
		       char * uri,
		       int size);
		       
void *forwarder(void* args);
void *webTalk(void* args);
void secureTalk(int clientfd, rio_t client, char *inHost, char *version, int serverPort);
void ignore();

int debug;
int proxyPort;
int debugfd;
int logfd;
pthread_mutex_t mutex;

/* main function for the proxy program */

int main(int argc, char *argv[])
{
  int count = 0;
  int listenfd, connfd, clientlen, optval, serverPort, i;
  struct sockaddr_in clientaddr;
  struct hostent *hp;
  char *haddrp;
  sigset_t sig_pipe; 
  pthread_t tid;
  int *args;
  
  if (argc < 2) {
    printf("Usage: ./%s port [debug] [webServerPort]\n", argv[0]);
    exit(1);
  }
  if(argc == 4)
    serverPort = atoi(argv[3]);
  else
    serverPort = 80;
  
  Signal(SIGPIPE, ignore); // ignore SIGPIPE signals
  
  if(sigemptyset(&sig_pipe) || sigaddset(&sig_pipe, SIGPIPE))
    unix_error("creating sig_pipe set failed");
  if(sigprocmask(SIG_BLOCK, &sig_pipe, NULL) == -1)
    unix_error("sigprocmask failed");
  
  proxyPort = atoi(argv[1]);

  if(argc > 2)
    debug = atoi(argv[2]);
  else
    debug = 0;


  /* start listening on proxy port */

  listenfd = Open_listenfd(proxyPort);

  optval = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int)); 
  
  if(debug) debugfd = Open(DEBUG_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0666);

  logfd = Open(LOG_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0666);    


  /* if writing to log files, force each thread to grab a lock before writing
     to the files */
  
  pthread_mutex_init(&mutex, NULL);
  
  while(1) {

    clientlen = sizeof(clientaddr);

    /* accept a new connection from a client here */

    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    
    /* you have to write the code to process this new client request */
    /* create a new thread (or two) to process the new connection */
    args=malloc(2*sizeof(int));
    args[0]=connfd;
    args[1]=serverPort;
    Pthread_create(&tid,NULL,webTalk,(void *)args);
    Pthread_detach(tid);
  }
  
  if(debug) Close(debugfd);
  Close(logfd);
  pthread_mutex_destroy(&mutex);
  
  return 0;
}

/* a possibly handy function that we provide, fully written */

void parseAddress(char* url, char* host, char** file, int* serverPort)
{
	char *point1;
        char *point2;
        char *saveptr;

	if(strstr(url, "http://"))
		url = &(url[7]);
	*file = strchr(url, '/');
	
	strcpy(host, url);

	/* first time strtok_r is called, returns pointer to host */
	/* strtok_r (and strtok) destroy the string that is tokenized */

	/* get rid of everything after the first / */

	strtok_r(host, "/", &saveptr);

	/* now look to see if we have a colon */

	point1 = strchr(host, ':');
	if(!point1) {
		*serverPort = 80;
		return;
	}
	
	/* we do have a colon, so get the host part out */
	strtok_r(host, ":", &saveptr);

	/* now get the part after the : */
	*serverPort = atoi(strtok_r(NULL, "/",&saveptr));
}



/* this is the function that I spawn as a thread when a new
   connection is accepted */

void *webTalk(void* args)
{
  printf("Thread created finish!\n");
  
  int numBytes, lineNum, serverfd, clientfd, serverPort;
  int tries=3;
  int byteCount = 0;
  char buf1[MAXLINE], buf2[MAXLINE], buf3[MAXLINE];
  char host[MAXLINE];
  char url[MAXLINE], logString[MAXLINE];
  char *token, *cmd, *version, *file, *saveptr;
  rio_t server, client;
  char slash[10];
  strcpy(slash, "/");
  
  clientfd = ((int*)args)[0];
  serverPort = ((int*)args)[1];
  free(args);
  
  Rio_readinitb(&client, clientfd);;
  
  // Determine protocol (CONNECT or GET)
  while((Rio_readlineb(&client,buf1,MAXLINE))<1) {
    tries--;
    if(tries<0) {
      close(clientfd);
      return NULL;
    }
  }
  printf("Read request finish\n");
  
  cmd=strtok_r(buf1," \r\n",&saveptr);
  printf("cmd = %s\n",cmd);
  
  strcpy(url,strtok_r(NULL," \r\n",&saveptr));
  printf("url = %s\n",url);
  
  version=strtok_r(NULL," \r\n",&saveptr);
  printf("version = %s\n",version);
  printf("Before parse\n");

  parseAddress(url,host,&file,&serverPort);

  printf("parse finish\n");
  printf("cmd = %s\n",cmd);
  printf("url = %s\n",url);
  printf("file = %s\n",file);
  if(file==NULL) file=slash;
  printf("file = %s\n",file);
  
  // GET: open connection to webserver (try several times, if necessary)
  tries=3;
  if(strcmp(cmd,"GET")==0) {
    while((serverfd=Open_clientfd(host,serverPort))<1) {
      tries--;
      if(tries<0) {
	printf("Error: Failed to open connection to webserver for 'GET'!\n");
	//close(clientfd);
	return NULL;
      } 
    }
    printf("get,connect to server finish\n");
    /* GET: Transfer first header to webserver */
    sprintf(buf1,"%s %s %s\r\n",cmd,file,version);
    printf("buf1 = %s",buf1);
    Rio_writen(serverfd,buf1,strlen(buf1));
    printf("write first header to webserver finish\n");
    // GET: Transfer remainder of the request
    while((byteCount=Rio_readlineb(&client,buf2,MAXLINE))>0) {
      if(strcmp(buf2,"Proxy-Connection: Keep-Alive\r\n")==0) {
	Rio_writen(serverfd,"Connection: close\r\n",strlen("Connection: close\r\n"));
	continue;
      }
      Rio_writen(serverfd,buf2,byteCount);
      if(strcmp(buf2,"\r\n")==0) break;
    }
    printf("write remainder to web server finish\n");
    // GET: now receive the response(To fetch the content fron the origin web server) 
    while(1) {
      if((byteCount=Rio_readp(serverfd,buf3,MAXLINE))<1) {
	break;
      }
      Rio_writen(clientfd,buf3,byteCount);
    }
    printf("receive response finish\n");
    close(clientfd);
    close(serverfd);
    return NULL;
  }
  // CONNECT: call a different function, securetalk, for HTTPS   
  else if(strcmp(cmd,"CONNECT")==0) {
    secureTalk(clientfd,client,host,version,serverPort);
    // close(clientfd);
    //close(serverfd);
    //return NULL;
  }
  return NULL;
}


/* this function handles the two-way encrypted data transferred in
   an HTTPS connection */

void secureTalk(int clientfd, rio_t client, char *inHost, char *version, int serverPort)
{
  printf("goto securetalk\n");
  int serverfd, numBytes1, numBytes2;
  int tries=3;
  rio_t server;
  char buf1[MAXLINE], buf2[MAXLINE];
  pthread_t tid;
  int *args;

  if (serverPort == proxyPort)
    serverPort = 443;
  
  /* Open connecton to webserver */
  /* clientfd is browser */
  /* serverfd is server */
  while((serverfd=Open_clientfd(inHost,serverPort))<1) {
    tries--;
    if(tries<0) {
      printf("Error: Failed to open connection to webserver for 'CONNECT'!\n");
      return;
    }
  }
  /* let the client know we've connected to the server */
  Rio_writep(clientfd,"HTTP/1.1 200 Connection established\r\n\r\n",strlen("HTTP/1.1 200 Connection established\r\n\r\n"));
  /* spawn a thread to pass bytes from origin server through to client */
  args=malloc(2*sizeof(int));
  args[0]=clientfd;
  args[1]=serverfd;
  Pthread_create(&tid,NULL,forwarder,(void *)args);
  /* now pass bytes from client to server */
  while(1) {
    if((numBytes1=Rio_readp(clientfd,buf1,MAXLINE))<1) {
      break;     
    }
    Rio_writen(serverfd,buf1,numBytes1);
  }
  Pthread_join(tid,NULL);
  close(clientfd);
  close(serverfd);
}

/* this function is for passing bytes from origin server to client */

void *forwarder(void* args)
{
  printf("goto forwarder\n");
  int numBytes, lineNum, serverfd, clientfd;
  int byteCount = 0;
  char buf1[MAXLINE];
  clientfd = ((int*)args)[0];
  serverfd = ((int*)args)[1];
  free(args);

  while(1) {
    /* serverfd is for talking to the web server */
    /* clientfd is for talking to the browser */
    if((numBytes=Rio_readp(serverfd,buf1,MAXLINE))<1) {
      break;
    }
    Rio_writen(clientfd,buf1,numBytes);   
  }
  return NULL;
}


void ignore()
{
	;
}


/*============================================================
 * url parser:
 *    find_target_address()
 *        Given a url, copy the target web server address to
 *        target_address and the following path to path.
 *        target_address and path have to be allocated before they 
 *        are passed in and should be long enough (use MAXLINE to be 
 *        safe)
 *
 *        Return the port number. 0 is returned if there is
 *        any error in parsing the url.
 *
 *============================================================*/

/*find_target_address - find the host name from the uri */
int  find_target_address(char * uri, char * target_address, char * path,
                         int  * port)

{
  //  printf("uri: %s\n",uri);
  

    if (strncasecmp(uri, "http://", 7) == 0) {
	char * hostbegin, * hostend, *pathbegin;
	int    len;
       
	/* find the target address */
	hostbegin = uri+7;
	hostend = strpbrk(hostbegin, " :/\r\n");
	if (hostend == NULL){
	  hostend = hostbegin + strlen(hostbegin);
	}
	
	len = hostend - hostbegin;

	strncpy(target_address, hostbegin, len);
	target_address[len] = '\0';

	/* find the port number */
	if (*hostend == ':')   *port = atoi(hostend+1);

	/* find the path */

	pathbegin = strchr(hostbegin, '/');

	if (pathbegin == NULL) {
	  path[0] = '\0';
	  
	}
	else {
	  pathbegin++;	
	  strcpy(path, pathbegin);
	}
	return 0;
    }
    target_address[0] = '\0';
    return -1;
}



/*============================================================
 * log utility
 *    format_log_entry
 *       Copy the formatted log entry to logstring
 *============================================================*/

void format_log_entry(char * logstring, int sock, char * uri, int size)
{
    time_t  now;
    char    buffer[MAXLINE];
    struct  sockaddr_in addr;
    unsigned  long  host;
    unsigned  char a, b, c, d;
    int    len = sizeof(addr);

    if (getpeername(sock, (struct sockaddr *) & addr, &len)) {
      /* something went wrong writing log entry */
      printf("getpeername failed\n");
      return;
    }

    host = ntohl(addr.sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;

    sprintf(logstring, "%s: %d.%d.%d.%d %s %d\n", buffer, a,b,c,d, uri, size);
}
