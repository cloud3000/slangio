#include "slangio.h"
static pid_t GetMasterPid();
static int libslangio_debug = 0;
static int memfd = -1;
static int LOG_CONTROL = 0;
extern char **environ;

static char rn[] PROGMEM = "\r\n";
void show4bits(char c)
{
    char outstr[5]; // the string that will contain the 1's and 0's
    short b = (short)c;
    int i;
    for (i = 3; i >= 0; --i)
    {
        outstr[i] = (b & (1<<0)) ? '1' : '0';
        b = b >> 1;
    }
    outstr[4]='\0';
    printf("%s\n",outstr);
}

void showuint8(uint8_t b)
{
    char outstr[9]; // the string that will contain the 1's and 0's
    int i;
    for (i = 7; i >= 0; --i)
    {
        outstr[i] = (b & (1<<0)) ? '1' : '0';
        b = b >> 1;
    }
    outstr[8]='\0';
    printf("%s\n",outstr);
}

void showuint16(uint16_t b)
{
    char outstr[17]; // the string that will contain the 1's and 0's
    int i;
    for (i = 15; i >= 0; --i)
    {
        outstr[i] = (b & (1<<0)) ? '1' : '0';
        b = b >> 1;
    }
    outstr[16]='\0';
    printf("%s\n",outstr);
}

void showuint32(uint32_t b)
{
    char outstr[33]; // the string that will contain the 1's and 0's
    int i;
    for (i = 31; i >= 0; --i)
    {
        outstr[i] = (b & (1<<0)) ? '1' : '0';
        b = b >> 1;
    }
    outstr[32]='\0';
    printf("%s",outstr);
}

void showuint64(uint64_t b)
{
    char outstr[65]; // the string that will contain the 1's and 0's
    int i;
    for (i = 63; i >= 0; --i)
    {
        outstr[i] = (b & (1<<0)) ? '1' : '0';
        b = b >> 1;
    }
    outstr[64]='\0';
    printf("%s",outstr);
}

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
