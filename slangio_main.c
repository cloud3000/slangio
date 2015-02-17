
/*-----------------------------------------------------------
 * Data Structures
 ----------------------------------------------------------*/
#include "slangio.h"
/* Prototypes */ 
int getSocket();
static int 	SaveUser(char *username);
static int 	SwitchUser(char *username);
static void KillChildren();
static void ChildExit (EV_P_ ev_child *w, int revents);
static void STDOUT_FromChild_Ready (struct ev_loop *loop, ev_io *w, int revents);
static void STDERR_FromChild_Ready (struct ev_loop *loop, ev_io *w, int revents);
static void STDOUT_FromClient_Ready (struct ev_loop *loop, ev_io *w, int revents);

static void Idle (struct ev_loop *loop, ev_idle *w, int revents);
static void CommandReady (struct ev_loop *loop, ev_io *w, int revents);
static void CommandAccept (struct ev_loop *loop, ev_io *w, int revents);
static int 	IsNumber(char *str);
static int 	GetLockFile(char *cmd);

int 		CreateCommandPipe (int id);
int 		DestroyCommandPipe (int id);
int 		SetupChildExit(int pid);
int 		MakeChild(char *cmd, char *user);
int 		EventLoop (int SIO_pid);

/* Globals */
static int Appl_stdin[2];
static int Appl_stdout[2];
static int Appl_stderr[2];

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
static char 	mpeldev[LINE_MAX];
static char 	branch[LINE_MAX];
static char 	authcode[LINE_MAX];
static char 	authclient[LINE_MAX];
static char 	conntype[LINE_MAX];
int 			sockfd = 0;
static int 		ToChild_fd;
static int 		FromChild_fd;
static int 		FromChildErr_fd;
 
/*---------------------------getSocket-----------------------*/
int getSocket()
{
    int file=0;
    struct stat fileStat;
    int SIO_MAX_FD = sysconf(_SC_OPEN_MAX);

     for (int i = 0; i < SIO_MAX_FD; ++i)
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

/*---------------------------SaveUser------------------------*/
static int SaveUser(char *username) {

	int	results=0;
	struct passwd *userinfo;

	userinfo = getpwnam(username);
	if (userinfo == NULL) {
		/*
		 * Invalid username so return to default thinclnt
		 */
		LogErr("SaveUser invalid user=%s changing to default %s", 
			username, SIO_DEFAULT_USER);
		userinfo = getpwnam(SIO_DEFAULT_USER);
	}
	if (userinfo == NULL) {
		/*
		 * Invalid username still so possible security violation
		 * so die to protect the system
		 */
		LogErr("SaveUser invalid user=%s default %s fails ", 
			username, SIO_DEFAULT_USER);
		LogAlert("SaveUser Security Failed; Aborting....");
		exit(1);
	}

//	LogDebug("SaveUser uid=%d gid=%d\n", userinfo->pw_uid, userinfo->pw_gid);
//	memcpy(&(masterptr->loginuserinfo), userinfo, sizeof(struct passwd));
	return results;
}

/*---------------------------SwitchUser------------------------*/
static int SwitchUser(char *username) {

	struct passwd *userinfo;

	userinfo = getpwnam(username);
	if (userinfo == NULL) {
		/*
		 * Invalid username so return to default thinclnt
		 */
		LogErr("SwitchUser invalid user=%s changing to default %s", 
			username, SIO_DEFAULT_USER);
		userinfo = getpwnam(SIO_DEFAULT_USER);
	}
	if (userinfo == NULL) {
		/*
		 * Invalid username still so possible security violation
		 * so die to protect the system
		 */
		LogErr("SwitchUser invalid user=%s default %s fails ", 
			username, SIO_DEFAULT_USER);
		LogAlert("SwitchUser Security Failed; Aborting....");
		exit(1);
	}

	LogDebug("uid=%d gid=%d\n", userinfo->pw_uid, userinfo->pw_gid);
//	chown(memfilename, userinfo->pw_uid, userinfo->pw_gid);
	initgroups(username, userinfo->pw_gid);
	setregid(userinfo->pw_gid, userinfo->pw_gid);
	setreuid(userinfo->pw_uid, userinfo->pw_uid);
	chdir(userinfo->pw_dir);

	PutEnv("HISTFILE=%s/.sh_history", userinfo->pw_dir);
	PutEnv("HISTSIZE=%d", SIO_HISTSIZE);

	//if (masterptr->sessiondir != NULL) {
	//	chdir(masterptr->sessiondir);
	// }
	return 0;
}

/*---------------------------KillChildren------------------------*/
static void KillChildren() {

	int i;

	for (i=0; i < SIO_nextchild; i++) {

		kill(SIO_children[i].pid, SIGQUIT);
	}
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

/*---------------------------STDOUT_FromClient_Ready------------------------*/
static void STDOUT_FromClient_Ready (struct ev_loop *loop, ev_io *w, int revents) {
	char	buffer[BUFFSIZE];
	char	result[BUFFSIZE];
	int len;
	int *output;
	
	memset( (void *)&result, '\0', sizeof(result)); 
	memset( (void *)&buffer, '\0', sizeof(buffer)); 
	fcntl (w->fd, F_SETFL, O_NONBLOCK);
	len = read(w->fd, buffer, sizeof(buffer));
	LogDebug("STDOUT_FromClient_Ready read (fd=%d) len=%d", w->fd, len);
	if (len == 0) {
		ev_io_stop(loop, w);
		free(w);
	} else if (len > 0) {
		sprintf(buffer, "%s", buffer);
		LogDebug("STDOUT_FromClient_Ready (fd=%d) buffer=%s", w->fd, buffer);
		sprintf(result, "%s\n", buffer);
		len = strlen(result);
		Writen(Appl_stdin[WRITE_END], result, len);
	} else {
		LogErr("STDOUT_FromClient_Ready (fd=%d) read failed", w->fd, buffer);
	}
}


/*---------------------------STDOUT_FromChild_Ready------------------------*/
static void STDOUT_FromChild_Ready (struct ev_loop *loop, ev_io *w, int revents) {

	char	buffer[BUFFSIZE];
	char	result[BUFFSIZE];
	int len;
	int *output;
	
	memset( (void *)&result, '\0', sizeof(result)); 
	memset( (void *)&buffer, '\0', sizeof(buffer)); 
	fcntl (w->fd, F_SETFL, O_NONBLOCK);
	len = read(w->fd, buffer, sizeof(buffer));
	LogDebug("STDOUT_FromChild_Ready read (fd=%d) len=%d", w->fd, len);
	if (len == 0) {
		ev_io_stop(loop, w);
		free(w);
	} else if (len > 0) {
		sprintf(buffer, "%s", buffer);
		LogDebug("STDOUT_FromChild_Ready (fd=%d) buffer=%s", w->fd, buffer);
		sprintf(result, "%s\n", buffer);
		len = strlen(result);
		Writen(sockfd, result, len);
	} else {
		LogErr("STDOUT_FromChild_Ready (fd=%d) read failed", w->fd, buffer);
	}
}

/*---------------------------STDERR_FromChild_Ready------------------------*/
static void STDERR_FromChild_Ready (struct ev_loop *loop, ev_io *w, int revents) {

	char	buffer[BUFFSIZE];
	char	result[BUFFSIZE];
	int len;
	int *output;
	
	memset( (void *)&result, '\0', sizeof(result)); 
	memset( (void *)&buffer, '\0', sizeof(buffer)); 
	fcntl (w->fd, F_SETFL, O_NONBLOCK);
	len = read(w->fd, buffer, sizeof(buffer));
	LogDebug("STDERR_FromChild_Ready read (fd=%d) len=%d", w->fd, len);
	if (len == 0) {
		ev_io_stop(loop, w);
		free(w);
	} else if (len > 0) {
		sprintf(buffer, "%s", buffer);
		LogDebug("STDERR_FromChild_Ready (fd=%d) buffer=%s", w->fd, buffer);
		//write(SIO_SOCKET_SAVE_FD, buffer, len);
		sprintf(result, "%s\n", buffer);
		len = strlen(result);
		Writen(sockfd, result, len);
	} else {
		LogErr("STDERR_FromChild_Ready (fd=%d) read failed", w->fd, buffer);
	}
}

/*---------------------------Idle------------------------*/
static void Idle (struct ev_loop *loop, ev_idle *w, int revents) {

	int	childstatus;
	pid_t waitresult;
	ev_tstamp yieldtime = 0.10;

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

/*---------------------------CommandReady------------------------*/
static void CommandReady (struct ev_loop *loop, ev_io *w, int revents) {

	static char	buffer[LINE_MAX];
	int len;
	int *output;
	char *token; 
	int token_len;
	int rc;
	
	LogDebug("CommandReady begins");
	fcntl (w->fd, F_SETFL, O_NONBLOCK);
	len = read(w->fd, buffer, sizeof(buffer));
	if (len == 0) {
		/* End of File */
		LogErr("CommandReady (fd=%d) (len=0) read EOF", w->fd);
		if (errno != EAGAIN) {
			/* 
			 * This means that the command input has closed and
			 * this function is at EOF so close the io handler
			 * and the file descriptor.
			 *
			 */
			ev_io_stop(loop, w);
			close(w->fd);
		}
	} else if (len > 0) {
		sprintf(buffer, "%s", buffer);
		LogDebug("CommandReady (fd=%d) buffer=%s", w->fd, buffer);

		token = strtok(buffer, WHITESPACE);
		token_len = strlen(token);
		if (! strncmp(token, SIO_STOP, len) ) {
			KillChildren();
			ev_io_stop(loop, w);
			ev_unloop (loop, EVUNLOOP_ALL);
		} else {
			LogErr("CommandReady (fd=%d) unknown command", w->fd, buffer);
		}
	} else {
		LogErr("CommandReady (fd=%d) read failed", w->fd);
	}
	LogDebug("CommandReady ends");
}

/*---------------------------CommandAccept------------------------*/
static void CommandAccept (struct ev_loop *loop, ev_io *w, int revents) {

	ev_io 			*input;
	int 			comfd = STDIN_FILENO; 
	struct sockaddr_un	addr;
	struct stat		statinfo;
	int			len;

	LogDebug("CommandAccept begins");
	fcntl (w->fd, F_SETFL, O_NONBLOCK);
	len = sizeof(addr);
	errno = 0;
	while (comfd >= 0) {

		memset(&addr, 0, sizeof(addr));
		comfd = accept(w->fd, (struct sockaddr *) &addr, &len);

		LogDebug("CommandAccept: accept client path=%s", addr.sun_path);

		if ((comfd < 0) && (errno != EAGAIN)) {
			LogDebug("CommandAccept: accept fails comfd=%d", comfd);
			break;
		}
		if ((comfd < 0) && (errno == EAGAIN)) {
			LogDebug("CommandAccept: accept no more inbound requests comfd=%d", comfd);
			break;
		}

		/* 
		 * Setup io handler for new client connection 
		 */
		input = malloc(sizeof(ev_io));
		ev_io_init(input, CommandReady, comfd, EV_READ);
		ev_io_start(EV_DEFAULT_ input);
		LogDebug("CommandAccept: accept good comfd=%d", comfd);
	}
	LogDebug("CommandAccept ends");
}

/*---------------------------CreateCommandPipe------------------------*/
int CreateCommandPipe (int id) {

	char fifo[LINE_MAX];
	int results;
	struct sockaddr_un addr;
	int	len;
	int	pipefd;
	LogDebug("CreateCommandPipe Started");
	snprintf(fifo, sizeof(fifo), SIO_FIFO_FMT, id);
	unlink(fifo);
	// strcpy(masterptr->SIO_control, fifo);
	LogDebug("Creating UNIX socket %s", fifo);

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, fifo);
	len = strlen(addr.sun_path) + sizeof(addr.sun_family);

	pipefd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (pipefd < 0) {
		LogErr("CreateCommandPipe socket failed");
		return pipefd;
	}

	results = bind(pipefd, (struct sockaddr *) &addr, len);
	if (results < 0) {
		LogErr("CreateCommandPipe bind failed");
		return results;
	}

	results = listen(pipefd, 32);
	if (results < 0) {
		LogErr("CreateCommandPipe listen failed");
		return results;
	}

	LogDebug("CreateCommandPipe listening on fd %d", pipefd );
	results = pipefd; /* socket to listen to for commands  */
	LogDebug("CreateCommandPipe results=%d", results);
	return results;
}

/*---------------------------DestroyCommandPipe------------------------*/
int DestroyCommandPipe (int id) {

	char fifo[LINE_MAX];
	int results;

	snprintf(fifo, sizeof(fifo), SIO_FIFO_FMT, id);
	results = unlink(fifo);

	return results;
}

/*---------------------------IsNumber------------------------*/
static int IsNumber(char *str) {

	int i;
	int len;
	int results = 1;

	len = strlen(str);
	for (i=0; i < len; i++) {
		if (! isdigit(str[i]))  {
			results = 0;
			break;
		}
	}

	return results;
}

/*---------------------------SetupChildExit------------------------*/
int SetupChildExit(int pid) {

	int results = 0;

	/* Setup event handler for this child process */
	ev_child_init(&SIO_children[SIO_nextchild], ChildExit, pid, 0);
	ev_child_start(EV_DEFAULT_ &SIO_children[SIO_nextchild]);
	SIO_nextchild++;
	SIO_count++;

	return results; 
}
/*---------------------------MakeChild------------------------*/
int MakeChild(char *cmd, char *user) {
	
	LogDebug("MakeChild Starting");
	int rc = 0;
	int argc = 0;
	static char buf[BUFFSIZE] = "";
	static char copycmd[LINE_MAX] = "";
	static char execcmd[LINE_MAX] = "";
	static char arg0[LINE_MAX] = "";
	static char *argv[MAX_CMD_ARGS];
	char *token;
	ev_io *childout;
	ev_io *childerr;
	ev_io *clientout;
	int i;

	SIO_MAX_FD = sysconf(_SC_OPEN_MAX);

	/* Arg 0 */
	LogDebug("A -- MakeChild Parse argv");
	strncpy(arg0, user, sizeof(arg0));
	strncat(arg0, ": ", sizeof(arg0));
	strncpy(copycmd, cmd, sizeof(copycmd));
	token = strtok(copycmd, WHITESPACE);
	strncpy(execcmd, token, sizeof(execcmd));
	strncat(arg0, execcmd, sizeof(arg0));

	argv[argc] = arg0;
	argc++;

	/* Parse rest of command */
	LogDebug("B -- MakeChild Parse argv");
	token = strtok(NULL, WHITESPACE);
	while (token != NULL) {
		argv[argc] = token;
		argc++;
		token = strtok(NULL, WHITESPACE);
	}
	
	argv[argc] = (char *) NULL;
	LogDebug("MakeChild argc=%d cmd=%s", argc, cmd);

	/* Set up standard I/O for the child*/
	LogDebug("MakeChild Create Pipes");
	pipe(Appl_stdin);
	pipe(Appl_stdout);
	pipe(Appl_stderr);
	ToChild_fd 		= Appl_stdin[WRITE_END];
	FromChild_fd 	= Appl_stdout[READ_END];
	FromChildErr_fd = Appl_stderr[READ_END];

	// static int client_output 	= sockfd; 
	// static int child_output 	= STDOUT_FILENO; 
	// static int child_output_err = STDERR_FILENO; 

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
		SwitchUser(user);

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

		rc = execvp(execcmd, argv);
		LogDebug("MakeChild execvp failed rc=%d cmd=%s", rc, execcmd);

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
		ev_io_init(childout, STDOUT_FromChild_Ready, FromChild_fd, EV_READ);
	//	childout->data = &child_output;
		ev_io_start(EV_DEFAULT_ childout);

		/* Setup event handler for child errors */
		LogDebug("Slang.main parent, setting I/O read event on fd: %d", Appl_stderr[READ_END]);
		childerr = malloc(sizeof(ev_io));
		ev_io_init(childerr, STDERR_FromChild_Ready, FromChildErr_fd, EV_READ);
	//	childerr->data = &child_output_err;
		ev_io_start(EV_DEFAULT_ childerr);

		/* Setup event handler for Client browser output */
		clientout = malloc(sizeof(ev_io));
		ev_io_init(clientout, STDOUT_FromClient_Ready, sockfd, EV_READ);
	//	clientout->data = &client_output;
		ev_io_start(EV_DEFAULT_ clientout);

		/* Close child end of each pipe */

		LogDebug("Slang.main parent, Closing Appl_stdin[READ_END]: %d", Appl_stdin[READ_END]);
		close(Appl_stdin[READ_END]);
		LogDebug("Slang.main parent, Closing Appl_stdout[WRITE_END]: %d", Appl_stdout[WRITE_END]);
		close(Appl_stdout[WRITE_END]);
		LogDebug("Slang.main parent, Closing Appl_stderr[WRITE_END]: %d", Appl_stderr[WRITE_END]);
		close(Appl_stderr[WRITE_END]);
	} else {
		/* Ricky Ricardo might say: MUY maldito MALA! El sistema no puede crear un nuevo proceso */
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

	LogDebug("EventLoop Started");
	loop = ev_default_loop(EVBACKEND_SELECT);

	/* Create an idle event to stop the program */
	eidle = malloc(sizeof(ev_idle));
	ev_idle_init(eidle, Idle);
	ev_idle_start(loop, eidle);

	/* Create an command input event */
	input = malloc(sizeof(ev_io));
	command = CreateCommandPipe(SIO_pid);
	ev_io_init(input, CommandAccept, command, EV_READ);
	ev_io_start(loop, input);
	ev_loop(loop, 0);
	LogDebug("EventLoop event loop finished");

	return 0;
}
/*-----------------------------------------------------------*/


/*-----------------------------------------------------------*/
int main (int argc, char *argv[]) {

	int memfd;
	int SIO_pid;
	int SIO_page;
	int i;
	int rc;
	int wait_rc;
	int block;
	unsigned short mpeidx = 0;
	unsigned short mpeid = 0;
	unsigned char *memptr;
	unsigned char *secptr;
	char	*envvar = NULL;
	char buf[LINE_MAX];
	//struct tms sessiontime;
	struct stat filestat;
	char logid[] = "slangio_main";

	// Get socket descriptor
	sockfd = getSocket(); 
    sprintf(buf, "slangio_main, socket descriptor is: %d\n", sockfd);
    Writen(sockfd, buf, strlen(buf)); // This is first output sent back to the client.
	
    openlog("slangio_main", LOG_PID, LOG_USER);
	setlogmask(LOG_UPTO(LOG_DEBUG));
    syslog(LOG_INFO, "%s STARTED ", argv[0]);
	LogDebug("slangio_main started");
	LogDebug("%s",buf);

	sleep(5);
	SIO_pid = getpid();
	LogDebug("Session Leader is %d", SIO_pid);
	LogDebug("slangio_main Session Leader is %d", SIO_pid);
	LogInfo("LOGIN MAX NAME LEN=%d", sysconf(_SC_LOGIN_NAME_MAX));

	PutEnv(SIO_MAIN, SIO_pid);

	rc = MakeChild(checksec, loginuser);

	/*
	 * Start the event loop
	 * Note:
	 * If the firstprog exists then the EventLoop will shutdown, and this program, will exit
	 */

	EventLoop(SIO_pid);
	
	/*
	 * Clean up on exit close the log and return 0 (OK in UNIX)
	 */
	LogDebug("slangio_main shutdown");
	DestroyCommandPipe(SIO_pid);
	LogClose();
	rc = 0; 
	return rc;
}
