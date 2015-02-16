/*
 * Copyright (c) 2014 Cloud3000.com
 * Author: Michael at Cloud3000.com, a C Programming newbie.
 * cobc -x -lpthread slangio_server.c
 *
 * Special Thanks to Richard Sonnier for helping me with the C programming language.
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
 */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/resource.h>
#include <stdarg.h>      /* ANSI C header file */
#include <syslog.h>      /* for syslog() */
#include <errno.h>
#include <sys/wait.h>
#include "websocket.h"
/* Miscellaneous constants */
#define MAXLINE     4096    /* max text line length */
#define LINE_MAX    4096    /* max text line length */
#define BUFFSIZE    8192    /* buffer size for reads and writes */
#define MAXCONNS    256     /* Number of concurrent connections */
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

void    Pthread_create(pthread_t *, const pthread_attr_t *, void * (*)(void *), void *);
void    Pthread_detach(pthread_t);
void    * Client_Thread(void *arg);
void    error(const char *msg);
int     safeSend(int NewConns, uint8_t buffer, size_t bufferSize);
int     session_init(int sockfd);
void    Close_Thread(int fd);
void    Writen(int, void *, size_t);

#define PORT 8081
#define BUF_LEN 0xFFFF
#define PACKET_DUMP

static int SIO_MAX_FD;

//***************************************************
void Pthread_create(pthread_t *tid, const pthread_attr_t *attr, void * (*func)(void *), void *arg)
{
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
    Pthread_detach(pthread_self());
    session_init((int) arg);
    Close_Thread((int) arg);
    return 0;
}

//***************************************************
void error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

//***************************************************
int safeSend(int NewConns, uint8_t buffer, size_t bufferSize)
{
    #ifdef PACKET_DUMP
    syslog(LOG_INFO,"out packet: %u", buffer);
    fwrite(&buffer, 1, bufferSize, stdout);
    #endif
    ssize_t written = send(NewConns, &buffer, bufferSize, 0);
    if (written == -1) {
        close(NewConns);
        perror("send failed");
        return EXIT_FAILURE;
    }
    if (written != bufferSize) {
        close(NewConns);
        perror("written not all bytes");
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

//***************************************************
int session_init(int sockfd)
{
    char buf[BUF_LEN];
    int i = 0;
    int MAX_Fileno;
    int sid;
    int status;
    pid_t pid0;
    pid_t pid1;
    pid_t w;
    char *argv[1024];
    int stdin[2];
    int stdout[2];
    int stderr[3];
    SIO_MAX_FD = sysconf(_SC_OPEN_MAX);

    syslog(LOG_INFO,"%d starting a session", getpid());
    pid0 = fork();
    if (pid0 == 0) {
        pipe(stdin);
        pipe(stdout);
        pipe(stderr);

        pid1 = fork();
        if (pid1 < 0) {  
            syslog(LOG_CRIT,"Critical condition, can't Fork a process");   
            exit(EXIT_FAILURE);  
        }     
  
        if (pid1 > 0) {  
            pid0 = getpid();
            exit(EXIT_SUCCESS); /*Killing the Parent Process*/  
        }     

        syslog(LOG_INFO," New Session leader %d",getpid());
  
        /* At this point we are executing as the child process */  
  
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
  

        /* Close all other file descriptors greater than stderr*/
        for(i=STDERR_FILENO+1; i < SIO_MAX_FD; i++) { 
            if (i != sockfd) close(i);
            }

        sleep(5);
        sprintf(buf, "Child Slang.IO Server, r1.0 socket=%d\n", sockfd);
        Writen(sockfd, buf, strlen(buf));
        // This is to be the deamon process.
        execvp("/volume1/applications/slangio_main", argv);
    } else {
        sprintf(buf, "Parent Slang.IO Server, r1.0 socket=%d\n", sockfd);
        Writen(sockfd, buf, strlen(buf));
        // Reap death of Child, no zombies please.
        // This is not for the execvp's child directly above.
        // This waitpid call is for the first forked child, who has already died.
        w = waitpid(pid0, &status, WUNTRACED | WCONTINUED);  
        sleep(1);
        syslog(LOG_INFO,"session_init socket %d", sockfd);
        sprintf(buf, "session_init In-Thread: Closing %d\n", sockfd);
        Writen(sockfd, buf, strlen(buf));
        close(sockfd); 
    }
    
} // Thread will terminate, and it's already detacted, so no join is needed.

//***************************************************
void Close_Thread(int fd)
{
    syslog(LOG_INFO,"Close_Thread socket %d", fd);
    return;
}

//***************************************************
int main(int argc, char** argv)
{
    openlog("slangio", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Slang.IO Server started ");
    pthread_t       tid;
    int i = 0;
    int ncptr = 0;
    int NewConns[256];
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

        syslog(LOG_INFO,"connected %s:%d to fd %d\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port), NewConns[ncptr]);

        Pthread_create(&tid, NULL, &Client_Thread, (void *) NewConns[ncptr]);
    }
    
    closelog();
    close(listenSocket);
    return EXIT_SUCCESS;
}
