# Executing External Commands

You should structure your shell such that it forks a process for each new
command the user executes in it (the exception to this are [built-in]
(03_builtin_commands.md) commands).

> [!tip]
>
> To execute commands after parsing them, look into `fork()`, `exec()`, and
> `wait()/waitpid()`. See the man pages for these functions. Go through the
> [Process API](https://pages.cs.wisc.edu/~remzi/OSTEP/cpu-api.pdf) chapter
> from the book and the corresponding
> [code](https://github.com/remzi-arpacidusseau/ostep-code/tree/master/cpu-api)
> for a refresher on these.

Your basic shell should be able to parse a command and run the program
corresponding to the command. For example, if the user types `cp -r /tmp 
/tmp2`, your shell should run the program `/bin/cp` with the arguments `-r`,
`/tmp` and `/tmp2`. How does the shell know to run `/bin/cp`? It infers this
from the **PATH** environment variable; more on this in
the [note](#searching-in-path) below.

Note that the shell itself does not implement `cp` or other commands
(except built-ins). All it does is find those executables in one of
the directories specified by path and create a new process to run them. Try
`echo $PATH` (in your preferred shell like `bash`/`zsh` not `wsh`) to see where
your shell looks for executables.

> [!important]
>
> If the user specifies a command that starts with a `.` (i.e. a relative
> path) or `/` (i.e. absolute path) (e.g. `./myprog` or `/usr/bin/ls`), you
> should treat it as the path to an executable file. In this case, you should
> not search for the executables in the directories specified by `PATH`.

## Searching in PATH

> [!note]
>
> The shell uses the `PATH` environment variable to find the executable file
> corresponding to a command. The `PATH` variable contains a colon-separated
> list of directories.
>
> For example, if `PATH` is set to `/bin:/usr/bin`, when
> the user types `cp`, the shell will look for an executable file named
> `cp` in the directories `/bin` and `/usr/bin`, in that order. If it finds the
> file in one of these directories, it runs that file. If it doesn't find
> the file in any of these directories, it prints an error message.
>
> > [!tip]
> > Look into the `getenv()` function to get the value of the `PATH` (or any
> > other) environment variable. `access()` with the `X_OK` flag can help
> > you check if a file exists and if you have permissions to execute it.

You will note that there are a variety of commands in the `exec` family; for
this project, you must use `execv()`. You should **not** use `system()`
library function call to run a command. Remember that if `execv()` is
successful, it will not return; if it does return, there was an error (e.g.,
the command does not exist).

> [!important]
>
> Your initial PATH environment variable should have only one directory:
> `/bin`. Look into the `setenv()` function to set environment variables.
>
> Later in the project, you will implement a built-in command `path` to help
> you change the PATH variable through your shell.

## Handling Errors

Your shell should handle errors gracefully. All error messages should be
printed to `stderr`

- If the user types a command that does not exist or if you cannot execute it,
  your shell should print the error message `Command not found or not 
  executable: %s\n` where `%s` is the command.
- If the given command is not a absolute or relative path, and the `PATH`
  variable is empty, your shell should print the error message `PATH empty 
  or not set\n`.
- If there is a missing closing quote when parsing the command line, your shell
  should print the error message `Missing Closing Quote\n`.

> [!tip]
>
>  Use `perror("pipe")`, `perror("fork")`, `perror("dup2")`, `perror("strdup")
`, `perror("malloc")`, `perror("realloc")` if the corresponding calls fail.

If your shell encounters an error, it should not crash or exit; it should
print the error message and continue to prompt the user for the next command.

## Understanding the parsing logic (Provided to you)

The parsing logic is provided to you through the `parseline_no_subst()`
function.

```c++
void parseline_no_subst(const char *cmdline, char **argv, int *argc);
```

This function takes a command line string as input and fills the `argv` array
with pointers to the individual arguments, and sets `argc` to the number of
arguments.

**Features**

- **Supports only single quotes** for grouping arguments.
  - Double quotes are not supported.
  - Also note that there is no escape character support. So you cannot
    escape a quote inside a quote by doing something like `'abc \' bcd'`.
  - It expects quotes to come right after a space. So, something like `ls'a 
    bc'` will be split into two arguments: `ls'a` and `bc'`.
- **Splits input by spaces**, except for text inside single quotes.
- **Supports single-quoted arguments**. For example, `'My Folder'` becomes a
  single argument.
- **Handles leading/trailing spaces and empty input.** It also handles
  multiple spaces between arguments.
- **Memory allocation:** Each argument is dynamically allocated; you are
  responsible for freeing them after use.
- **Null-terminated `argv`:** The last element of `argv` is set to `NULL`,
  which should simplify using them with `execv()`.

**Example Usage:**

```c++
// NOTE: This is just and example number are not representative of actual usage
char *argv[10]; // Allocate space for up to 10 arguments
int argc;
parseline_no_subst("ls -l 'My Folder'", argv, &argc);

// After calling:
// argc == 3
// argv[0] == "ls"
// argv[1] == "-l"
// argv[2] == "My Folder"
// argv[3] == NULL

// You will have to free the allocated memory for each argument after use
```
