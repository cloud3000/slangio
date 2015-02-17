//#include "websocket.h"
#include <arpa/inet.h>
#include <ctype.h> 
#include <dirent.h>
#include <errno.h>
#include <ev.h>
#include <fcntl.h>
#include <grp.h>
#include <libgen.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h> 
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <syslog.h> 
#include <time.h>
#include <unistd.h>
/*-----------------------------------------------------------
 * Data Structures
 ----------------------------------------------------------*/
#define READ_END 0
#define WRITE_END 1
#define TRUE 1
#define	BUFFSIZE	16384	/* buffer size for reads and writes */
#define	MAXLINE		4096	/* max text line length */
#define MAX_CMD_ARGS 32
#define ARGSMAX 1024
#define MAX_CONTROL_BLOCKS 4096
#define MAXCONNS    256     /* Number of concurrent connections */
#define PERIOD "."
#define RESERVED_BLOCKS 512
#define SIO_ABORT_PROGRAM "/volume1/applications/abortmsg"
#define SIO_APP_HOME "/volume1/applications/"
#define SIO_APPL_SECURITY "/volume1/applications/appsec"
#define SIO_APPL_MAIN "/volume1/applications/appmain"
#define SIO_CHILD_BLOCKS (MAX_CONTROL_BLOCKS - RESERVED_BLOCKS)
#define SIO_DEBUG "debug"
#define SIO_DEFAULT_USER "j3k"
#define SIO_FIFO_FMT "/volume1/applications/j3k/mycntl%08d"
#define SIO_HISTSIZE 1024
#define SIO_INIT 0
#define SIO_INIT_MSG "Ready"
#define SIO_LOCK "lock"
#define SIO_LOCKS_FMT "%s/.%08d/locks"
#define SIO_LOGIN_VERSION 0.01
#define SIO_MAIN "SIO_MAIN=%d"
#define SIO_MSG_LEN 32
#define SIO_PERM_FILE_DIR "/volume1/perm"
#define SIO_PID_FMT "/volume1/applications/j3k/mysess%08d"
#define SIO_PLOCK_FMT "%s/%s"
#define SIO_PROFILE "/volume1/applications/mylogin_profile.sh"
#define SIO_RUN "run"
#define SIO_RUNCBL "runcbl"
#define SIO_RUNCBL_DEBUG "runcbl -d -b +o stdlist_o +ee stdlist_e "
#define SIO_RUNCBL_ERRFILE "stdlist_e"
#define SIO_RUNCBL_STD "runcbl -b +o stdlist_o +ee stdlist_e "
#define SIO_SEND "client send"
#define SIO_SESSION_FMT "%s/.%08d"
#define SIO_SESSION_TIME "times"
#define SIO_SESSION_VAR_MAX 1024
#define SIO_SESSION_VARLEN_MAX 255
#define SIO_SHELL "/bin/bash"
#define SIO_SQL_PATH "/volume1/lib/sql"
#define SIO_STATUS "status"
#define SIO_STOP "stop"
#define SIO_TCL_PATH "/volume1/lib/tcl"
#define SIO_UNLOCK "unlock"
#define SIO_VPLUS_TCL "tcl_libfunc.tcl"
#define STDERR_FILENO 2
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define WHITESPACE " \t\n\r"

extern void LogOpenSystem(char *id);
extern void LogEmerg(char *msg, ...);
extern void LogAlert(char *msg, ...);
extern void LogCrit(char *msg, ...);
extern void LogErr(char *msg, ...);
extern void LogWarning(char *msg, ...);
extern void LogNotice(char *msg, ...);
extern void LogInfo(char *msg, ...);
extern void LogDebug(char *msg, ...);
extern void LogConsole(char *msg);
extern void LogClose();
extern void LogDebugOn();
extern void LogDebugOff();
extern void base64enc(char* dest, const void* src, uint16_t length);
void PutEnv (char *format, ...);
ssize_t	 Readline(int, void *, size_t);
void	 Writen(int, void *, size_t);
