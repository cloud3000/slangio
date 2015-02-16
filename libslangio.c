#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
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

void LogOpenSystem(char *id) {
    openlog(id, LOG_PID,LOG_USER);
	LOG_CONTROL = 1;
}

void LogDebugOn() {
	setlogmask(LOG_UPTO(LOG_DEBUG));
}

void LogDebugOff() {
	setlogmask(LOG_UPTO(LOG_ERR));
}

void LogEmerg(char *msg, ...) {
	va_list args;
	char fullmsg[LINE_MAX];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_EMERG, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}


void LogAlert(char *msg, ...) {
	va_list args;
	char fullmsg[LINE_MAX];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_ALERT, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}

void LogCrit(char *msg, ...) {
	va_list args;
	char fullmsg[LINE_MAX];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_CRIT, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}

void LogErr(char *msg, ...) {
	va_list args;
	char fullmsg[LINE_MAX];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_ERR, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}

void LogWarning(char *msg, ...) {
	va_list args;
	char fullmsg[LINE_MAX];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_WARNING, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}

void LogNotice(char *msg, ...) {
	va_list args;
	char fullmsg[LINE_MAX];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_NOTICE, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}

void LogInfo(char *msg, ...) {
	va_list args;
	char fullmsg[LINE_MAX];
	va_start(args, msg);
	vsnprintf(fullmsg, sizeof(fullmsg), msg, args);  
	syslog(LOG_INFO, "%s errno=%d (%m)\n", fullmsg, errno);
	va_end(args);
	errno = 0;
}

void LogDebug(char *msg, ...) {
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

