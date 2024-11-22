/*---------------------------------------------------------------------------*/
/* snush.h                                                                   */
/* Author: Jongki Park, Kyoungsoo Park                                       */
/*---------------------------------------------------------------------------*/

#ifndef _SNUSH_H_
#define _SNUSH_H_

/* SIG_UNBLOCK & sigset_t */
#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_BG_PRO 16
#define MAX_FG_PRO 16

/*
        //
        // TODO-start: data structures in snush.h
        //

        You can add your own data structures to manage the background processes
        You can also add macros to manage background processes

        //
        // TODO-end: data structures in snush.h
        //
*/

struct BgProcess
{
        pid_t pid;         // Process ID
        pid_t pgid;        // Process group ID
        int status;        // Process status
        time_t start_time; // When the process started
        char *cmd;         // Command string
};

// Array to store background processes
struct BgProcessList
{
        struct BgProcess processes[MAX_BG_PRO];
        int count; // Number of active background processes
};

// Macros for background process management
#define BG_PROCESS_DONE 1
#define BG_PROCESS_RUNNING 0
#define BG_PROCESS_ERROR -1

#endif /* _SNUSH_H_ */