#include "slangio.h"
#define SERVER "localhost"
#define PORT 8081
#define TRUE 1
#define FALSE 0

ssize_t			STDIN_to_Socket(int, const void *, size_t);
static ssize_t 	SOCKET_Actual_Read(int, char *);
ssize_t 		SOCKET_GetNextLine(int, void *, size_t);
static void 	ReadStdin (struct ev_loop *, ev_io *, int);
static void 	ReadSocket (struct ev_loop *, ev_io *, int);
static int 		EventLoop ();

static char		line[BUFFSIZE];
static int 		sockfd;
int 			SHUTDOWN = FALSE;
int 			SOCKET_ready = FALSE;
static int		SOCKET_cnt;
static char		*SOCKET_ptr;
static char		SOCKET_buf[BUFFSIZE];

char *global_argv[64];
/* ************************************************************** */
ssize_t	STDIN_to_Socket(int fd, const void *vptr, size_t n)
{
	size_t		nleft;
	ssize_t		nwritten;
	const char	*ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0;		/* and call write() again */
			else
				return(-1);			/* error */
		}

		nleft -= nwritten;
		ptr   += nwritten;
	}
	return(n);
}
/* ************************************************************** */
static ssize_t SOCKET_Actual_Read(int fd, char *ptr)
{
	if (SOCKET_cnt <= 0) { // TRUE on first call?
socketagain:
		if ( (SOCKET_cnt = read(fd, SOCKET_buf, sizeof(SOCKET_buf))) < 0) {
			if (errno == EINTR) {	// The call was interrupted by a signal before any data was read;
				goto socketagain;	// Go read again. 
			}

			return(-1); // read error, errno is set appropriately.	
		} else {
			if (SOCKET_cnt == 0) {  // End of file. The remote has closed the connection. 
				SOCKET_ready = FALSE;
				return(0);
			}				
		} 

		//If we'er here, then SOCKET_cnt is > 0
		SOCKET_ptr = SOCKET_buf; // Update address of read pointer. 
	}
	SOCKET_cnt--;
	*ptr = *SOCKET_ptr++; // Set pointer to read pointer and advance the read pointer by 1 character.
	return(1);
}
/* ************************************************************** */ 
ssize_t SOCKET_GetNextLine(int fd, void *vptr, size_t maxlen)
{
	ssize_t	n, rc;
	char	c, *ptr;
	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
		if ( (rc = SOCKET_Actual_Read(fd, &c)) == 1) {
			*ptr++ = c;
			if (c == '\n')
				break;	/* newline is stored, like fgets() */
		} else if (rc == 0) {
			*ptr = 0;
			return(n - 1);	/* EOF, n - 1 bytes were read */
		} else
			return(rc);		/* error, errno set by read() */
	}
	*ptr = 0;	/* null terminate like fgets() */
	return(n);
}
/*---------------------------Idle------------------------*/
static void Idle (struct ev_loop *loop, ev_idle *w, int revents) {
	if (SHUTDOWN) {
		LogDebug("Idle processing (SHUTDOWN=%d)", SHUTDOWN);
		printf("Idle processing SHUTDOWN=%d, Shutting down.\n", SHUTDOWN );
		ev_idle_stop(loop, w);
		ev_unloop (loop, EVUNLOOP_ALL);
	} else {
		ev_tstamp yieldtime = 0.10;
		ev_sleep(yieldtime);
	}
}
/*-----------------------------ReadStdin------------------------------*/
static void ReadStdin(struct ev_loop *loop, ev_io *w, int revents) {
	char 		result[BUFFSIZE];
	char 		new_string[BUFFSIZE];

    LogDebug("%s ReadStdin Fired ", global_argv[0]);
    __fpurge(stdin);
    fflush(stdout);
	fgets(new_string, BUFFSIZE, stdin);

	// Processing input from stdin
	sprintf(result, "%s", new_string);
	STDIN_to_Socket(sockfd, result, strlen(result));


	memset( (void *)&result, '\0', sizeof(result)); 
	memset( (void *)&new_string, '\0', sizeof(new_string)); 
}
/*-----------------------------ReadSocket-----------------------------*/
static void ReadSocket (struct ev_loop *loop, ev_io *w, int revents) {
	ssize_t		nread;
	ssize_t		len;
	ssize_t		t;
	char 		result[BUFFSIZE];
	nread = 0;
	SOCKET_ready = TRUE;
	memset( (void *)&result, '\0', sizeof(result)); 
	memset( (void *)&line, '\0', sizeof(line)); 
    LogDebug("%s ReadSocket Fired ", global_argv[0]);
	fcntl (w->fd, F_SETFL, O_NONBLOCK);
	while ( SOCKET_ready ) {
		nread = SOCKET_GetNextLine(w->fd, line, sizeof(line));
		if (nread > 0) {
			t += nread;
			sprintf(result, "%s", line);
			len = strlen(result);
			if (line == "ev://app_exit/") {
				SHUTDOWN = TRUE;
				ev_io_stop(loop, w);
				printf("Application requested SHUTDOWN\n");
			} else {
				printf("%s", result );
			}
		} else {
			if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) { 
			// No more data to read, until next socket event occurs.
				SOCKET_ready = FALSE;
			} else {
				// assume any other error is so bad that the I/O channel is unusable.
				ev_io_stop(loop, w);
				printf("Remote connection closed.\n");
				SHUTDOWN = TRUE;
			}
		}
	} // End while
}

/*-----------------------------EventLoop------------------------------*/
static  int EventLoop () {
	struct ev_loop *loop;
	struct ev_idle *eidle;
	ev_io *standard_input;
	ev_io *socket_input;

	LogDebug("%s EventLoop Started", global_argv[0]);
	loop = ev_default_loop(EVBACKEND_SELECT);

	/* Create an idle event to stop the program */
	eidle = malloc(sizeof(ev_idle));
	ev_idle_init(eidle, Idle);
	ev_idle_start(loop, eidle);

	/* Create the standard_input event */
	standard_input = malloc(sizeof(ev_io));
	ev_io_init(standard_input, ReadStdin, STDIN_FILENO, EV_READ);
	ev_io_start(loop, standard_input);

	/* Create the socket_input event */
	socket_input = malloc(sizeof(ev_io));
	ev_io_init(socket_input, ReadSocket, sockfd, EV_READ);
	ev_io_start(loop, socket_input);

	ev_loop(loop, 0);
	LogDebug("%s EventLoop finished", global_argv[0]);

	return 0;
}
/*---------------------------main------------------------
              M A I N   E N T R Y    P O I N T
  ---------------------------main------------------------*/
int main(int argc, char *argv[])
{
	struct sockaddr_in server;
	struct hostent *sp;
	char *host;
	*global_argv = *argv;
    openlog("browser", LOG_PID, LOG_USER);
	setlogmask(LOG_UPTO(LOG_DEBUG));
    syslog(LOG_INFO, "%s STARTED", argv[0]);

	/* Create a socket */
	sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	/* Connect it to a server */
	memset((char *) &server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons((u_short) PORT);
	sp = gethostbyname(SERVER);
	memcpy(&server.sin_addr, sp->h_addr, sp->h_length);
	connect(sockfd, (struct sockaddr *) &server, sizeof(struct sockaddr_in));

	/* All event driven I/O starts in the EventLoop */
	int rc = EventLoop();
	/* This process ends when all event driven I/O file descriptors are closed */

    syslog(LOG_INFO, "%s SHUTDOWN ", argv[0]);
	/* SHUTDOWN */
	close(sockfd);
	exit(0);
}
