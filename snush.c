/*---------------------------------------------------------------------------*/
/* snush.c                                                                     */
/* Author: Jongki Park, Kyoungsoo Park                                       */
/*---------------------------------------------------------------------------*/

#include "util.h"
#include "token.h"
#include "dynarray.h"
#include "execute.h"
#include "lexsyn.h"
#include "snush.h"

/*
        //
        // TODO-start: global variables in snush.c
        //

        You may add global variables for handling background processes

        //
        // TODO-end: global variables in snush.c
        //
*/

struct BgProcessList bg_list;
int bg_messages_pending = 0;

int total_bg_cnt;

/*---------------------------------------------------------------------------*/
void cleanup()
{
    // Free any allocated memory for background processes
    for (int i = 0; i < bg_list.count; i++)
    {
        if (bg_list.processes[i].cmd != NULL)
        {
            free(bg_list.processes[i].cmd);
        }
    }
    // Reset the background process count
    bg_list.count = 0;
}
/*---------------------------------------------------------------------------*/
void check_bg_status()
{
    if (bg_messages_pending > 0)
    {
        for (int i = 0; i < bg_list.count; i++)
        {
            if (bg_list.processes[i].status == BG_PROCESS_DONE)
            {
                printf("[%d] Background process done\n", bg_list.processes[i].pgid);

                // Remove this process from our tracking
                if (bg_list.processes[i].cmd != NULL)
                {
                    free(bg_list.processes[i].cmd);
                }

                // Shift remaining processes left
                for (int j = i; j < bg_list.count - 1; j++)
                {
                    bg_list.processes[j] = bg_list.processes[j + 1];
                }

                bg_list.count--;
                bg_messages_pending--;
                i--; // Adjust index since we removed an element
            }
        }
    }
}
/*---------------------------------------------------------------------------*/
/* Whenever a child process terminates, this handler handles all zombies. */
static void sigzombie_handler(int signo)
{
    pid_t pid;
    int status;

    if (signo == SIGCHLD)
    {
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        {
            pid_t current_pgid = -1;
            
            // Find the process's pgid
            for (int i = 0; i < bg_list.count; i++)
            {
                if (bg_list.processes[i].pid == pid)
                {
                    current_pgid = bg_list.processes[i].pgid;
                    
                    // Remove this process from the list
                    free(bg_list.processes[i].cmd);
                    for (int j = i; j < bg_list.count - 1; j++)
                    {
                        bg_list.processes[j] = bg_list.processes[j + 1];
                    }
                    bg_list.count--;
                    if (total_bg_cnt > 0)
                        total_bg_cnt--;
                    break;
                }
            }
            
            // Check if this was the last process in the group
            if (current_pgid != -1)
            {
                int remaining = 0;
                for (int i = 0; i < bg_list.count; i++)
                {
                    if (bg_list.processes[i].pgid == current_pgid)
                    {
                        remaining++;
                    }
                }
                
                // Only print Done message if this was the last process in the group
                if (remaining == 0)
                {
                    printf("[%d] Done background process group\n", current_pgid);
                    fflush(stdout);
                }
            }
        }
    }
}
/*---------------------------------------------------------------------------*/
static void shell_helper(const char *in_line)
{
    DynArray_T oTokens;

    enum LexResult lexcheck;
    enum SyntaxResult syncheck;
    enum BuiltinType btype;
    int pcount;
    int ret_pgid; // background pid
    int is_background;

    oTokens = dynarray_new(0);
    if (oTokens == NULL)
    {
        error_print("Cannot allocate memory", FPRINTF);
        exit(EXIT_FAILURE);
    }

    lexcheck = lex_line(in_line, oTokens);
    switch (lexcheck)
    {
    case LEX_SUCCESS:
        if (dynarray_get_length(oTokens) == 0)
            return;

        /* dump lex result when DEBUG is set */
        dump_lex(oTokens);

        syncheck = syntax_check(oTokens);
        if (syncheck == SYN_SUCCESS)
        {
            btype = check_builtin(dynarray_get(oTokens, 0));
            if (btype == NORMAL)
            {
                is_background = check_bg(oTokens);

                pcount = count_pipe(oTokens);

                if (is_background)
                {
                    if (total_bg_cnt + pcount + 1 > MAX_BG_PRO)
                    {
                        printf("Program exceeds the maximum number of the background processes\n");
                        return;
                    }
                }

                if (pcount > 0)
                {
                    ret_pgid = iter_pipe_fork_exec(pcount, oTokens,
                                                   is_background);
                }
                else
                {
                    ret_pgid = fork_exec(oTokens, is_background);
                }

                if (ret_pgid > 0)
                {
                    if (is_background == 1)
                        printf("[%d] Background process running\n",
                               ret_pgid);
                }
                else
                {
                    printf("Invalid return value "
                           "of external command execution\n");
                }
            }
            else
            {
                /* Execute builtin command */
                execute_builtin(oTokens, btype);
            }
        }

        /* syntax error cases */
        else if (syncheck == SYN_FAIL_NOCMD)
            error_print("Missing command name", FPRINTF);
        else if (syncheck == SYN_FAIL_MULTREDOUT)
            error_print("Multiple redirection of standard out", FPRINTF);
        else if (syncheck == SYN_FAIL_NODESTOUT)
            error_print("Standard output redirection without file name",
                        FPRINTF);
        else if (syncheck == SYN_FAIL_MULTREDIN)
            error_print("Multiple redirection of standard input", FPRINTF);
        else if (syncheck == SYN_FAIL_NODESTIN)
            error_print("Standard input redirection without file name",
                        FPRINTF);
        else if (syncheck == SYN_FAIL_INVALIDBG)
            error_print("Invalid use of background", FPRINTF);
        break;

    case LEX_QERROR:
        error_print("Unmatched quote", FPRINTF);
        break;

    case LEX_NOMEM:
        error_print("Cannot allocate memory", FPRINTF);
        break;

    case LEX_LONG:
        error_print("Command is too large", FPRINTF);
        break;

    default:
        error_print("lex_line needs to be fixed", FPRINTF);
        exit(EXIT_FAILURE);
    }

    /* Free memories allocated to tokens */
    dynarray_map(oTokens, free_token, NULL);
    dynarray_free(oTokens);
}
/*---------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    sigset_t sigset;
    char c_line[MAX_LINE_SIZE + 2];

    atexit(cleanup);

    /* Initialize variables for background processes */
    total_bg_cnt = 0;

    /*
        //
        // TODO-start: Initializing in snush.c
        //


         You should initialize or allocate your own global variables
        for handling background processes

        //
        // TODO-end: Initializing in snush.c
        //

    */
    memset(&bg_list, 0, sizeof(struct BgProcessList));
    bg_messages_pending = 0;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGCHLD);
    sigaddset(&sigset, SIGQUIT);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);

    /* Register signal handler */
    signal(SIGINT, SIG_IGN);
    signal(SIGCHLD, sigzombie_handler);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    error_print(argv[0], SETUP);

    while (1)
    {
        check_bg_status();
        fprintf(stdout, "%% ");
        fflush(stdout);

        if (fgets(c_line, MAX_LINE_SIZE, stdin) == NULL)
        {
            printf("\n");
            exit(EXIT_SUCCESS);
        }
        shell_helper(c_line);
    }

    return 0;
}
/*---------------------------------------------------------------------------*/