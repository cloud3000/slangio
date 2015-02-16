#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h> 
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/times.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <syslog.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <zlog.h>
#include <libgen.h>

/*-----------------------------------------------------------
 * Data Structures
 ----------------------------------------------------------*/
#define MAX_CONTROL_BLOCKS 4096
#define RESERVED_BLOCKS 512
#define SIO_CHILD_BLOCKS (MAX_CONTROL_BLOCKS - RESERVED_BLOCKS)
#define MAX_CMD_ARGS 32
#define WHITESPACE " \t\n\r"
#define PERIOD "."
#define SIO_PID_FMT "/volume1/applications/j3k/mysess%08d"
#define SIO_FIFO_FMT "/volume1/applications/j3k/mycntl%08d"
#define SIO_SESSION_FMT "%s/.%08d"
#define SIO_LOCKS_FMT "%s/.%08d/locks"
#define SIO_PLOCK_FMT "%s/%s"
#define SIO_LOGIN_VERSION "1.0RC"
#define SIO_MSG_LEN 32
#define SIO_INIT 0
#define SIO_INIT_MSG "Ready"
#define SIO_STOP "stop"
#define SIO_SEND "client send"
#define SIO_RUN "run"
#define SIO_RUNCBL "runcbl"
#define SIO_DEBUG "debug"
#define SIO_LOCK "lock"
#define SIO_UNLOCK "unlock"
#define SIO_STATUS "status"
#define SIO_SESSION_TIME "times"
#define SIO_RUNCBL_STD "runcbl -b +o stdlist_o +ee stdlist_e "
#define SIO_RUNCBL_DEBUG "runcbl -d -b +o stdlist_o +ee stdlist_e "
#define SIO_RUNCBL_ERRFILE "stdlist_e"
#define SIO_CHECK_SEC "/volume1/applications/jssecurity"
#define SIO_SHELL "/bin/bash"
#define SIO_HISTSIZE 1024
#define SIO_PROFILE "/volume1/applications/mylogin_profile.sh"
#define SIO_DEFAULT_USER "j3k"
#define SIO_MAIN "SIO_MAIN=%d"
#define SIO_SESSION_VAR_MAX 1024
#define SIO_SESSION_VARLEN_MAX 255
#define SIO_SOCKET_FD 86
#define SIO_TCL_PATH "/volume1/lib/tcl"
#define SIO_VPLUS_TCL "tcl_libfunc.tcl"
#define SIO_SQL_PATH "/volume1/lib/sql"
#define SIO_PERM_FILE_DIR "/volume1/perm"
#define SIO_ABORT_PROGRAM "/volume1/applications/jsabortmsg"
#define SIO_APP_HOME "/volume1/applications/"

void LogOpenSystem(char *id);
void LogEmerg(char *msg, ...);
void LogAlert(char *msg, ...);
void LogCrit(char *msg, ...);
void LogErr(char *msg, ...);
void LogWarning(char *msg, ...);
void LogNotice(char *msg, ...);
void LogInfo(char *msg, ...);
void LogDebug(char *msg, ...);
void LogConsole(char *msg);
extern void LogClose();
void LogDebugOn();
void LogDebugOff();

/*-----------------------------------------------------------
 * Environment Variables
 ----------------------------------------------------------*/
void PutEnv (char *format, ...);
