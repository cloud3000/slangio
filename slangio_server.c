/*
 * Copyright (c) 2014 Cloud3000.com
 * Author: Michael at Cloud3000.com
 *
 * cobc -g -x -lpthread slangio_server.c libslangio.c
 *
 * Special Thanks to Richard Sonnier of Nimble Services,
 * for helping me with the C programming language.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 *  TODO's
 *  some pre-process initialization for session control-block storage
 *  maybe add event based signaling for messaging, for commands to shutdown or retrieve info.
 *  
 *
 */
#include "slangio.h" 
#include <pthread.h>

void    Pthread_create(pthread_t *, const pthread_attr_t *, void * (*)(void *), void *);
void    Pthread_detach(pthread_t);
void    * Client_Thread(void *arg);
void    error(const char *msg);
int     session_init(void *arg);
void    Close_Thread(void *arg);

#define PORT 8081

static int SIO_MAX_FD;

//***************************************************
void Pthread_create(pthread_t *tid, const pthread_attr_t *attr, void * (*func)(void *), void *arg)
{
    int *x = (int*)arg;
    printf("Pthread_create received arg  %d at address %p\n", *x, arg );
    int     n;
    if ( (n = pthread_create(tid, attr, func, arg)) == 0)
        return;
    errno = n;
    syslog(LOG_INFO,"pthread_create error\n");
}

//***************************************************
void Pthread_detach(pthread_t tid)
{
    int     n;
    if ( (n = pthread_detach(tid)) == 0)
        return;
    errno = n;
    syslog(LOG_INFO,"pthread_detach error\n");
}

//***************************************************
void * Client_Thread(void *arg)
{
    int *x = (int*)arg;
    printf("Client_Thread received arg %d at address %p\n",*x, arg );

    Pthread_detach(pthread_self());
    session_init(arg); // spawn a new daemon, to remain in servitude to client.
    Close_Thread(arg);
    return 0;
}

//***************************************************
void error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

//***************************************************
int session_init(void *arg)
{
    int *fd = (int*)arg;
    int sockfd = (int)*fd;
    printf("session_init received arg %d at address %p\n",*fd, arg );

    char buf[BUFFSIZE];
    int i = 0;
    int sid;
    int status;
    pid_t pid0;
    pid_t pid1;
    pid_t w;
    char *argv[ARGSMAX];
    SIO_MAX_FD = sysconf(_SC_OPEN_MAX);

    syslog(LOG_INFO,"%d starting a session", getpid());
    pid0 = fork();
    if (pid0 == 0) {

        pid1 = fork();  // Double-fork.. creating a deamon.
                        // pid0 will exit imediately, orphan pid1
                        // pid1 will become an independent session leader, slangio_main.
        if (pid1 < 0) {  
            syslog(LOG_CRIT,"Critical condition, can't Fork a process");   
            exit(EXIT_FAILURE);  
        }     
  
        if (pid1 > 0) {  
            exit(EXIT_SUCCESS); /* Killing pid0, leaving pid1 to go it alone.
                                    hopefully pid1 will be adopted by init. */  
        }     

        syslog(LOG_INFO," New Session leader %d",getpid());
  
        /* At this point we are executing as the child process pid1 */  
  
        /* Create a new SID for the child process */  
        sid = setsid();  
        if (sid < 0)    
        {
            syslog(LOG_ERR,"setsid failed, sid=%d", sid);  
            exit(EXIT_FAILURE);  
        }  
        
        /* Change the current working directory. */  
        if (chdir("/") < 0) {  
             exit(EXIT_FAILURE);  
        }

         /* Change the umask. */  
        if (umask(027) < 0) {  
            exit(EXIT_FAILURE);  
        }
  
        /* Close all other file descriptors, except for the socket to the client.
            When slangio_main is executed, it will use all 3 standard I/O files. */

        for(i=0; i < SIO_MAX_FD; i++) { 
            if (i != sockfd) close(i); // slangio_main will need sockfd.
            }

        /* redirect stdin, stdout, and stderr to /dev/null 
            However, slangio_main will close & redirect standard I/O for it's own children */
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_RDWR);
        open("/dev/null", O_RDWR);
        execvp("/volume1/applications/slangio_main", argv);
    } else {
        w = waitpid(pid0, &status, WUNTRACED | WCONTINUED);  // reap pid0
        syslog(LOG_INFO,"session_init socket %d", sockfd);
        // Now we can release sockfd, to be reused.
        close(sockfd); 
    }

} /* Thread will terminate, and it's already detacted, so no join is needed.

      NOTE:   This process blocks only for new connections, and nothing else.
              I use this thread to fork a new session, so I can quickly get
              back to listening for new connections.
              
              Trying to keep it simple, no events or signals. */

//***************************************************
void Close_Thread(void *arg)
{
    int *fd = (int*)arg;
    printf("Close_Thread received arg %d at address %p\n",*fd, arg );
    syslog(LOG_INFO,"Close_Thread socket=%d", *fd);
    memset(arg,0,sizeof(int));
    return;
}

//***************************************************
int main(int argc, char** argv)
{
    openlog("slangio", LOG_PID, LOG_USER);
    //
    //
    // Start Security deamon
    //
    // Start Spooler deamon 
    //
    // Start Message deamon
    //
    //
    syslog(LOG_INFO, "Slang.IO Server started ");
    pthread_t       tid;
    int i = 0;
    int ncptr = 0;
    int NewConns[MAXCONNS];
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == -1) {
        syslog(LOG_ERR,"create socket failed");
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(PORT);
    if (bind(listenSocket, (struct sockaddr *) &local, sizeof(local)) == -1) {
        syslog(LOG_ERR,"bind failed on port %d", local.sin_port);
        exit(EXIT_FAILURE);
    }
    
    if (listen(listenSocket, 1) == -1) {
        syslog(LOG_ERR,"listen failed on port %d", local.sin_port);
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO,"listening on %s:%d\n", inet_ntoa(local.sin_addr), ntohs(local.sin_port));
    
    while (TRUE) {
        struct sockaddr_in remote;
        socklen_t sockaddrLen = sizeof(remote);
        for (i = 0; i < MAXCONNS; ++i)
        {
            if (NewConns[i] == 0)
            {
                ncptr = i;
                break;
            }
        }
        NewConns[ncptr] = accept(listenSocket, (struct sockaddr*)&remote, &sockaddrLen);
        if (NewConns[ncptr] == -1) {
            syslog(LOG_ERR,"accept failed");
        }

        syslog(LOG_INFO,"connected %s:%d to fd %d\n",
         inet_ntoa(remote.sin_addr), ntohs(remote.sin_port), NewConns[ncptr]);

        Pthread_create(&tid, NULL, &Client_Thread, (void *) &NewConns[ncptr]);
    }
    
    closelog();
    close(listenSocket);
    return EXIT_SUCCESS;
}
    