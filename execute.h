/*---------------------------------------------------------------------------*/
/* execute.h                                                                 */
/* Author: Jongki Park, Kyoungsoo Park                                       */
/*---------------------------------------------------------------------------*/

#ifndef _EXECUTE_H_
#define _EXECUTE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "dynarray.h"
#include "util.h"
#include "snush.h"

void redout_handler(char *fname);
void redin_handler(char *fname);
struct CommandInfo
{
    char *redirect_out; // File for output redirection
    int cnt;            // Number of arguments
    char **args;        // Dynamic array of argument pointers
};

int build_command_partial(DynArray_T oTokens, int start, int end, struct CommandInfo *cmd);
int build_command(DynArray_T oTokens, char *args[]);
void execute_builtin(DynArray_T oTokens, enum BuiltinType btype);
int fork_exec(DynArray_T oTokens, int is_background);
int iter_pipe_fork_exec(int pCount, DynArray_T oTokens, int is_background);

struct RedirectionInfo
{
    char *outfile; // Output redirection filename
    char *infile;  // Input redirection filename
    int has_output_redirection;
    int has_input_redirection;
};

#endif /* _EXEUCTE_H_ */