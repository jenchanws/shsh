#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <mcheck.h>
#include <pwd.h>

#include "parser.h"
#include "shell.h"

/**
 * Program that simulates a simple shell.
 * The shell covers basic commands, including builtin commands 
 * (cd and exit only), standard I/O redirection and piping (|). 
 
 */

#define MAX_DIRNAME 100
#define MAX_HOSTNAME 64
#define MAX_COMMAND 1024
#define MAX_TOKEN 128

/* Functions to implement, see below after main */
int execute_cd(char** words);
int execute_nonbuiltin(simple_command *s);
int execute_simple_command(simple_command *cmd);
int execute_complex_command(command *cmd);

int execute_set(char **words);
int execute_unset(char **words);

void print_prompt(void);

int main(int argc, char** argv) {
	
	char command_line[MAX_COMMAND];  /* The command */
	char *tokens[MAX_TOKEN];         /* Command tokens (program name, 
					  * parameters, pipe, etc.) */

	while (1) {

		/* Display prompt */		
		print_prompt();

		/* Read the command line */
		fgets(command_line, MAX_COMMAND, stdin);
		/* Strip the new line character */
		if (command_line[strlen(command_line) - 1] == '\n') {
			command_line[strlen(command_line) - 1] = '\0';
		}
		
		/* Parse the command into tokens */
		parse_line(command_line, tokens);
		process_tokens(tokens);

		/* Check for empty command */
		if (!(*tokens)) {
			continue;
		}
		
		/* Construct chain of commands, if multiple commands */
		command *cmd = construct_command(tokens);
		//print_command(cmd, 0);

		int exitcode = 0;
		if (cmd->scmd) {
			exitcode = execute_simple_command(cmd->scmd);
			if (exitcode == -1) {
				break;
			}
		}
		else {
			exitcode = execute_complex_command(cmd);
			if (exitcode == -1) {
				break;
			}
		}
		release_command(cmd);
	}
    
	return 0;
}


/**
 * Changes directory to a path specified in the words argument;
 * For example: words[0] = "cd"
 *              words[1] = "csc209/assignment3/"
 * Your command should handle both relative paths to the current 
 * working directory, and absolute paths relative to root,
 * e.g., relative path:  cd csc209/assignment3/
 *       absolute path:  cd /u/bogdan/csc209/assignment3/
 */
int execute_cd(char** words) {
	/* Check that 'words' is a valid string of tokens, i.e.
	 * it exists and the first one is "cd". */
	if (words == NULL ||
		words[0] == NULL ||
		strcmp(words[0], "cd"))
		return EXIT_FAILURE;

	/* If we only have 1 token "cd", change to the user's home directory. */
	char *dir = words[1] ? words[1] : getenv("HOME");

	/* If the command is 'cd -', return to the previous working directory. */
	if (!strcmp(dir, "-"))
		dir = getenv("OLDPWD");

	/* Change the directory, returning -1 if it fails. */
	int ret = chdir(dir);
	if (ret == -1) {
		perror("cd");
		return 1;
	}
	return ret;
}

/* Sets an environment variable specified in the words argument:
 * For example: words[0] = 'set'
 *              words[1] = 'PROMPT'
 *              words[2] = '"$ "'
 */
int execute_set(char **words) {
	/* Check that 'words' is a valid string of tokens, i.e.
	 * it exists and the first one is "set". */
	if (words == NULL ||
		words[0] == NULL ||
		strcmp(words[0], "set"))
		return EXIT_FAILURE;

	/* If we don't have a variable name (or a value), do nothing. */
	if (!words[1])
		return EXIT_SUCCESS;

	/* Get the variable name and value. */
	char *name = words[1], *value = words[2];
	/* If we don't have a value, just print the value of the variable. */
	if (!value) {
		value = getenv(name);
		if (value)
			printf("%s = %s\n", name, value);
		else
			printf("%s is not set.\n", name);
		return EXIT_SUCCESS;
	}
	if (setenv(name, value, 1) == -1) {
		perror("setenv");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}


/* Unsets an environment variable specified in the words argument:
 * For example: words[0] = 'unset'
 *              words[1] = 'PROMPT'
 */
int execute_unset(char **words) {
	/* Check that 'words' is a valid string of tokens, i.e.
	 * it exists and the first one is "unset". */
	if (words == NULL ||
		words[0] == NULL ||
		strcmp(words[0], "unset"))
		return EXIT_FAILURE;

	/* If we don't have a variable name, do nothing. */
	if (!words[1])
		return EXIT_FAILURE;

	/* Get the variable name. */
	char *name = words[1];
	if (getenv(name) == NULL) {
		printf("%s is not set.\n", name);
		return EXIT_FAILURE;
	}
	if (unsetenv(name) == -1) {
		perror("unsetenv");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

/**
 * Executes a program, based on the tokens provided as 
 * an argument.
 * For example, "ls -l" is represented in the tokens array by 
 * 2 strings "ls" and "-l", followed by a NULL token.
 * The command "ls -l | wc -l" will contain 5 tokens, 
 * followed by a NULL token. 
 */
int execute_command(char **tokens) {
	/* Execute the command here. */
	execvp(tokens[0], tokens);
	/* If the command executed properly, it should NOT get to this point.
	 * If it does, something went wrong; we just print the error here. */
	perror(tokens[0]);
	exit(EXIT_FAILURE);
}


/**
 * Executes a non-builtin command.
 */
int execute_nonbuiltin(simple_command *s) {
	/* If we write to any files, make sure that we set the permissions to 644. */
	#define MODE_644 (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

	/* If 'in' is set, open the file to read stdin from. */
	if (s->in) {
		int infd = open(s->in, O_RDONLY);
		if (infd == -1 ||
			dup2(infd, fileno(stdin)) == -1) {
			perror(s->in);
			return infd;
		}
		close(infd);
	}
	/* If 'out' is set, open the file to write stdout to. */
	if (s->out) {
		int outfd = open(s->out, O_CREAT | O_WRONLY | O_TRUNC, MODE_644);
		if (outfd == -1 ||
			dup2(outfd, fileno(stdout)) == -1) {
			perror(s->out);
			return outfd;
		}
		close(outfd);
	}
	/* If 'err' is set, open the file to write stderr to. */
	if (s->err) {
		int errfd = open(s->err, O_CREAT | O_WRONLY | O_TRUNC, MODE_644);
		if (errfd == -1 ||
			dup2(errfd, fileno(stderr)) == -1) {
			perror(s->err);
			return errfd;
		}
		close(errfd);
	}

	/* Finally execute the command. */
	return execute_command(s->tokens);
}


/**
 * Executes a simple command (no pipes).
 */
int execute_simple_command(simple_command *cmd) {
	/* Call the appropriate function if the command is a builtin. */
	switch (cmd->builtin) {
		case BUILTIN_CD:
			return execute_cd(cmd->tokens);
		case BUILTIN_SET:
			return execute_set(cmd->tokens);
		case BUILTIN_UNSET:
			return execute_unset(cmd->tokens);
		case BUILTIN_EXIT:
			exit(EXIT_SUCCESS);
	}

	/* Otherwise, we fork a new process to execute the command. */
	int pid = fork();
	if (pid == -1) {
		perror("fork");
		return EXIT_FAILURE;
	} else if (pid == 0) {
		return execute_nonbuiltin(cmd);
	} else {
		int status;
		if (wait(&status) == -1) {
			perror("wait");
			return EXIT_FAILURE;
		} else {
			if (WIFEXITED(status))
				return WEXITSTATUS(status);
			else
				return EXIT_FAILURE;
		}
	}
}


/**
 * Executes a complex command.  A complex command is two commands chained 
 * together with a pipe operator.
 */
int execute_complex_command(command *c) {
	/* If this is a simple command, just run it, except if it's a
	 * builtin, in which case we just ignore it. */
	if (c->scmd) {
		if (c->scmd->builtin)
			return 0;
		execute_nonbuiltin(c->scmd);
		return 0;
	}

	/** 
	 * Optional: if you wish to handle more than just the 
	 * pipe operator '|' (the '&&', ';' etc. operators), then 
	 * you can add more options here. 
	 */
	if (!strcmp(c->oper, "|")) {
		/* Do not execute if one of the commands was incomplete. */
		if (c->cmd1 == NULL || c->cmd2 == NULL) {
			fprintf(stderr, "incomplete command\n");
			return EXIT_FAILURE;
		}

		/* Create a pipe to communicate between the two processes. */
		int pfd[2];
		if (pipe(pfd) == -1) {
			perror("pipe");
			return EXIT_FAILURE;
		}
			
		/* Fork for the first process. */
		int pid = fork();
		if (pid == -1) {
			perror("fork");
			return EXIT_FAILURE;
		} else if (pid == 0) {
			/* Set the first process's stdout to write to the pipe. */
			close(fileno(stdout));
			close(pfd[0]);
			if (dup2(pfd[1], fileno(stdout)) == -1) {
				perror("dup2");
				return EXIT_FAILURE;
			}
			close(pfd[1]);
			/* Execute the first half of the pipe. */
			execute_complex_command(c->cmd1);
			exit(EXIT_FAILURE);
		} else {
			/* Fork for the second process. */
			int pid2 = fork();
			if (pid2 == -1) {
				perror("fork");
				return EXIT_FAILURE;
			} else if (pid2 == 0) {
				/* Set the second process's stdin to read from the pipe. */
				close(fileno(stdin));
				close(pfd[1]);
				if (dup2(pfd[0], fileno(stdin)) == -1) {
					perror("dup2");
					return EXIT_FAILURE;
				}
				close(pfd[0]);
				/* Execute the second half of the pipe. */
				execute_complex_command(c->cmd2);
				exit(EXIT_FAILURE);
			} else {
				int status1, status2;
				close(pfd[0]);
				close(pfd[1]);
				/* Wait for both programs to exit, then return the exit status
				 * of the second one. Kill if at least one of them did not exit. */
				waitpid(pid, &status1, 0);
				waitpid(pid2, &status2, 0);
				if (WIFEXITED(status1) && WIFEXITED(status2))
					return WEXITSTATUS(status2);
				else
					return EXIT_FAILURE;
			}
		}
		
	} else if (!strcmp(c->oper, "&")) {
		/* Do not execute if the left half is missing. */
		if (c->cmd1 == NULL) {
			fprintf(stderr, "incomplete command\n");
			return EXIT_FAILURE;
		}

		/* Fork for the first process. */
		int pid = fork();
		if (pid == -1) {
			perror("fork");
			return EXIT_FAILURE;
		} else if (pid == 0) {
			/* Execute the first command. */
			execute_complex_command(c->cmd1);
			exit(EXIT_FAILURE);
		} else {
			if (c->cmd2 == NULL) {
				return 0;
			}

			/* Fork for the second process. */
			int pid2 = fork();
			if (pid2 == -1) {
				perror("fork");
				return EXIT_FAILURE;
			} else if (pid2 == 0) {
				/* Execute the second command. */
				execute_complex_command(c->cmd2);
				exit(EXIT_FAILURE);
			} else {
				int status;
				/* Wait for only the second program to exit. The first
				 * one is running in the background. */
				waitpid(pid2, &status, 0);
				return WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
			}
		}
	} else if (!strcmp(c->oper, ";") || !strcmp(c->oper, "&&") || !strcmp(c->oper, "||")) {
		/* These three operators work in a similar way, so we can use
		 * the same code to implement them, with only a few changes. */

		/* Do not execute if one of the commands was incomplete. */
		if (c->cmd1 == NULL || c->cmd2 == NULL) {
			fprintf(stderr, "incomplete command\n");
			return EXIT_FAILURE;
		}

		/* Fork for the first process. */
		int pid = fork();
		if (pid == -1) {
			perror("fork");
			return EXIT_FAILURE;
		} else if (pid == 0) {
			/* Execute the first command. */
			execute_complex_command(c->cmd1);
			exit(EXIT_FAILURE);
		} else {
			/* This time, we check the wait for the first process to end
			 * before we execute the second one. */
			int status;
			if (wait(&status) == -1) {
				perror("wait");
				return EXIT_FAILURE;
			}
			if (!WIFEXITED(status))
				return EXIT_FAILURE;

			status = WEXITSTATUS(status);
			/* Exit after running the first process if:
			 *  (a) it failed and our command had a &&; or
			 *  (b) it succeeded and our command had a ||. */
			if (!strcmp(c->oper, "&&") && status)
				return status;
			if (!strcmp(c->oper, "||") && !status)
				return status;

			/* Fork for the second process. */
			int pid2 = fork();
			if (pid2 == -1) {
				perror("fork");
				return EXIT_FAILURE;
			} else if (pid2 == 0) {
				/* Execute the second command. */
				execute_complex_command(c->cmd2);
				exit(EXIT_FAILURE);
			} else {
				int status;
				/* Wait for it to exit. */
				waitpid(pid2, &status, 0);
				return WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
			}
		}
	}
	return 0;
}

/* Print the prompt string. */
void print_prompt(void) {
	/* Get the username. */
	struct passwd *pw = getpwuid(getuid());
	char *username = pw->pw_name;
	/* Get the home directory (from the HOME environment variable),
	 * so we can print the current directory relative to it. */
	char *homedir = pw->pw_dir;

	/* Get the computer's hostname. */
	char host[MAX_HOSTNAME];
	gethostname(host, MAX_HOSTNAME - 1);

	/* Get the current directory. */
	char cwd[MAX_DIRNAME];
	getcwd(cwd, MAX_DIRNAME - 1);

	/* Rewrite the current directory relative to the home directory (if possible). */
	char *cwd2 = cwd;
	if (is_in_home(cwd, homedir)) {
		cwd2 += strlen(homedir) - 1;
		*cwd2 = '~';
	}

	/* Build the prompt string from the contents of the PROMPT environment variable. */
	char *pstr = getenv("PROMPT");
	if (!pstr)
		pstr = "\\u@\\h:\\w$ ";
	int i;
	for (i = 0; i < strlen(pstr); ++i) {
		if (pstr[i] == '\\') {
			switch (pstr[++i]) {
				case 'u':
					printf("%s", username);
					break;
				case 'h':
					printf("%s", host);
					break;
				case 'w':
					printf("%s", cwd2);
					break;
				case 'e':
					printf("\033");
					break;
				default:
					break;
			}
		} else {
			printf("%c", pstr[i]);
		}
	}
}

