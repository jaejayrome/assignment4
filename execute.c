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

extern int total_bg_cnt;
/*---------------------------------------------------------------------------*/
void redout_handler(char *fname)
{
	printf("DEBUG: Starting redout_handler for %s\n", fname); // Add this
	int fd;

	fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
	{
		error_print(NULL, PERROR);
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("DEBUG: Redirecting stdout to file\n"); // Add this
		dup2(fd, STDOUT_FILENO);
		close(fd);
	}
	printf("DEBUG: Finished redout_handler\n"); // Add this
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

	// Just collect command info, don't execute redirection yet
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

		// Handle redirection only in child process
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

		// Execute command
		execvp(cmd.args[0], cmd.args);
		error_print(NULL, PERROR);
		free(cmd.args);
		exit(EXIT_FAILURE);
	}
	else
	{ // Parent process
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
	pid_t child_pids[MAX_FG_PRO]; // Store all child PIDs

	// Iterate through all commands in the pipeline
	for (i = 0; i < cmd_count; i++)
	{
		// Find start and end of current command
		token_start = token_idx;
		while (token_idx < dynarray_get_length(oTokens))
		{
			struct Token *t = dynarray_get(oTokens, token_idx);
			if (t->token_type == TOKEN_PIPE)
				break;
			token_idx++;
		}
		token_end = token_idx;
		token_idx++; // Skip the pipe token

		// Create pipe for all except last command
		if (i < cmd_count - 1)
		{
			if (pipe(pipe_fds) < 0)
			{
				error_print(NULL, PERROR);
				// Clean up previously created children
				for (int j = 0; j < i; j++)
				{
					kill(child_pids[j], SIGTERM);
				}
				return -1;
			}
		}

		// Fork new process
		pid = fork();

		if (pid < 0)
		{
			error_print(NULL, PERROR);
			// Clean up
			for (int j = 0; j < i; j++)
			{
				kill(child_pids[j], SIGTERM);
			}
			return -1;
		}

		if (pid == 0)
		{ // Child process
			signal(SIGINT, SIG_DFL);

			// Set up process group
			if (pgid == -1)
			{
				pgid = getpid();
			}
			setpgid(0, pgid);

			// Handle input from previous pipe
			if (prev_pipe_read != -1)
			{
				dup2(prev_pipe_read, STDIN_FILENO);
				close(prev_pipe_read);
			}

			// Handle output to next pipe
			if (i < cmd_count - 1)
			{
				close(pipe_fds[0]); // Close read end
				dup2(pipe_fds[1], STDOUT_FILENO);
				close(pipe_fds[1]);
			}

			// Close all unnecessary file descriptors
			for (int j = 3; j < 256; j++)
			{
				close(j);
			}

			// Build and execute command using new CommandInfo structure
			struct CommandInfo cmd = {0};
			if (build_command_partial(oTokens, token_start, token_end, &cmd) < 0)
			{
				error_print("Command building failed", FPRINTF);
				exit(EXIT_FAILURE);
			}

			// Execute command
			execvp(cmd.args[0], cmd.args);

			// If execvp returns, it must have failed
			free(cmd.args); // Clean up before error
			error_print(NULL, PERROR);
			exit(EXIT_FAILURE);
		}
		else
		{ // Parent process
			// Store PID
			child_pids[i] = pid;

			// Set up process group
			if (pgid == -1)
			{
				pgid = pid;
				first_child_pid = pid; // Store first child's PID
			}
			setpgid(pid, pgid);

			// Close unused pipe ends
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

	// Parent process waiting
	if (!is_background)
	{
		int status;
		for (i = 0; i < cmd_count; i++)
		{
			pid_t waited_pid = waitpid(child_pids[i], &status, 0);
			if (waited_pid < 0)
			{
				if (errno != ECHILD)
				{ // Ignore "No child processes" error
					error_print(NULL, PERROR);
					return -1;
				}
			}
		}
	}
	else
	{
		// For background processes, increment counter
		total_bg_cnt += cmd_count;
	}

	return first_child_pid;
}
/*---------------------------------------------------------------------------*/