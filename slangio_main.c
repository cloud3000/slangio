#include "slangio.h"
#include "websocket.h"
// cobc -x slangio_main.c libslangio.c websocket.c sha1.c base64_enc.c -lev
//#define TRUE 1
//#define FALSE 0
#define PACKET_DUMP
#define BUF_LEN 0xFFFF

/* Prototypes */ 
void error(const char *);
int safeSend(int , const uint8_t *, size_t );
static void 	Idle (struct ev_loop *, ev_idle *, int);
static void 	ChildExit (EV_P_ ev_child *, int);
int 			getSocket();
int 			MakeChild(char *, char *);
int 			EventLoop (int);

ssize_t			STDOUT_to_Socket(int fd, const void *vptr, size_t n);
static void 	STDOUT_FromChild_Event (struct ev_loop *, ev_io *, int);
static ssize_t 	STDOUT_Actual_Read(int, char *);
ssize_t 		STDOUT_GetNextLine(int, void *, size_t);

ssize_t			STDERR_to_Socket(int fd, const void *vptr, size_t n);
static void 	STDERR_FromChild_Event (struct ev_loop *, ev_io *, int);
static ssize_t 	STDERR_Actual_Read(int, char *);
ssize_t 		STDERR_GetNextLine(int, void *, size_t);

ssize_t			SOCKET_to_Appl(int fd, const void *vptr, size_t n);
static void 	SOCKET_FromClient_Event (struct ev_loop *, ev_io *, int);
static ssize_t 	SOCKET_Actual_Read(int, char *);
ssize_t 		SOCKET_GetNextLine(int, void *, size_t);

/* Globals */

static int 		Appl_stdin[2];
static int 		Appl_stdout[2];
static int 		Appl_stderr[2];
static int 		sockfd = 0;

static int 		SIO_toplevel = 0;
static int 		SIO_MAX_FD;
static int 		jsdebug = 0;
static 			ev_child SIO_children[SIO_CHILD_BLOCKS];
static int 		SIO_nextchild = 0;
static int 		SIO_count = 0;
static char 	SIO_exec[LINE_MAX];
static char 	checksec[LINE_MAX] = SIO_APPL_SECURITY;
static char 	loginuser[LINE_MAX] = SIO_DEFAULT_USER;
static char 	firstprog[LINE_MAX] = SIO_APPL_MAIN;
static int 		ToChild_fd;
static int 		FromChild_fd;
static int 		FromChildErr_fd;
static int 		SHUTDOWN = FALSE;

static int 		STDOUT_ready = FALSE;
static int		STDOUT_cnt;
static char		*STDOUT_ptr;
static char		STDOUT_buf[BUFFSIZE];

static int 		STDERR_ready = FALSE;
static int		STDERR_cnt;
static char		*STDERR_ptr;
static char		STDERR_buf[BUFFSIZE];

static int 		SOCKET_ready = FALSE;
static int		SOCKET_cnt;
static char		*SOCKET_ptr;
static char		SOCKET_buf[BUFFSIZE];
/* ************************************************************** */
void error(const char *msg)
{
 
    perror(msg);
    exit(EXIT_FAILURE);
 
}
 
/* ************************************************************** */
int safeSend(int clientSocket, const uint8_t *buffer, size_t bufferSize)
{
 
    ssize_t written = send(clientSocket, buffer, bufferSize, 0);
    if (written == -1) {
 
        close(clientSocket);
        perror("send failed");
        return EXIT_FAILURE;
     
    }
    if (written != bufferSize) {
 
        close(clientSocket);
        perror("written not all bytes");
        return EXIT_FAILURE;
     
    }
     
    return EXIT_SUCCESS;
 
}
/* ************************************************************** */
ssize_t	STDOUT_to_Socket(int fd, const void *vptr, size_t n)
{
    uint8_t buffer[BUF_LEN];
    size_t frameSize = n;

    memset(buffer, 0, BUF_LEN);
    wsMakeFrame((uint8_t *)vptr, n, buffer, &frameSize, WS_TEXT_FRAME);

    return(safeSend(fd, buffer, frameSize));
}

/* ************************************************************** */
ssize_t	STDERR_to_Socket(int fd, const void *vptr, size_t n)
{
    uint8_t buffer[BUF_LEN];
    size_t frameSize = 0;

    memset(buffer, 0, BUF_LEN);
    wsMakeFrame((uint8_t *)vptr, n, buffer, &frameSize, WS_TEXT_FRAME);

    return(safeSend(fd, buffer, frameSize));
}

/* ************************************************************** */
ssize_t	SOCKET_to_Appl(int fd, const void *vptr, size_t n)
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
static ssize_t STDOUT_Actual_Read(int fd, char *ptr)
{
	if (STDOUT_cnt <= 0) { // TRUE on first call?
outagain:
		if ( (STDOUT_cnt = read(fd, STDOUT_buf, sizeof(STDOUT_buf))) < 0) {
			if (errno == EINTR) // The call was interrupted by a signal before any data was read;
				goto outagain;	    // Go read again. 
				
			return(-1); // read error, and errno is set appropriately. 
		} else if (STDOUT_cnt == 0)
			return(0); // End of File
		// STDOUT_cnt is > 0
		STDOUT_ptr = STDOUT_buf; // Update address of read pointer. 
	}
	STDOUT_cnt--;
	*ptr = *STDOUT_ptr++; // Set pointer to read pointer and advance the read pointer by 1 character.
	return(1);
}
/* ************************************************************** */ 
ssize_t STDOUT_GetNextLine(int fd, void *vptr, size_t maxlen)
{
	ssize_t	n, rc;
	char	c, *ptr;
	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
		if ( (rc = STDOUT_Actual_Read(fd, &c)) == 1) {
			*ptr++ = c;
			if (c == '\n')
				break;	/* newline is stored, like fgets() */
		} else if (rc == 0) {
			*ptr = 0;
			return(n - 1);	/* EOF, n - 1 bytes were read */
		} else
			return(-1);		/* error, errno set by read() */
	}
	*ptr = 0;	/* null terminate like fgets() */
	return(n);
}
/* ************************************************************** */
static ssize_t STDERR_Actual_Read(int fd, char *ptr)
{
	if (STDERR_cnt <= 0) { // TRUE on first call?
erragain:
		if ( (STDERR_cnt = read(fd, STDERR_buf, sizeof(STDERR_buf))) < 0) {
			if (errno == EINTR) // The call was interrupted by a signal before any data was read;
				goto erragain;	    // Go read again. 
				
			return(-1); // read error, and errno is set appropriately. 
		} else if (STDERR_cnt == 0)
			return(0); // End of File
		// STDERR_cnt is > 0
		STDERR_ptr = STDERR_buf; // Update address of read pointer. 
	}
	STDERR_cnt--;
	*ptr = *STDERR_ptr++; // Set pointer to read pointer and advance the read pointer by 1 character.
	return(1);
}
/* ************************************************************** */ 
ssize_t STDERR_GetNextLine(int fd, void *vptr, size_t maxlen)
{
	ssize_t	n, rc;
	char	c, *ptr;
	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
		if ( (rc = STDERR_Actual_Read(fd, &c)) == 1) {
			*ptr++ = c;
			if (c == '\n')
				break;	/* newline is stored, like fgets() */
		} else if (rc == 0) {
			*ptr = 0;
			return(n - 1);	/* EOF, n - 1 bytes were read */
		} else
			return(-1);		/* error, errno set by read() */
	}
	*ptr = 0;	/* null terminate like fgets() */
	return(n);
}
/* ************************************************************** */
static ssize_t SOCKET_Actual_Read(int fd, char *ptr)
{
	if (SOCKET_cnt <= 0) { // TRUE on first call?
socketagain:
		if ( (SOCKET_cnt = read(fd, SOCKET_buf, sizeof(SOCKET_buf))) < 0) {
			if (errno == EINTR) // The call was interrupted by a signal before any data was read;
				goto socketagain;	    // Go read again. 
				
			return(-1); // read error, and errno is set appropriately. 
		} else if (SOCKET_cnt == 0)
			return(0); // End of File
		// SOCKET_cnt is > 0
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
			return(-1);		/* error, errno set by read() */
	}
	*ptr = 0;	/* null terminate like fgets() */
	return(n);
}
/*---------------------------getSocket-----------------------*/
int getSocket()
{
    int file=0;
    struct stat fileStat;
    int SIO_MAX_FD = sysconf(_SC_OPEN_MAX);
    int i=0;

     for (i = 0; i < SIO_MAX_FD; ++i)
    {
        if(fstat(i,&fileStat) == 0) {
            if ((fileStat.st_mode & S_IFMT) == S_IFSOCK) {
                file = i;
                break;
            }
        }
    }
    return file;
}

/*---------------------------ChildExit------------------------*/
static void ChildExit (EV_P_ ev_child *w, int revents) {

	ev_child_stop(EV_A_ w);
	LogInfo("ChildExit pid=%d status=%d", w->rpid, w->rstatus);
	/* Clean up after child process */
	if (SIO_count) {
		SIO_count--;
	}
}
/*---------------------------SOCKET_FromClient_Event------------------------*/
static void SOCKET_FromClient_Event (struct ev_loop *loop, ev_io *w, int revents) {
 
    uint8_t buffer[BUF_LEN];
	char	result[BUF_LEN];
    memset(buffer, 0, BUF_LEN);
    size_t len = 0;
    size_t readedLength = 0;
    size_t frameSize = BUF_LEN;
    enum wsState state = WS_STATE_OPENING;
    uint8_t *data = NULL;
    size_t dataSize = 0;
    enum wsFrameType frameType = WS_INCOMPLETE_FRAME;
    struct handshake hs;
    nullHandshake(&hs);
     
    #define prepareBuffer frameSize = BUF_LEN; memset(buffer, 0, BUF_LEN);
    #define initNewFrame frameType = WS_INCOMPLETE_FRAME; readedLength = 0; memset(buffer, 0, BUF_LEN);
     
    while (frameType == WS_INCOMPLETE_FRAME) {
 
        ssize_t readed = recv(w->fd, buffer+readedLength, BUF_LEN-readedLength, 0);
        if (!readed) {
            return;
        }
        readedLength+= readed;
        frameType = wsParseInputFrame(buffer, readedLength, &data, &dataSize);
        if ((frameType == WS_INCOMPLETE_FRAME && readedLength == BUF_LEN) || frameType == WS_ERROR_FRAME) {
                prepareBuffer;
                wsMakeFrame(NULL, 0, buffer, &frameSize, WS_CLOSING_FRAME);
                if (safeSend(w->fd, buffer, frameSize) == EXIT_FAILURE)
                    break;
                state = WS_STATE_CLOSING;
                initNewFrame;         
        }
        if (frameType == WS_CLOSING_FRAME) {
                prepareBuffer;
                wsMakeFrame(NULL, 0, buffer, &frameSize, WS_CLOSING_FRAME);
                safeSend(w->fd, buffer, frameSize);
         		sprintf(result, "ev://cli_exit/\n");
                LogDebug("SOCKET_FromClient_Event %s",result);
    	    	len = strlen(result);
	        	SOCKET_to_Appl(Appl_stdin[WRITE_END], result, len);
                break;
        } else if (frameType == WS_TEXT_FRAME) {
    		sprintf(result, "%s\n", buffer+6);
            LogDebug("SOCKET_FromClient_Event %s",result);
    		len = strlen(result);
	    	SOCKET_to_Appl(Appl_stdin[WRITE_END], result, len);
            break;
        }
        initNewFrame;
    } 
    return;
}

/*---------------------------SOCKET_FromClient_Event------------------------*/

/*
static void SOCKET_FromClient_Event (struct ev_loop *loop, ev_io *w, int revents) {
	ssize_t	nread;
	ssize_t len;
	ssize_t	t;
	char	buffer[BUFFSIZE];
	char	result[BUFFSIZE];
	int SOCKET_ready = TRUE;
	int myerror;
	t = 0;
	nread = 1;

	LogDebug("SOCKET_FromClient_Event ");
	memset( (void *)&result, '\0', sizeof(result)); 
	memset( (void *)&buffer, '\0', sizeof(buffer)); 
	fcntl (w->fd, F_SETFL, O_NONBLOCK);
	while ( SOCKET_ready ) {
		nread = SOCKET_GetNextLine(w->fd, buffer, sizeof(buffer));
		LogDebug("SOCKET_FromClient_Event read (fd=%d) len=%d", w->fd, nread);
		if (nread > 0) {
			t += nread;
			sprintf(result, "%s", buffer);
//			result[strcspn ( result, "\n" )] = '\0';
			len = strlen(result);
			SOCKET_to_Appl(Appl_stdin[WRITE_END], result, len);
		} else {
			myerror = errno;
			switch(myerror){
				case 0 :
					SOCKET_ready = FALSE;
					if (nread == 0){
						ev_io_stop(loop, w);
						LogDebug("Client closed SOCKET connection ");
						SHUTDOWN = TRUE;
						sprintf(result, "ev://cli_exit/\n");
						len = strlen(result);
						SOCKET_to_Appl(Appl_stdin[WRITE_END], result, len);
					}
					break;
			    case EWOULDBLOCK :
					SOCKET_ready = FALSE;
					break;
  				default : // assume any other error is so bad that the I/O channel is unusable.
					ev_io_stop(loop, w);
					LogDebug("Client SOCKET error connection closed");
					SOCKET_ready = FALSE;
					SHUTDOWN = TRUE;
			}
		}
	}
}
*/

/*---------------------------STDOUT_FromChild_Event------------------------*/
static void STDOUT_FromChild_Event (struct ev_loop *loop, ev_io *w, int revents) {
	ssize_t	nread;
	ssize_t len;
	ssize_t	t = 0;
	char	buffer[BUFFSIZE];
	char	result[BUFFSIZE];
	int 	myerror;
	t = 0;
	nread = 1;
	STDOUT_ready = TRUE;
	LogDebug("STDOUT_FromChild_Event ");
	memset( (void *)&result, '\0', sizeof(result)); 
	memset( (void *)&buffer, '\0', sizeof(buffer)); 
	fcntl (w->fd, F_SETFL, O_NONBLOCK);
	while ( STDOUT_ready ) {
		nread = STDOUT_GetNextLine(w->fd, buffer, sizeof(buffer));
		LogDebug("STDOUT_FromChild_Event read (fd=%d) len=%d", w->fd, nread);
		if (nread > 0) {
			t += nread;
			sprintf(result, "STDOUT:%s", buffer);
			len = strlen(result);
			STDOUT_to_Socket(sockfd, result, len);
		} else {
			myerror = errno;
			switch(myerror){
				case 0 :
					STDOUT_ready = FALSE;
					if (nread == 0){
						ev_io_stop(loop, w);
						LogDebug("Child closed STDOUT pipe ");
						SHUTDOWN = TRUE;
					}
					break;
			    case EWOULDBLOCK :
					STDOUT_ready = FALSE;
					break;
  				default : // assume any other error is so bad that the I/O channel is unusable.
					ev_io_stop(loop, w);
					LogDebug("Child STDOUT error pipe closed");
					STDOUT_ready = FALSE;
					SHUTDOWN = TRUE;
			}
		}
	}
}

/*---------------------------STDERR_FromChild_Event------------------------*/
static void STDERR_FromChild_Event (struct ev_loop *loop, ev_io *w, int revents) {

	ssize_t	nread;
	ssize_t len;
	ssize_t	t;
	char	buffer[BUFFSIZE];
	char	result[BUFFSIZE];
	int 	myerror;
	t = 0;
	nread = 1;
	STDERR_ready = TRUE;
	memset( (void *)&result, '\0', sizeof(result)); 
	memset( (void *)&buffer, '\0', sizeof(buffer)); 
	fcntl (w->fd, F_SETFL, O_NONBLOCK);
	while ( STDERR_ready ) {
		nread = STDERR_GetNextLine(w->fd, buffer, sizeof(buffer));
		if (nread > 0) {
			t += nread;
			sprintf(result, "STDERR:%s", buffer);
			len = strlen(result);
			STDERR_to_Socket(sockfd, result, strlen(result));
		} else {
			myerror = errno;
			switch(myerror){
				case 0 :
					STDERR_ready = FALSE;
					if (nread == 0){
						ev_io_stop(loop, w);
						LogDebug("Child closed STDERR pipe ");
						SHUTDOWN = TRUE;
					}
					break;
			    case EWOULDBLOCK :
					STDERR_ready = FALSE;
					break;
  				default : // assume any other error is so bad that the I/O channel is unusable.
					ev_io_stop(loop, w);
					LogDebug("Child STDERR error pipe closed");
					STDERR_ready = FALSE;
					SHUTDOWN = TRUE;
			}
		}
	}
}

/*---------------------------Idle------------------------*/
static void Idle (struct ev_loop *loop, ev_idle *w, int revents) {

	int	childstatus;
	pid_t waitresult;
	ev_tstamp yieldtime = 0.10;
	if (SHUTDOWN) SIO_count = -1;

	if (SIO_count <= 0) {
		LogDebug("Idle processing (children=%d)", SIO_count);
		ev_idle_stop(loop, w);
		ev_unloop (loop, EVUNLOOP_ALL);
	} else {
		/*
		 * Check to make sure we have at least one child left
		*/
		waitresult = waitpid((pid_t) 0, &childstatus, WNOHANG);	
		if ((waitresult < 0) && (errno == ECHILD)) {
			LogDebug("Idle processing waitpid thinks children are dead so shutdown this session");
			SIO_count = -1;
		}
		ev_sleep(yieldtime);
	}
}
/*---------------------------MakeChild------------------------*/
int MakeChild(char *cmd, char *user) {
	
	LogDebug("MakeChild Starting");
	static char arg0[LINE_MAX] = "x y z ";
	static char *argv[MAX_CMD_ARGS];
	int rc = 0;
	int argc = 0;
	static char buf[BUFFSIZE] = "";
	ev_io *childout;
	ev_io *childerr;
	ev_io *clientout;
	int i;

	SIO_MAX_FD = sysconf(_SC_OPEN_MAX);
	
	argv[6] = (char *) NULL;
	LogDebug("MakeChild argc=%d cmd=%s", argc, cmd);

	/* Set up standard I/O for the child*/
	LogDebug("MakeChild Create Pipes");
	pipe(Appl_stdin);
	pipe(Appl_stdout);
	pipe(Appl_stderr);
	ToChild_fd 		= Appl_stdin[WRITE_END];
	FromChild_fd 	= Appl_stdout[READ_END];
	FromChildErr_fd = Appl_stderr[READ_END];

   	LogDebug("Slang.main parent, socket descriptor is     : %d", sockfd);
   	LogDebug("Slang.main parent, STDIN descriptor is      : %d", STDIN_FILENO);
   	LogDebug("Slang.main parent, STDOUT descriptor is     : %d", STDOUT_FILENO);
   	LogDebug("Slang.main parent, STDERR descriptor is     : %d", STDERR_FILENO);
   	LogDebug("Slang.main parent, STDINpipe0 descriptor is : %d", Appl_stdin[READ_END]);
   	LogDebug("Slang.main parent, STDINpipe1 descriptor is : %d", Appl_stdin[WRITE_END]);
   	LogDebug("Slang.main parent, STDOUTpipe0 descriptor is: %d", Appl_stdout[READ_END]);
   	LogDebug("Slang.main parent, STDOUTpipe1 descriptor is: %d", Appl_stdout[WRITE_END]);
   	LogDebug("Slang.main parent, STDERRpipe0 descriptor is: %d", Appl_stderr[READ_END]);
   	LogDebug("Slang.main parent, STDERRpipe1 descriptor is: %d", Appl_stderr[WRITE_END]);

	rc = fork(); // if rc == 0, then this is a the child process

	if (rc == 0) {
		/* Set the child process to the appropriate user */
		//SwitchUser(user);

		/* Redirect stdin for the child, so we can write to the child */
		close(STDIN_FILENO);
		LogDebug("Slang.main child, Dup descriptor %d to %d", Appl_stdin[READ_END], STDIN_FILENO);
		dup2(Appl_stdin[READ_END],STDIN_FILENO);

		/* Redirect stdout for the child, to read output from the child */
		close(STDOUT_FILENO);
		LogDebug("Slang.main child, Dup descriptor %d to %d", Appl_stdout[WRITE_END], STDOUT_FILENO);
		dup2(Appl_stdout[WRITE_END],STDOUT_FILENO);

		/* Redirect stderr for the child, to read errors from the child */
		close(STDERR_FILENO);
		LogDebug("Slang.main child, Dup descriptor %d to %d", Appl_stderr[WRITE_END], STDERR_FILENO);
		dup2(Appl_stderr[WRITE_END],STDERR_FILENO);

		/* Close all other file descriptors greater than stderr*/
		for(i=STDERR_FILENO+1; i < SIO_MAX_FD; i++) { 
				if (!close(i)) {
					LogDebug("Slang.main child, Closed %d", i);
				}
			}
		LogDebug("Invalid files ignored");
    	LogDebug("Slang.main child, STDIN descriptor should be %d and is: %d", Appl_stdin[READ_END], STDIN_FILENO);
    	LogDebug("Slang.main child, STDOUT descriptor should be %d and is: %d", Appl_stdout[WRITE_END], STDOUT_FILENO);
    	LogDebug("Slang.main child, STDERR descriptor should be %d and is: %d", Appl_stderr[WRITE_END], STDERR_FILENO);

    //	sleep(5);
		rc = execvp("/volume1/applications/appmain", argv); // This is the COBOL application
		LogDebug("MakeChild execvp failed rc=%d cmd=/volume1/applications/appmain", rc);

	} else if (rc > 0) {

		/* This is the parent process */
		LogInfo("MakeChild started %s (pid=%d)", cmd, rc);

		/* Setup event handler for this child process */
		ev_child_init(&SIO_children[SIO_nextchild], ChildExit, rc, 0);
		ev_child_start(EV_DEFAULT_ &SIO_children[SIO_nextchild]);
		SIO_nextchild++;
		SIO_count++;

		/* Setup event handler for child output */
		LogDebug("Slang.main parent, setting I/O read event on fd: %d", Appl_stdout[READ_END]);
		childout = malloc(sizeof(ev_io));
		ev_io_init(childout, STDOUT_FromChild_Event, FromChild_fd, EV_READ);
		ev_io_start(EV_DEFAULT_ childout);

		/* Setup event handler for child errors */
		LogDebug("Slang.main parent, setting I/O read event on fd: %d", Appl_stderr[READ_END]);
		childerr = malloc(sizeof(ev_io));
		ev_io_init(childerr, STDERR_FromChild_Event, FromChildErr_fd, EV_READ);
		ev_io_start(EV_DEFAULT_ childerr);

		/* Setup event handler for Client browser output */
		clientout = malloc(sizeof(ev_io));
		ev_io_init(clientout, SOCKET_FromClient_Event, sockfd, EV_READ);
		ev_io_start(EV_DEFAULT_ clientout);

		/* Close child end of each pipe */

		LogDebug("Slang.main parent, Closing Appl_stdin[READ_END]: %d", Appl_stdin[READ_END]);
		close(Appl_stdin[READ_END]);
		LogDebug("Slang.main parent, Closing Appl_stdout[WRITE_END]: %d", Appl_stdout[WRITE_END]);
		close(Appl_stdout[WRITE_END]);
		LogDebug("Slang.main parent, Closing Appl_stderr[WRITE_END]: %d", Appl_stderr[WRITE_END]);
		close(Appl_stderr[WRITE_END]);
	} else {
		/* MUY maldito MALA! El sistema no puede crear un nuevo proceso */
		LogAlert("MakeChild failed to fork %s (pid=%d)", cmd, rc);
	}
	return rc;
}
/*-----------------------------EventLoop------------------------------*/
int EventLoop (int SIO_pid) {

	struct ev_loop *loop;
	struct ev_idle *eidle;
	ev_io *input;
	int command; 

	loop = ev_default_loop(EVBACKEND_SELECT);

	/* Create an idle event to stop the program */
	eidle = malloc(sizeof(ev_idle));
	ev_idle_init(eidle, Idle);
	ev_idle_start(loop, eidle);

	/* Start loop */
	LogDebug("EventLoop Started");
	ev_loop(loop, 0);
	LogDebug("EventLoop event loop finished");

	return 0;
}
/*-----------------------------------------------------------*/


/*-----------------------------------------------------------*/
int main (int argc, char *argv[]) {

	int SIO_pid;
	int rc;
	char buf[LINE_MAX];
	char logid[] = "slangio_main";
	struct sockaddr_in client;
	struct sockaddr_in server;
	int		addrlen;
	int		results;
	char		*client_addr;
	unsigned short	client_port;
	char		*server_addr;
	unsigned short	server_port;

//ws vars
    uint8_t buffer[BUF_LEN];
    memset(buffer, 0, BUF_LEN);
    size_t readedLength = 0;
    size_t frameSize = BUF_LEN;
    enum wsState state = WS_STATE_OPENING;
    uint8_t *data = NULL;
    size_t dataSize = 0;
    enum wsFrameType frameType = WS_INCOMPLETE_FRAME;
    struct handshake hs;
    nullHandshake(&hs);
     
    #define prepareBuffer frameSize = BUF_LEN; memset(buffer, 0, BUF_LEN);
    #define initNewFrame frameType = WS_INCOMPLETE_FRAME; readedLength = 0; memset(buffer, 0, BUF_LEN);
//ws var end


	// Get socket descriptor
	sockfd = getSocket(); 
    while (frameType == WS_INCOMPLETE_FRAME) {
 
        ssize_t readed = recv(sockfd, buffer+readedLength, BUF_LEN-readedLength, 0);
        if (!readed) {
            return 1;
        }

        readedLength+= readed;
  
        frameType = wsParseHandshake(buffer, readedLength, &hs);
                  
        if ((frameType == WS_INCOMPLETE_FRAME && readedLength == BUF_LEN) || frameType == WS_ERROR_FRAME) {
 
            if (frameType == WS_INCOMPLETE_FRAME)
                printf("buffer too small");
            else
                printf("error in incoming frame\n");
             
            if (state == WS_STATE_OPENING) {
 
                prepareBuffer;
                frameSize = sprintf((char *)buffer,
                                    "HTTP/1.1 400 Bad Request\r\n"
                                    "%s%s\r\n\r\n",
                                    versionField,
                                    version);
                safeSend(sockfd, buffer, frameSize);
                break;
             
            } else {
 
                prepareBuffer;
                wsMakeFrame(NULL, 0, buffer, &frameSize, WS_CLOSING_FRAME);
                if (safeSend(sockfd, buffer, frameSize) == EXIT_FAILURE)
                    break;
                state = WS_STATE_CLOSING;
                initNewFrame;
             
            }
         
        }
         
        if (state == WS_STATE_OPENING) {
 
            assert(frameType == WS_OPENING_FRAME);
            if (frameType == WS_OPENING_FRAME) {
 
                // if resource is right, generate answer handshake and send it
                if (strcmp(hs.resource, "/echo") != 0) {
 
                    frameSize = sprintf((char *)buffer, "HTTP/1.1 404 Not Found\r\n\r\n");
                    if (safeSend(sockfd, buffer, frameSize) == EXIT_FAILURE)
                        break;
                 
                }
             
                prepareBuffer;
                wsGetHandshakeAnswer(&hs, buffer, &frameSize);
                if (safeSend(sockfd, buffer, frameSize) == EXIT_FAILURE)
                    break;
                state = WS_STATE_NORMAL;
                initNewFrame;
                break;             
            } 
        }    
    } // read/write cycle

	sprintf(buf, "slangio_main, socket FD: %d\n", sockfd);
	STDOUT_to_Socket(sockfd, buf, strlen(buf));

	addrlen = sizeof(client);
	results = getpeername(sockfd, (struct sockaddr *) &client, &addrlen);
	if (results == 0 && client.sin_family == AF_INET ) {
		client_addr = inet_ntoa(client.sin_addr);
		client_port = ntohs(client.sin_port);
		sprintf(buf,"slangio_main, client=%s port=%d\n", client_addr, client_port);
		STDOUT_to_Socket(sockfd, buf, strlen(buf));
	}
	
	results = getsockname(sockfd, (struct sockaddr *) &server, &addrlen);
	if (results == 0 && server.sin_family == AF_INET) {
		server_addr = inet_ntoa(server.sin_addr);
		server_port = ntohs(server.sin_port);
		sprintf(buf,"slangio_main, server=%s port=%d\n", server_addr, server_port);
		STDOUT_to_Socket(sockfd, buf, strlen(buf));
	}

	LogOpenSystem(logid);
	LogDebugOn();
    LogDebug("%s STARTED ", argv[0]);
	LogDebug("slangio_main started");
	LogDebug("%s",buf);

	SIO_pid = getpid();
	LogDebug("Session Leader is %d", SIO_pid);
	LogDebug("slangio_main Session Leader is %d", SIO_pid);
	LogInfo("LOGIN MAX NAME LEN=%d", sysconf(_SC_LOGIN_NAME_MAX));

	PutEnv(SIO_MAIN, SIO_pid);

	rc = MakeChild(checksec, loginuser);

	EventLoop(SIO_pid);
	
	LogDebug("slangio_main shutdown");
	LogClose();
	return 0;
}
