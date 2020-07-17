#ifndef __PARSER_H__
#define __PARSER_H__

#include "shell.h"

/* Determine if a token is a special operator (like '|') */
int is_operator(char *token); 

/* Determine if a command is builtin */
int is_builtin(char *token);

/* Determine if a path is relative or absolute (relative to root) */
int is_relative(char* path);

/* Determine if a path is inside the user's home directory. */
int is_in_home(char *path, char *home);

/* Determine if a command is complex (has an operator like pipe '|') */
int is_complex_command(char **tokens);

/* Parse a line into its tokens */
void parse_line(char *line, char **tokens);

/* Extract redirections of stdin, stdout, or stderr */
int extract_redirections(char** tokens, simple_command* cmd);

/* Construct command */
command* construct_command(char** tokens);

/* Release resources */
void release_command(command *cmd);

/* Print command */
void print_command(command *cmd, int level);

/* Remove double quotes from strings and expand environment variables. */
void process_tokens(char **tokens);

#endif
