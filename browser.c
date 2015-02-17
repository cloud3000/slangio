#include "slangio.h"
#define SERVER "localhost"
#define PORT 8081
#define TRUE 1
#define FALSE 0

static void 	ReadStdin (struct ev_loop *, ev_io *, int);
static void 	ReadSocket (struct ev_loop *, ev_io *, int);
static int 		EventLoop ();

static char		line[BUFFSIZE];
static int 		sockfd;
int 			SHUTDOWN = FALSE;

char *global_argv[64];

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
	sprintf(result, "%s\n", new_string);
	Writen(sockfd, result, strlen(result));

	memset( (void *)&result, '\0', sizeof(result)); 
	memset( (void *)&new_string, '\0', sizeof(new_string)); 
}
/*-----------------------------ReadSocket-----------------------------*/
static void ReadSocket (struct ev_loop *loop, ev_io *w, int revents) {
	ssize_t		nread;
	char 		result[BUFFSIZE];

	// Get input from remote application
    LogDebug("%s ReadSocket Fired ", global_argv[0]);
	nread = Readline(sockfd, line, BUFFSIZE);

	// Processing input from remote application
	if (nread < 1) {
		ev_io_stop(loop, w);
		printf("Remote connection closed.\n");
		SHUTDOWN = TRUE;
	} else {
		if (line == "ev://app_exit/") {
			SHUTDOWN = TRUE;
			ev_io_stop(loop, w);
		} else {
			sprintf(result, "%s\n", line	);
			printf("[%s]\n", result );
		}
	}
	memset( (void *)&result, '\0', sizeof(result)); 
	memset( (void *)&line, '\0', sizeof(line)); 
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

	/* Get on with the event-driven conversation */
	int rc = EventLoop();

    syslog(LOG_INFO, "%s SHUTDOWN ", argv[0]);
	/* SHUTDOWN */
	close(sockfd);
	exit(0);
}
