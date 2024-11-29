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
extern int total_bg_cnt;
extern int prompt_needed;

struct BgProcess
{
        pid_t pid;  // Process ID
        pid_t pgid; // Process group ID
        char *cmd;  // Command string for jobs display
        int status; // Process status
};
struct CompletedProcessGroup
{
        pid_t pgid;
        int printed;
};

struct BgProcessList
{
        struct BgProcess processes[MAX_BG_PRO];
        struct CompletedProcessGroup completed[MAX_BG_PRO];
        int count;
        int completed_count;
};

extern struct BgProcessList bg_list;

// Macros for background process management
#define BG_PROCESS_DONE 1
#define BG_PROCESS_RUNNING 0
#define BG_PROCESS_ERROR -1

#endif /* _SNUSH_H_ */