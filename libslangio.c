#include "slangio.h"
static pid_t GetMasterPid();
static int libslangio_debug = 0;
static int memfd = -1;
static int LOG_CONTROL = 0;
extern char **environ;

void PutEnv (char *format, ...) {
	va_list args;
	char *env_var;
	int rc;

	env_var = malloc(LINE_MAX);
	va_start(args, format);
	vsnprintf(env_var, LINE_MAX, format, args);  
	rc = putenv(env_var);
	if (rc < 0) {
		LogErr("Unable to set %s", env_var);
	} else {
		LogDebug(env_var);
	}
	va_end(args);
}

unsigned char vplusblock1[1024*1024] = "BLANK DATA 1";
unsigned char vplusblock2[1024*1024] = "BLANK DATA 2";

void GetPtrVplusBlock1(unsigned char **ptr) {

	*ptr = vplusblock1;
 
}

void GetPtrVplusBlock2(unsigned char **ptr) {

	*ptr = vplusblock2;
 
}

static sig_atomic_t mydeath_of_child;
static struct sigaction mysigchld_action;
static pid_t mymy_children[64];
static int mydeadlock_avoid = 0;

void myhandle_child_death(int signal_event) {
	
	char		cmd[LINE_MAX];
	char		response[LINE_MAX];
	int			status;
	pid_t		childpid;
	int			i;

	childpid = waitpid(-1, &status, WNOHANG);
	/* Store the child status in case we need it */
	mydeath_of_child = status;

	LogDebug("Begins");

	if (childpid > 0) {
		LogDebug("Detected actual child process");
		/*
		 * See if it is one of my special children
		 */
		for(i=0; i<64; i++) {
			if (mymy_children[i] == childpid) { 
				mymy_children[i] = 0;

				/*
				 * Activate this process
				 */
				sprintf(cmd, "unlock %u", getpid());
				LogDebug("Unlocking myself (%s)", cmd);
				//SendTCCommand(cmd, response);
				break;
			}
		}
	} 

	LogDebug("Ends");
	return;
}


static int GetUserAccount(uid_t uid, gid_t gid, char *useracct, int useracctlen) {
	struct passwd *passwd_info;
	struct group *group_info;
	char	*envacct;
	char	defacct[5] = "NONE";
	int result = 0;
	int len = 0;

	passwd_info = getpwuid(uid);
	group_info = getgrgid(gid);

	LogDebug("passwd_info.pw_name=%s", passwd_info->pw_name);
	memset(useracct, ' ', useracctlen); 	

	if (useracctlen < 0) {
		useracctlen = 0;
	}

	len = strlen(passwd_info->pw_name);
	if (len > useracctlen) {
		len = useracctlen;
	}
	memcpy(useracct, passwd_info->pw_name, len);
	useracctlen = useracctlen - len;
	if (useracctlen < 0) {
		useracctlen = 0;
	}

	len = 1;
	if (len > useracctlen) {
		len = useracctlen;
	}
	memcpy(useracct, ".", len);
	useracctlen = useracctlen - len;
	if (useracctlen < 0) {
		useracctlen = 0;
	}

	envacct = getenv("HPACCOUNT");
	if (envacct == NULL) {
		envacct = defacct;
	}
	len = strlen(envacct);
	if (len > useracctlen) {
		len = useracctlen;
	}
	memcpy(useracct, envacct, len);

	return result;
}

static pid_t GetMasterPid() 
{

	pid_t masterpid = 0;
	int rc;
	char *siomaster;
	unsigned short result = 0;

	siomaster = getenv("SIO_MAIN");
	if (siomaster == NULL) {
		return masterpid;
	}

	rc = sscanf(siomaster, "%d", &masterpid);
	if (rc != 1) {
		masterpid = 0;
		return masterpid;
	}
	return masterpid;
}

extern void LogOpenSystem(char *id) {
    openlog(id, LOG_PID,LOG_USER);
	LOG_CONTROL = 1;
}

extern void LogDebugOn() {
	setlogmask(LOG_UPTO(LOG_DEBUG));
}

extern void LogDebugOff() {
	setlogmask(LOG_UPTO(LOG_ERR));
}

extern void LogEmerg(char *msg, ...) {
	va_list args;
	char fullmsg[LINE_MAX];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_EMERG, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}


extern void LogAlert(char *msg, ...) {
	va_list args;
	char fullmsg[LINE_MAX];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_ALERT, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}

extern void LogCrit(char *msg, ...) {
	va_list args;
	char fullmsg[LINE_MAX];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_CRIT, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}

extern void LogErr(char *msg, ...) {
	va_list args;
	char fullmsg[LINE_MAX];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_ERR, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}

extern void LogWarning(char *msg, ...) {
	va_list args;
	char fullmsg[LINE_MAX];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_WARNING, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}

extern void LogNotice(char *msg, ...) {
	va_list args;
	char fullmsg[LINE_MAX];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_NOTICE, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}

extern void LogInfo(char *msg, ...) {
	va_list args;
	char fullmsg[LINE_MAX];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_INFO, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}

extern void LogDebug(char *msg, ...) {
	va_list args;
	char fullmsg[LINE_MAX];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_DEBUG, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}

extern void LogClose() {
	closelog();
}

static int	read_cnt;
static char	*read_ptr;
static char	read_buf[BUFFSIZE];
/* ************************************************************** */
static ssize_t Actual_Read(int fd, char *ptr)
{
/*	Actual_Read will read 0 to MAXLINE bytes, or return error -1
	If error -1, then we return, AKA: we're done reading.
	Once the read succeeds then read_cnt contains the number of bytes that were read.
	Each call thereafter to Actual_Read doesn't call 'read', because read_cnt is positive.
	So while read_cnt is positive Actual_Read will increment *read_ptr, char by char, while
	decrementing read_cnt, until read_cnt is zero returning the char at *read_ptr to GetNextLine.

	'GetNextLine' will continue calling Actual_Read 
	until maxlen is reached, or *read_ptr points to a new line '\n' character.

	Actual_Read should be immune to signal interruption.
*/ 
	if (read_cnt <= 0) { // TRUE on first call?
again:
		if ( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {
			if (errno == EINTR) // The call was interrupted by a signal before any data was read;
				goto again;	    // Go read again. 
				
			return(-1); // read error, and errno is set appropriately. 
		} else if (read_cnt == 0)
			return(0); // End of File
		// read_cnt is > 0
		read_ptr = read_buf; // Update address of read pointer. 
	}
	read_cnt--;
	*ptr = *read_ptr++; // Set pointer to read pointer and advance the read pointer by 1 character.
	return(1);
}
/* ************************************************************** */
ssize_t GetNextLine(int fd, void *vptr, size_t maxlen)
{
	ssize_t	n, rc;
	char	c, *ptr;
	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
		if ( (rc = Actual_Read(fd, &c)) == 1) {
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

/* **************************Readline**************************** */
/*                   E N T R Y    P O I N T                       */
/*                 ReadLine calls GetNextLine                     */
/*                GetNextLine calls ActualRead                    */
/*    GetNextLine will not return until a newline (/n) is read    */
/* ************************************************************** */
ssize_t Readline(int fd, void *ptr, size_t maxlen)
{
	ssize_t		n;
	if ( (n = GetNextLine(fd, ptr, maxlen)) < 0)
		syslog(LOG_ERR,"GetNextLine error");
	return(n);
}
/* ************************************************************** */
	/* Write "n" bytes to a descriptor. */
/* ************************************************************** */
ssize_t	writen(int fd, const void *vptr, size_t n)
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
/* end writen */
/* ************************************************************** */
void Writen(int fd, void *ptr, size_t nbytes)
{
	if (writen(fd, ptr, nbytes) != nbytes)
		syslog(LOG_ERR,"writen error");
}
