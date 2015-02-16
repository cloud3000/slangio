#include <syslog.h>      /* for syslog() */
#include "unp.h"
#define TRUE 1
#define FALSE 0

void 			LogDebug(char *msg, ...);
static void 	Idle (struct ev_loop *, ev_idle *, int);
static void 	ReadStdin(struct ev_loop *, ev_io *, int );
static int 		EventLoop ();

static char		line[MAXLINE];
static int 		sockfd;

int 		SHUTDOWN = FALSE;
char *global_argv[64];

/*---------------------------LogDebug------------------------*/
void LogDebug(char *msg, ...) {
	va_list args;
	char fullmsg[MAXLINE];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_DEBUG, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}
/*---------------------------Idle------------------------*/
static void Idle (struct ev_loop *loop, ev_idle *w, int revents) {
	if (SHUTDOWN) {
		LogDebug("Idle processing (SHUTDOWN=%d)", SHUTDOWN);
		ev_idle_stop(loop, w);
		ev_unloop (loop, EVUNLOOP_ALL);
	} else {
		ev_tstamp yieldtime = 0.10;
		ev_sleep(yieldtime);
	}
}
/*-----------------------------ReadStdin------------------------------*/
static void ReadStdin(struct ev_loop *loop, ev_io *w, int revents) {
	int			ntowrite;
	char 		result[MAXLINE];
	char 		new_string[MAXLINE];

	// Read from stdin
    __fpurge(stdin);
    fflush(stdout);
	fgets(new_string, MAXLINE, stdin);
	LogDebug("ReadStdin: %s : %s", global_argv[0], new_string);

	// Processing input from User Interface
	if ((strncmp("ev://cli_exit/", (const char *)&new_string, 14)) == 0) {
		sprintf(result, "ev://app_exit/%s\n", global_argv[0]);
		Writen(STDOUT_FILENO, result, strlen(result));
		SHUTDOWN = TRUE;
		ev_io_stop(loop, w);
	} else {
		sprintf(result, "%s\n", new_string);
		Writen(STDOUT_FILENO, result, strlen(result));
	}

	// Set buffers to NULL
	memset( (void *)&result, '\0', sizeof(result)); 
	memset( (void *)&new_string, '\0', sizeof(new_string)); 
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

	ev_loop(loop, 0);
	LogDebug("%s EventLoop finished", global_argv[0]);

	return 0;
}

/*---------------------------main------------------------
              M A I N   E N T R Y    P O I N T
  ---------------------------main------------------------*/
int main(int argc, char *argv[])
{
	*global_argv = *argv;
    openlog("application", LOG_PID, LOG_USER);
	setlogmask(LOG_UPTO(LOG_DEBUG));
    syslog(LOG_INFO, "%s STARTED ", argv[0]);

	int rc = EventLoop();

    syslog(LOG_INFO, "%s SHUTDOWN ", argv[0]);
	/* SHUTDOWN */
	close(sockfd);
	exit(0);
}
