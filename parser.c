#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "parser.h"
#include "shell.h"

/* Determine if a token is a special operator (like '|') */
int is_operator(char *token) {
	/** 
	 * Optional: edit this if you wish to parse other operators
	 * like ";", "&&", etc.
	 */
	return !strcmp(token, "|") ||
			!strcmp(token, "&") ||
			!strcmp(token, ";") ||
			!strcmp(token, "||") ||
			!strcmp(token, "&&");
}

/* Determine if a command is builtin */
int is_builtin(char *token) {
	if (!strcmp(token, "cd"))
		return BUILTIN_CD;
	if (!strcmp(token, "exit"))
		return BUILTIN_EXIT;
	if (!strcmp(token, "set"))
		return BUILTIN_SET;
	if (!strcmp(token, "unset"))
		return BUILTIN_UNSET;
	return 0;
}

/* Determine if a path is relative or absolute (relative to root) */
int is_relative(char* path) {
	return (path[0] != '/'); 
}

/* Determine if a path is inside the user's home directory. */
int is_in_home(char *path, char *home) {
	/* If the path is exactly the same as the home directory */
	if (!strcmp(path, home))
		return 1;
	/* Then we check if the path begins with the path of the home directory,
	 * and the character immediately next to it is a slash. */
	if (strlen(path) > strlen(home) &&
		!strncmp(path, home, strlen(home)) &&
		path[strlen(home)] == '/')
		return 1;
	return 0;
}

/* Determine if a command is complex (has an operator like pipe '|') */
int is_complex_command(char **tokens) {
	
	int i = 0;
	while(tokens[i]) {		
		if (is_operator(tokens[i]))
			return 1;
		i++;
	}	
	return 0;
}

/* Parse a line into its tokens/words */
void parse_line(char *line, char **tokens) {
	
	/* This flag indicates whether the current character is
	 * in a double-quoted string (""). */
	int in_str = 0;

	while (*line != '\0') {
		/* Replace all whitespaces with \0 */
		while (*line == ' ' || *line == '\t' || *line == '\n') { 
			*line++ = '\0';
		}

		if (*line == '\0') {
      			break;
		}
			
		/* Store the position of the token in the line */
		*tokens++ = line;
		//printf("token: %s\n", *(tokens-1));
		
		/* Ignore non-whitespace, until next whitespace delimiter */
		while (*line != '\0' && *line != ' ' &&
		       *line != '\t' && *line != '\n')  {
			if (*line == '"')
				in_str = !in_str;
			line++;
			if (*line == ' ' && in_str)
				line++;
		}
	}
	*tokens = '\0';
}

int extract_redirections(char** tokens, simple_command* cmd) {
	
	int i = 0;
	int skipcnt = 0;
	
	while(tokens[i]) {
		
		int skip = 0;		
		if (!strcmp(tokens[i], ">")) {
			if(!tokens[i+1]) {
				return -1;
			}
			cmd->out = tokens[i+1];
			skipcnt += 2;
			skip = 1;
		}			
		if (!strcmp(tokens[i], "<")) {
			if(!tokens[i+1]) {
				return -1;
			}
			cmd->in = tokens[i+1];
			skipcnt += 2;
			skip = 1;
		}			
		if (!strcmp(tokens[i], "2>")) {
			if(!tokens[i+1]) {
				return -1;
			}
			cmd->err = tokens[i+1];
			skipcnt += 2;
			skip = 1;
		}
		if (!strcmp(tokens[i], "&>")) {
			if(!tokens[i+1]) {
				return -1;
			}
			cmd->out = tokens[i+1];
			cmd->err = tokens[i+1];
			skipcnt += 2;
			skip = 1;
		}
			
		if(skip){   
			i++;
		}
		
		i++;
	}
	
	cmd->tokens = malloc((i-skipcnt+1) * sizeof(char*));	
	
	int j = 0;
	i = 0;
	while(tokens[i]) {
		if (!strcmp(tokens[i], "<") ||
		    !strcmp(tokens[i], ">") ||
		    !strcmp(tokens[i], "2>") ||
		    !strcmp(tokens[i], "&>")) {
			i += 2;
		 } else {
			cmd->tokens[j++] = tokens[i++];
		 }
	}
	cmd->tokens[j] = NULL;
	
	return 0;
}

/* Construct command */
command* construct_command(char** tokens) {
	if (*tokens == NULL)
		return NULL;

	/* Initialize a new command */	
	command *cmd = malloc(sizeof(command));
	cmd->cmd1 = NULL;
	cmd->cmd2 = NULL;
	cmd->scmd = NULL;

	if (!is_complex_command(tokens)) {
		
		/* Simple command */
		cmd->scmd = malloc(sizeof(simple_command));
		cmd->scmd->in = NULL;
		cmd->scmd->out = NULL;
		cmd->scmd->err = NULL;
		cmd->scmd->tokens = NULL;
		
		cmd->scmd->builtin = is_builtin(tokens[0]);
		
		int err = extract_redirections(tokens, cmd->scmd);
		if (err == -1) {
			printf("Error extracting redirections!\n");	
			return NULL;
		}
	}
	else {
		/* Complex command */
		
		char **t1 = tokens, **t2;
		int i = 0;
		while(tokens[i]) {
			if(is_operator(tokens[i])) {
				strncpy(cmd->oper, tokens[i], 2);
				tokens[i] = NULL;
				t2 = &(tokens[i+1]);
				break;
			}
			i++;
		}
		
		/* Recursively construct the rest of the commands */
		cmd->cmd1 = construct_command(t1);
		cmd->cmd2 = construct_command(t2);
	}
	
	return cmd;
}

/* Release resources */
void release_command(command *cmd) {
	
	if(cmd->scmd && cmd->scmd->tokens) {
		free(cmd->scmd->tokens);
	}
	if(cmd->cmd1) {
		release_command(cmd->cmd1);
	}
	if(cmd->cmd2) {
		release_command(cmd->cmd2);		
	}
}

/* Print command */
void print_command(command *cmd, int level) {

	int i;
	for(i = 0; i < level; i++) {
		printf("  ");
	}

	if (cmd == NULL) {
		printf("(empty)");
		return;
	}
	
	if(cmd->scmd) {
		
		i = 0;
		while(cmd->scmd->tokens[i]) { 
			printf("%s ", cmd->scmd->tokens[i]);
			i++;
		}
		
		if(cmd->scmd->in) {
			printf("< %s ", cmd->scmd->in);
		}

		if(cmd->scmd->out) {
			printf("> %s ", cmd->scmd->out);
		}

		if(cmd->scmd->err) {
			printf("2> %s ", cmd->scmd->err);
		}
			
		printf("\n");
		return;		 
	}
	
	printf("Pipeline:\n");
			
	if(cmd->cmd1) {
		print_command(cmd->cmd1, level+1);
	}

	if(cmd->cmd2) {
		print_command(cmd->cmd2, level+1);
	}
	
}

/* Define our valid characters in variable names. */
#define VALID_VAR_BEGIN(a) (isalpha(a) || (a) == '_')
#define VALID_VAR(a) (isalnum(a) || (a) == '_')

/* Remove double quotes from strings, and expand environment variables. */
void process_tokens(char **tokens) {
	/* For each token, we remove all the double quotes (if
	 * they're escaped, replace them with plain double quotes). */
	int i;
	for (i = 0; tokens[i]; ++i) {
		/* Set up pointers on the same string, to replace the quotes in-place. */
		char *s, *d;
		s = d = tokens[i];
		while (*s) {
			/* Substitute any escape sequences we find here. */
			if (*s == '\\') {
				switch (*(s + 1)) {
					case '"':
						*d++ = '"';
						break;
					case 'a':
						*d++ = '\a';
						break;
					case 'b':
						*d++ = '\b';
						break;
					case 'f':
						*d++ = '\f';
						break;
					case 'n':
						*d++ = '\n';
						break;
					case 'r':
						*d++ = '\r';
						break;
					case 't':
						*d++ = '\t';
						break;
					case 'v':
						*d++ = '\v';
						break;
					case '\\':
						*d++ = '\\';
						break;
					default:
						--s;
						break;
				}
				++s;
			} else if (*s != '"') {
				*d++ = *s;
			}
			++s;
		}
		*d = 0;
	}

	/* For each token, build a new string where all the $variables are expanded. */
	for (i = 0; tokens[i]; ++i) {
		/* If there is no $, just move to the next token. */
		if (!strchr(tokens[i], '$'))
			continue;

		char newtok[1024];
		char *s = tokens[i], *d = newtok;
		while (*s) {
			if (*s == '$') {
				/* If the $ is followed by a valid variable name, */
				if (VALID_VAR_BEGIN(*(s + 1))) {
					char varname[128];
					char *v = varname;
					/* we capture that name and copy the contents of that variable
					 * into our final string. */
					while (VALID_VAR(*(s + 1)))
						*v++ = *(s++ + 1);
					*v = 0;
					char *value = getenv(varname);
					if (value) {
						strcpy(d, getenv(varname));
						d = strchr(newtok, 0);
					}
				}
			} else if (*s == '\\') {
				switch (*(s + 1)) {
					/* Substitute some escape sequences. */
					case '$':
						*d++ = '$';
						break;
					case ' ':
						*d++ = ' ';
						break;
					case '\\':
						*d++ = '\\';
						break;
					default:
						--s;
						break;
				}
				++s;
			} else {
				*d++ = *s;
			}
			++s;
		}
		*d = 0;
		tokens[i] = newtok;
	}
}
