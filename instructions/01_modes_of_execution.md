# Basic Shell: `wsh`

The shell you implement (`wsh`) should support both **interactive** and
**batch** modes of execution.

The maximum line size can be 1024 and the maximum arguments on a command
line can be 128.

## Interactive Mode

When run without arguments, `wsh` enters interactive mode. It displays a
prompt `wsh> ` and waits for the user input. Note the prompt `wsh> ` has a
space after the `>`.

```shell
$ ./wsh
wsh> echo hello world
hello world
wsh> exit
```

> [!tip]
>
> Usually when you write to files like `stdout` or `stderr`, the data often
> is written to a temporary buffer in memory instead of being written
> directly immediately. This is called *buffering*.
>
> When writing interactive applications, buffering sometimes gets in your way.
> You want the changes to a stream to reflect immediately. To do so, look
> into `fflush()`.

## Batch Mode

When run with a single filename argument, the shell should read commands
from the file *without printing a prompt*.

```shell
$ ./wsh script.wsh
```

The shell can be invoked with either no arguments or a single argument; anything
else is an error.

## Handling Errors

Your shell should handle errors gracefully. All error messages should be
printed to `stderr`

- If there is an error reading input in the **interactive mode** using
  `fgets()`, your shell should print the error message `fgets error\n`.
- If the file passed to batch mode does not exist, your shell should print
  the error message `fopen: No such file or directory\n`. Consider using
  `perror('fopen')` to print this error message as it will print the system
  error message for you.