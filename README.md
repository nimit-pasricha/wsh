# Project 2 - Building a shell in C

This is an individual programming project to help you practice and develop
your understanding of the Process API in C. You will be building a simple
toy shell that can execute commands, similar to the one you use every day.

## Project Administration and Policies

> [!important]
>
> **Due date:** Tuesday, September 30th 2025, 11:59 PM
>
> Follow all CS537 [project policies](Admin.md)

## Project Workflow

Your solution code will be tested in
the [CS537 Docker container](https://git.doit.wisc.edu/cdis/cs/courses/cs537/useful-resources/cs537-docker).
If you do not already have your container setup, follow the instructions in that repository to create the proper environment.
After following those instructions you should have a `cs537-projects/`
directory where you can clone **all** of the projects for this course.

Follow the suggested [workflow](Workflow.md) for development of your solution.

--------------------------------------------------------------------------------

## Project Instructions

> [!warning]
> **⚠️This project will be checked for memory leaks!⚠️**
>
> - Your shell must properly free all allocated memory before exiting.
> - Use tools like `valgrind` or `gcc --sanitize=address` to check for
    > memory leaks.
>
> Example: `valgrind --leak-check=full --show-leak-kinds=all 
> --track-origins=yes ./wsh`

### Background: What is a Shell?

A **shell** is a command-line tool that acts as an interface between the users
and the operating system. When you type commands in a terminal, you are
actually interacting with a shell program (e.g., `bash`, `zsh`, `fish`, etc.).

The shell reads your input, interprets(parses) your commands, executes them
and displays the output back to you.

### Learning Objectives for this Project

In this project, you will build a simple Unix shell called `wsh`. The shell
is the heart of the command-line interface, and thus is central to the
Unix/C programming environment. Knowing how the shell itself is built is the
focus of this project. Specifically, you will learn about:

- Process creation and management using system calls like `fork()`, `exec()`,
  and `wait()`.
- Communication between processes using pipes.
- The shell and its internals.

### Features of `wsh`

Your shell, `wsh` should support the following features. Treat these as
checkpoints for your implementation as well as the requirements for your
final submission. We strongly recommend doing a second pass to check if all
the components are working together correctly.

> [!important]
>
> The grader will do a direct diff between your output/error/rc and the
> expected output/error/rc. To help you with this, you can use the provided
> preprocessor directives in `wsh.h` for the error messages. We have also
> created a function `wsh_warn` to help you print to `stderr` easily.
>
> **Examples**
> ```c++
> // Lets say you have a string variable `cmd`
> char* cmd = "NonExistentCommand";
> // You can do something like this to print the error message:
> wsh_warn(CMD_NOT_FOUND, cmd);
> 
> // This works because we have defined CMD_NOT_FOUND in wsh.h as
> #define CMD_NOT_FOUND "Command not found or not an executable: %s\n"
> // We have setup the wsh_warn as a wrapper around vfprintf to print to 
> // let you use string formatting with it.
> ```
>
> *Sidenote:* I call it `wsh_warn` because even if something goes wrong with
> a command, the shell doesn't actually "crash" or "fail", it just prints a
> warning and continues with the next command.
>

1. [Modes of Execution](instructions/01_modes_of_execution.md)
2. [Executing External Commands](instructions/02_executing_external_commands.md)
3. [Builtin Commands](instructions/03_builtin_commands.md)
4. [Supporting Pipelines](instructions/04_pipelines.md)

At this point, your shell should be able to handle a variety of commands. We
would recommend testing your shell with a variety of commands to ensure it
works as expected. Try combining different features and see if your shell is
handling them correctly.
