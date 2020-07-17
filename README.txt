While the required functionality for complex commands was only to support |,
this shell supports the following operators: |, &, ;, &&, ||. For &, if the
right half of the command is missing, it executes the left half in the
background; for the other operators, if at least one half it missing it
returns with an error 'incomplete command'. An additional few lines were added
to parser.c to handle empty commands (it used to throw segfaults before).

This shell takes prompt strings from the 'PROMPT' environment variable; you
can set it through the parent shell or using the 'set' command (see below).
The escape sequences \u (username), \h (hostname), \w (working directory) are
supported. Adding an $ at the end might mess things up though, because of
variable substitutions.

Environment variables can be set and unset using the 'set' and 'unset'
commands respectively:

    set PROMPT "\\w> "
	unset PROMPT

Environment variables can also be used in commands by prefixing their names
with $:

    echo $HOME

~ does not expand to $HOME though.

Surrounding text with double quotes ("") turns it into a single token, and
allows you to include spaces in e.g. filenames and text arguments.

