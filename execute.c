/*---------------------------------------------------------------------------*/
/* execute.c                                                                 */
/* Author: Jongki Park, Kyoungsoo Park                                       */
/*---------------------------------------------------------------------------*/

#include "dynarray.h"
#include "token.h"
#include "util.h"
#include "lexsyn.h"
#include "snush.h"
#include "execute.h"
#include <termios.h>

extern int total_bg_cnt;
extern struct BgProcessList bg_list;
extern int total_bg_cnt;
/*---------------------------------------------------------------------------*/
void redout_handler(char *fname)
{
	int fd;

	fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
	{
		error_print(NULL, PERROR);
		exit(EXIT_FAILURE);
	}
	else
	{
		dup2(fd, STDOUT_FILENO);
		close(fd);
	}
}
/*---------------------------------------------------------------------------*/
void redin_handler(char *fname)
{
	int fd;

	fd = open(fname, O_RDONLY);
	if (fd < 0)
	{
		error_print(NULL, PERROR);
		exit(EXIT_FAILURE);
	}
	else
	{
		dup2(fd, STDIN_FILENO);
		close(fd);
	}
}
/*---------------------------------------------------------------------------*/

int build_command_partial(DynArray_T oTokens, int start, int end, struct CommandInfo *cmd)
{
	int i, redout = FALSE; // Removed unused redin variable
	struct Token *t;

	cmd->cnt = 0;
	cmd->redirect_out = NULL;

	// First pass to count arguments
	int arg_count = 0;
	for (i = start; i < end; i++)
	{
		t = dynarray_get(oTokens, i);
		if (t->token_type == TOKEN_WORD)
		{
			if (!(redout == TRUE))
			{
				arg_count++;
			}
			redout = FALSE;
		}
		else if (t->token_type == TOKEN_REDOUT)
		{
			redout = TRUE;
		}
	}

	// Allocate space for arguments plus NULL terminator
	cmd->args = malloc(sizeof(char *) * (arg_count + 1));
	if (cmd->args == NULL)
	{
		return -1;
	}

	// Reset flags for second pass
	redout = FALSE;

	// Second pass to fill in arguments
	for (i = start; i < end; i++)
	{
		t = dynarray_get(oTokens, i);

		if (t->token_type == TOKEN_WORD)
		{
			if (redout == TRUE)
			{
				cmd->redirect_out = t->token_value;
				redout = FALSE;
			}
			else
			{
				cmd->args[cmd->cnt++] = t->token_value;
			}
		}
		else if (t->token_type == TOKEN_REDOUT)
		{
			redout = TRUE;
		}
	}
	cmd->args[cmd->cnt] = NULL;
	return 0;
}
/*---------------------------------------------------------------------------*/
int build_command(DynArray_T oTokens, char *args[])
{
	struct CommandInfo cmd = {0};
	int ret = build_command_partial(oTokens, 0, dynarray_get_length(oTokens), &cmd);

	if (ret == 0)
	{
		// Copy arguments to the provided array
		for (int i = 0; i <= cmd.cnt; i++)
		{ // Include NULL terminator
			args[i] = cmd.args[i];
		}
		free(cmd.args); // Free the temporary array
	}

	return ret;
}
/*---------------------------------------------------------------------------*/
void execute_builtin(DynArray_T oTokens, enum BuiltinType btype)
{
	int ret;
	char *dir = NULL;
	struct Token *t1;

	switch (btype)
	{
	case B_EXIT:
		if (dynarray_get_length(oTokens) == 1)
		{
			// printf("\n");
			dynarray_map(oTokens, free_token, NULL);
			dynarray_free(oTokens);

			exit(EXIT_SUCCESS);
		}
		else
			error_print("exit does not take any parameters", FPRINTF);

		break;

	case B_CD:
		if (dynarray_get_length(oTokens) == 1)
		{
			dir = getenv("HOME");
			if (dir == NULL)
			{
				error_print("cd: HOME variable not set", FPRINTF);
				break;
			}
		}
		else if (dynarray_get_length(oTokens) == 2)
		{
			t1 = dynarray_get(oTokens, 1);
			if (t1->token_type == TOKEN_WORD)
				dir = t1->token_value;
		}

		if (dir == NULL)
		{
			error_print("cd takes one parameter", FPRINTF);
			break;
		}
		else
		{
			ret = chdir(dir);
			if (ret < 0)
				error_print(NULL, PERROR);
		}
		break;

	default:
		error_print("Bug found in execute_builtin", FPRINTF);
		exit(EXIT_FAILURE);
	}
}
/*---------------------------------------------------------------------------*/
/* Important Notice!!
	Add "signal(SIGINT, SIG_DFL);" after fork (only to child process)
*/
int fork_exec(DynArray_T oTokens, int is_background)
{
	pid_t pid;
	int status;
	struct CommandInfo cmd = {0};

	if (build_command_partial(oTokens, 0, dynarray_get_length(oTokens), &cmd) < 0)
	{
		error_print("Memory allocation failed", FPRINTF);
		return -1;
	}

	pid = fork();
	if (pid < 0)
	{
		free(cmd.args);
		error_print(NULL, PERROR);
		return -1;
	}

	if (pid == 0)
	{ // Child process
		signal(SIGINT, SIG_DFL);
		setpgid(0, 0); // Create new process group

		if (cmd.redirect_out != NULL)
		{
			int fd = open(cmd.redirect_out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (fd < 0)
			{
				error_print(NULL, PERROR);
				free(cmd.args);
				exit(EXIT_FAILURE);
			}
			if (dup2(fd, STDOUT_FILENO) < 0)
			{
				error_print(NULL, PERROR);
				close(fd);
				free(cmd.args);
				exit(EXIT_FAILURE);
			}
			close(fd);
		}

		execvp(cmd.args[0], cmd.args);
		error_print(NULL, PERROR);
		free(cmd.args);
		exit(EXIT_FAILURE);
	}
	else
	{ // Parent process in fork_exec()
		if (!is_background)
		{
			if (waitpid(pid, &status, 0) < 0)
			{
				error_print(NULL, PERROR);
				free(cmd.args);
				return -1;
			}
		}
		else
		{
			setpgid(pid, pid);
			if (bg_list.count < MAX_BG_PRO)
			{
				fflush(stdout);

				bg_list.processes[bg_list.count].pid = pid;
				bg_list.processes[bg_list.count].pgid = pid;
				bg_list.processes[bg_list.count].status = BG_PROCESS_RUNNING;
				bg_list.processes[bg_list.count].cmd = strdup(cmd.args[0]);
				bg_list.count++;
				total_bg_cnt++;
			}
		}
		free(cmd.args);
		return pid;
	}
	return -1;
}
/*---------------------------------------------------------------------------*/
/* Important Notice!!
	Add "signal(SIGINT, SIG_DFL);" after fork (only to child process)
*/
int iter_pipe_fork_exec(int pcount, DynArray_T oTokens, int is_background)
{
	int i, token_start, token_end;
	int pipe_fds[2];
	int prev_pipe_read = -1;
	pid_t pid, first_child_pid = -1;
	int cmd_count = pcount + 1;
	int token_idx = 0;
	int pgid = -1;
	pid_t child_pids[MAX_FG_PRO];

	// Block SIGTTOU while setting up terminal control
	sigset_t mask, old_mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGTTOU);
	sigprocmask(SIG_BLOCK, &mask, &old_mask);

	for (i = 0; i < cmd_count; i++)
	{
		token_start = token_idx;
		while (token_idx < dynarray_get_length(oTokens))
		{
			struct Token *t = dynarray_get(oTokens, token_idx);
			if (t->token_type == TOKEN_PIPE)
				break;
			token_idx++;
		}
		token_end = token_idx;
		token_idx++;

		if (i < cmd_count - 1)
		{
			if (pipe(pipe_fds) < 0)
			{
				error_print(NULL, PERROR);
				for (int j = 0; j < i; j++)
				{
					kill(child_pids[j], SIGTERM);
				}
				sigprocmask(SIG_SETMASK, &old_mask, NULL);
				return -1;
			}
		}

		pid = fork();

		if (pid < 0)
		{
			error_print(NULL, PERROR);
			for (int j = 0; j < i; j++)
			{
				kill(child_pids[j], SIGTERM);
			}
			sigprocmask(SIG_SETMASK, &old_mask, NULL);
			return -1;
		}

		if (pid == 0)
		{ // Child process
			// Restore original signal mask in child
			sigprocmask(SIG_SETMASK, &old_mask, NULL);

			// Reset signal handlers
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
			signal(SIGTSTP, SIG_DFL);
			signal(SIGTTIN, SIG_DFL);
			signal(SIGTTOU, SIG_DFL);

			if (pgid == -1)
			{
				pgid = getpid();
			}
			setpgid(0, pgid);

			if (prev_pipe_read != -1)
			{
				dup2(prev_pipe_read, STDIN_FILENO);
				close(prev_pipe_read);
			}

			if (i < cmd_count - 1)
			{
				close(pipe_fds[0]);
				dup2(pipe_fds[1], STDOUT_FILENO);
				close(pipe_fds[1]);
			}

			for (int j = 3; j < 256; j++)
			{
				close(j);
			}

			struct CommandInfo cmd = {0};
			if (build_command_partial(oTokens, token_start, token_end, &cmd) < 0)
			{
				error_print("Command building failed", FPRINTF);
				exit(EXIT_FAILURE);
			}

			execvp(cmd.args[0], cmd.args);
			free(cmd.args);
			error_print(NULL, PERROR);
			exit(EXIT_FAILURE);
		}
		else
		{ // Parent process
			child_pids[i] = pid;

			if (pgid == -1)
			{
				pgid = pid;
				first_child_pid = pid;
			}
			setpgid(pid, pgid);

			if (!is_background && i == 0)
			{
				// Set process group as foreground
				tcsetpgrp(STDIN_FILENO, pgid);
			}

			if (prev_pipe_read != -1)
			{
				close(prev_pipe_read);
			}

			if (i < cmd_count - 1)
			{
				close(pipe_fds[1]);
				prev_pipe_read = pipe_fds[0];
			}
		}
	}

	if (!is_background)
	{
		int status;
		for (i = 0; i < cmd_count; i++)
		{
			pid_t waited_pid = waitpid(child_pids[i], &status, 0);
			if (waited_pid < 0)
			{
				if (errno != ECHILD)
				{
					error_print(NULL, PERROR);
					sigprocmask(SIG_SETMASK, &old_mask, NULL);
					return -1;
				}
			}
		}
		// Return terminal control to shell
		tcsetpgrp(STDIN_FILENO, getpgrp());
	}
	else
	{
		for (i = 0; i < cmd_count; i++)
		{
			if (bg_list.count < MAX_BG_PRO)
			{
				// Add each process in the pipeline to bg_list
				bg_list.processes[bg_list.count].pid = child_pids[i];
				bg_list.processes[bg_list.count].pgid = pgid; // Use the same pgid for all
				bg_list.processes[bg_list.count].status = BG_PROCESS_RUNNING;

				// Get command for this process
				struct CommandInfo cmd = {0};
				int token_start_pos = 0;
				int pipe_count = 0;
				for (int j = 0; j < dynarray_get_length(oTokens); j++)
				{
					struct Token *t = dynarray_get(oTokens, j);
					if (t->token_type == TOKEN_PIPE)
					{
						if (pipe_count == i)
						{
							break;
						}
						token_start_pos = j + 1;
						pipe_count++;
					}
				}
				build_command_partial(oTokens, token_start_pos, token_end, &cmd);
				bg_list.processes[bg_list.count].cmd = strdup(cmd.args[0]);
				free(cmd.args);

				bg_list.count++;
				total_bg_cnt++;
			}
		}
	}

	// Restore original signal mask
	sigprocmask(SIG_SETMASK, &old_mask, NULL);
	return first_child_pid;
}
/*---------------------------------------------------------------------------*/
void print_jobs(void)
{
	for (int i = 0; i < bg_list.count; i++)
	{
		if (bg_list.processes[i].status == BG_PROCESS_RUNNING)
		{
			// Check if process still exists
			if (kill(bg_list.processes[i].pid, 0) == 0)
			{
				printf("[%d] Running\t%s\n",
					   bg_list.processes[i].pid,
					   bg_list.processes[i].cmd);
			}
		}
	}
}