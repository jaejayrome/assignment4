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
	int i, redout = FALSE, redin = FALSE;
	struct Token *t;

	cmd->cnt = 0;
	cmd->redirect_out = NULL;
	cmd->redirect_in = NULL; // Add this field to CommandInfo struct

	// First pass to count arguments
	int arg_count = 0;
	for (i = start; i < end; i++)
	{
		t = dynarray_get(oTokens, i);
		if (t->token_type == TOKEN_WORD)
		{
			if (!(redout == TRUE || redin == TRUE))
			{
				arg_count++;
			}
			redout = FALSE;
			redin = FALSE;
		}
		else if (t->token_type == TOKEN_REDOUT)
		{
			redout = TRUE;
		}
		else if (t->token_type == TOKEN_REDIN)
		{
			redin = TRUE;
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
	redin = FALSE;

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
			else if (redin == TRUE)
			{
				cmd->redirect_in = t->token_value;
				redin = FALSE;
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
		else if (t->token_type == TOKEN_REDIN)
		{
			redin = TRUE;
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

	// Block SIGINT during fork
	sigset_t mask, old_mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigprocmask(SIG_BLOCK, &mask, &old_mask);

	// Save current SIGINT handler
	struct sigaction new_action, old_action;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_handler = SIG_IGN;
	new_action.sa_flags = 0;
	sigaction(SIGINT, &new_action, &old_action);

	if (build_command_partial(oTokens, 0, dynarray_get_length(oTokens), &cmd) < 0)
	{
		error_print("Memory allocation failed", FPRINTF);
		sigaction(SIGINT, &old_action, NULL);
		sigprocmask(SIG_SETMASK, &old_mask, NULL);
		return -1;
	}

	pid = fork();
	if (pid < 0)
	{
		free(cmd.args);
		error_print(NULL, PERROR);
		sigaction(SIGINT, &old_action, NULL);
		sigprocmask(SIG_SETMASK, &old_mask, NULL);
		return -1;
	}

	if (pid == 0)
	{ // Child process
		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_DFL;

		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGQUIT, &sa, NULL);
		sigaction(SIGTSTP, &sa, NULL);
		sigaction(SIGTTIN, &sa, NULL);
		sigaction(SIGTTOU, &sa, NULL);
		sigaction(SIGCHLD, &sa, NULL);

		// Unblock all signals
		sigset_t mask;
		sigemptyset(&mask);
		sigprocmask(SIG_SETMASK, &mask, NULL);

		// Create new process group
		setpgid(0, 0);

		if (cmd.redirect_in != NULL)
		{
			redin_handler(cmd.redirect_in);
		}

		if (cmd.redirect_out != NULL)
		{
			redout_handler(cmd.redirect_out);
		}

		execvp(cmd.args[0], cmd.args);
		error_print(NULL, PERROR);
		free(cmd.args);
		exit(EXIT_FAILURE);
	}
	else
	{ // Parent process
		setpgid(pid, pid);

		if (!is_background)
		{
			// Give terminal control to child
			tcsetpgrp(STDIN_FILENO, pid);

			// Restore original signal mask before waiting
			sigprocmask(SIG_SETMASK, &old_mask, NULL);

			if (waitpid(pid, &status, 0) < 0)
			{
				error_print(NULL, PERROR);
				free(cmd.args);
				sigaction(SIGINT, &old_action, NULL);
				return -1;
			}

			// Restore terminal control to shell
			tcsetpgrp(STDIN_FILENO, getpgrp());
		}
		else
		{
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
	}

	// Restore original signal handlers
	sigaction(SIGINT, &old_action, NULL);
	sigprocmask(SIG_SETMASK, &old_mask, NULL);

	return pid;
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

	// Block SIGTTOU and SIGINT while setting up processes
	sigset_t mask, old_mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGTTOU);
	sigaddset(&mask, SIGINT); // Block SIGINT during setup
	sigprocmask(SIG_BLOCK, &mask, &old_mask);

	// Temporarily install SIGINT handler for parent
	struct sigaction new_action, old_action;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_handler = SIG_IGN;
	new_action.sa_flags = 0;
	sigaction(SIGINT, &new_action, &old_action);

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
				sigaction(SIGINT, &old_action, NULL);
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
			sigaction(SIGINT, &old_action, NULL);
			return -1;
		}

		if (pid == 0)
		{ // Child process
		  // Restore original signal mask and handlers in child
			struct sigaction sa;
			sigemptyset(&sa.sa_mask);
			sa.sa_flags = 0;
			sa.sa_handler = SIG_DFL;

			sigaction(SIGINT, &sa, NULL);
			sigaction(SIGQUIT, &sa, NULL);
			sigaction(SIGTSTP, &sa, NULL);
			sigaction(SIGTTIN, &sa, NULL);
			sigaction(SIGTTOU, &sa, NULL);
			sigaction(SIGCHLD, &sa, NULL);

			// Unblock all signals
			sigset_t mask;
			sigemptyset(&mask);
			sigprocmask(SIG_SETMASK, &mask, NULL);

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

			// Close all other file descriptors
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

			// Handle redirection for last command
			if (i == cmd_count - 1 && cmd.redirect_out != NULL)
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

			// Give terminal control to the process group if foreground
			if (!is_background && i == 0)
			{
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

	// Parent process cleanup and waiting
	if (!is_background)
	{
		int status;

		// Restore original signal handling before waiting
		sigprocmask(SIG_SETMASK, &old_mask, NULL);

		for (i = 0; i < cmd_count; i++)
		{
			if (waitpid(child_pids[i], &status, 0) < 0)
			{
				if (errno != ECHILD)
				{
					error_print(NULL, PERROR);
					sigaction(SIGINT, &old_action, NULL);
					return -1;
				}
			}
		}

		// Restore terminal control to shell
		tcsetpgrp(STDIN_FILENO, getpgrp());
	}
	else
	{
		// Background process handling remains the same
		for (i = 0; i < cmd_count; i++)
		{
			if (bg_list.count < MAX_BG_PRO)
			{
				bg_list.processes[bg_list.count].pid = child_pids[i];
				bg_list.processes[bg_list.count].pgid = pgid;
				bg_list.processes[bg_list.count].status = BG_PROCESS_RUNNING;

				struct CommandInfo cmd = {0};
				build_command_partial(oTokens, token_start, token_end, &cmd);
				bg_list.processes[bg_list.count].cmd = strdup(cmd.args[0]);
				free(cmd.args);

				bg_list.count++;
				total_bg_cnt++;
			}
		}
	}

	// Restore original signal handlers
	sigaction(SIGINT, &old_action, NULL);
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