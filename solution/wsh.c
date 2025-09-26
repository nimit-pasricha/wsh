#define _GNU_SOURCE
#include "wsh.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <ctype.h>

#include "dynamic_array.h"
#include "hash_map.h"
#include "utils.h"

int rc;
HashMap *alias_hm;
DynamicArray *history_da;

/***************************************************
 * Helper Functions
 ***************************************************/
/**
 * @Brief Free any allocated global resources
 */
void wsh_free(void)
{
  // Free any allocated resources here
  if (alias_hm != NULL)
  {
    hm_free(alias_hm);
    alias_hm = NULL;
  }
  if (history_da != NULL)
  {
    da_free(history_da);
    history_da = NULL;
  }
}

/**
 * @Brief Cleanly exit the shell after freeing resources
 *
 * @param return_code The exit code to return
 */
void clean_exit(int return_code)
{
  wsh_free();
  exit(return_code);
}

/**
 * @Brief Print a warning message to stderr and set the return code
 *
 * @param msg The warning message format string
 * @param ... Additional arguments for the format string
 */
void wsh_warn(const char *msg, ...)
{
  va_list args;
  va_start(args, msg);

  vfprintf(stderr, msg, args);
  va_end(args);
  rc = EXIT_FAILURE;
}

/**
 * @Brief Main entry point for the shell
 *
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return
 */
int main(int argc, char **argv)
{
  alias_hm = hm_create();
  history_da = da_create(10);
  setenv("PATH", "/bin", 1);
  if (argc > 2)
  {
    wsh_warn(INVALID_WSH_USE);
    return EXIT_FAILURE;
  }
  switch (argc)
  {
  case 1:
    interactive_main();
    break;
  case 2:
    rc = batch_main(argv[1]);
    break;
  default:
    break;
  }
  wsh_free();
  return rc;
}

char *get_command_path(char *command)
{
  if (command[0] == '.' || command[0] == '/')
  {
    if (access(command, X_OK) == 0)
    {
      return strdup(command);
    }
    wsh_warn(CMD_NOT_FOUND, command);
    return NULL;
  }

  char *path = getenv("PATH");
  if (path == NULL || *path == '\0')
  {
    wsh_warn(EMPTY_PATH);
    return NULL;
  }

  char *token = strtok(path, ":");
  char *command_path = NULL;
  while (token != NULL)
  {
    if (asprintf(&command_path, "%s/%s", token, command) < 0)
    {
      free(command_path);
      return NULL;
    }
    if (access(command_path, X_OK) == 0)
    {
      return command_path;
    }
    token = strtok(NULL, ":");
  }

  free(command_path);
  wsh_warn(CMD_NOT_FOUND, command);
  return NULL;
}

void free_argv(char *argv[], int argc)
{
  for (int i = 0; i < argc; i++)
  {
    free(argv[i]);
  }
}

int exit_shell(int argc)
{
  if (argc > 1)
  {
    wsh_warn(INVALID_EXIT_USE);
    return 1;
  }
  return 0;
}

int create_alias(char *argv[], int argc)
{
  if (argc > 4)
  {
    return 1;
  }

  if (argc == 1)
  {
    hm_print_sorted(alias_hm);
    return 0;
  }

  int found_equals_at_correct_spot = 0;
  for (int i = 0; i < argc; i++)
  {
    if (strcmp(argv[i], "=") == 0)
    {
      if (i != 2)
      {
        return 1;
      }
      found_equals_at_correct_spot = 1;
    }
  }

  if (!found_equals_at_correct_spot)
  {
    return 1;
  }

  char *name = argv[1];
  if (strlen(name) == 0)
  {
    return 1;
  }

  for (size_t i = 0; i < strlen(name); i++)
  {
    if (isspace(name[i]))
    {
      return 1;
    }
  }

  if (argc == 3)
  {
    argv[4] = NULL;
    argv[3] = "";
  }

  hm_put(alias_hm, name, argv[3]);
  return 0;
}

int substitute_alias(char *argv[], int *argc)
{
  if (*argc == 0)
  {
    return 0;
  }

  HashMap *seen_elements = hm_create();
  char *new_argv[MAX_ARGS];
  int new_argc = 1;
  char *command = hm_get(alias_hm, argv[0]);
  int keep_going = 1;
  while (command != NULL && keep_going)
  {
    parseline_no_subst(command, new_argv, &new_argc);

    if (new_argc == 0)
    {
      if (*argc == 1)
      {
        free_argv(argv, *argc);
        argv[0] = "";
        *argc = 0;
        argv[1] = NULL;
      }
      else
      {
        free(argv[0]);
        memmove(argv, argv + 1, (*argc - 1) * sizeof(char *));
        argv[*argc - 1] = NULL;
        *argc -= 1;
      }

      hm_free(seen_elements);
      return 0;
    }

    // Prevent infinite loop in case of circular alias.
    if (hm_get(seen_elements, argv[0]) != NULL)
    {
      hm_free(seen_elements);
      return 0;
    }

    // Prevent infinite loop. (eg: alias ls = 'ls -l')
    if (strcmp(new_argv[0], argv[0]) == 0)
    {
      keep_going = 0;
    }

    hm_put(seen_elements, argv[0], "");

    memmove(argv + new_argc - 1, argv, *argc * sizeof(char *));
    free(argv[new_argc - 1]);
    memmove(argv, new_argv, new_argc * sizeof(char *));
    command = hm_get(alias_hm, argv[0]);
  }
  *argc += new_argc - 1;
  argv[*argc] = NULL;
  hm_free(seen_elements);
  return 0;
}

int unalias(char *argv[], int argc)
{
  if (argc == 2)
  {
    hm_delete(alias_hm, argv[1]);
    return 0;
  }

  return 1;
}

int which_command(char *argv[], int argc)
{
  if (argc != 2)
  {
    wsh_warn(INVALID_WHICH_USE);
    return 1;
  }

  char *name = argv[1];
  char *res;
  if ((res = hm_get(alias_hm, name)) != NULL)
  {
    printf(WHICH_ALIAS, name, res);
    return 0;
  }

  char *builtins[7] = {"exit", "alias", "unalias", "which", "path", "cd", "history"};
  for (int i = 0; i < 7; i++)
  {
    if (strcmp(builtins[i], argv[1]) == 0)
    {
      printf(WHICH_BUILTIN, builtins[i]);
      return 0;
    }
  }

  if (name[0] == '.' || name[0] == '/')
  {
    if (access(name, X_OK) == 0)
    {
      printf(WHICH_EXTERNAL, name, name);
      return 0;
    }
    else
    {
      printf(WHICH_NOT_FOUND, name);
      return 1;
    }
  }

  char *path = getenv("PATH");
  if (path == NULL || *path == '\0')
  {
    wsh_warn(EMPTY_PATH);
    return -1;
  }

  char *token = strtok(path, ":");
  char *command_path = NULL;
  while (token != NULL)
  {
    if (asprintf(&command_path, "%s/%s", token, name) < 0)
    {
      free(command_path);
      return 1;
    }
    if (access(command_path, X_OK) == 0)
    {
      printf(WHICH_EXTERNAL, name, command_path);
      free(command_path);
      return 0;
    }
    token = strtok(NULL, ":");
  }

  free(command_path);
  printf(WHICH_NOT_FOUND, name);
  return 1;
}

int path_set_and_get(char *argv[], int argc)
{
  char *path = getenv("PATH");
  if (argc == 1)
  {
    if (path == NULL)
    {
      wsh_warn(EMPTY_PATH);
      return 1;
    }
    printf("%s\n", path);
    return 0;
  }
  else if (argc == 2)
  {
    setenv("PATH", argv[1], 1);
    return 0;
  }
  wsh_warn(INVALID_PATH_USE);
  return 1;
}

int change_directory(char *argv[], int argc)
{
  if (argc == 2)
  {
    if (chdir(argv[1]) == -1)
    {
      perror("cd");
      return 1;
    }
    return 0;
  }

  if (argc == 1)
  {
    char *home = getenv("HOME");
    if (home == NULL)
    {
      wsh_warn(CD_NO_HOME);
      return 1;
    }
    if (chdir(home) == -1)
    {
      perror("cd");
      return 1;
    }
    return 0;
  }
  wsh_warn(INVALID_CD_USE);
  return 1;
}

int show_history(char *argv[], int argc)
{
  if (argc == 1)
  {
    da_print(history_da);
    return 0;
  }
  if (argc == 2)
  {
    char *endptr = " ";
    int i = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0')
    {
      wsh_warn(HISTORY_INVALID_ARG);
      return 1;
    }
    else
    {
      char *command = da_get(history_da, i - 1);
      if (command != NULL)
      {
        printf("%s", command);
        return 0;
      }
      wsh_warn(HISTORY_INVALID_ARG);
      return 1;
    }
  }
  wsh_warn(INVALID_HISTORY_USE);
  return 1;
}

/**
 * Checks if command matches any builtins.
 * Returns: 0 if success
 *          1 if error
 *          2 if must exit.
 *         -1 if builtin not found.
 */
int check_builtins(char *argv[], int argc)
{
  int res;
  if (argc == 0)
  {
    res = 0;
  }
  else if (strcmp(argv[0], "exit") == 0)
  {
    res = exit_shell(argc);
    if (res == 0)
    {
      return 2;
    }
  }
  else if (strcmp(argv[0], "alias") == 0)
  {
    res = create_alias(argv, argc);
    if (res == 1)
    {
      wsh_warn(INVALID_ALIAS_USE);
    }
  }
  else if (strcmp(argv[0], "unalias") == 0)
  {
    res = unalias(argv, argc);
    if (res == 1)
    {
      wsh_warn(INVALID_UNALIAS_USE);
    }
  }
  else if (strcmp(argv[0], "which") == 0)
  {
    res = which_command(argv, argc);
  }
  else if (strcmp(argv[0], "path") == 0)
  {
    res = path_set_and_get(argv, argc);
  }
  else if (strcmp(argv[0], "cd") == 0)
  {
    res = change_directory(argv, argc);
  }
  else if (strcmp(argv[0], "history") == 0)
  {
    res = show_history(argv, argc);
  }
  else
  {
    res = -1;
  }
  return res;
}

/***************************************************
 * Modes of Execution
 ***************************************************/

/**
 * @Brief Interactive mode: print prompt and wait for user input
 * execute the given input and repeat
 */
void interactive_main(void)
{
  char *argv[MAX_ARGS];
  int argc;

  while (1)
  {
    while (wait(NULL) > 0)
    {
      // Wait until all children finish executing.
    }

    printf("%s", PROMPT);
    fflush(stdout);

    char input[MAX_LINE + 1];
    if (fgets(input, sizeof(input), stdin) == NULL)
    {
      fprintf(stderr, "fgets error\n");
      break;
    }

    // parse commands and store into commands array.
    char *commands[MAX_ARGS];
    int num_commands = 0;
    char *input_copy = input;
    char *input_dup_for_history = strdup(input);
    char *subcommand;

    while ((subcommand = strsep(&input_copy, "|")) != NULL && num_commands < MAX_ARGS)
    {
      commands[num_commands++] = subcommand;
    }

    // user just pressed 'Enter'.
    if (num_commands == 0)
    {
      free(input_dup_for_history);
      continue;
    }

    // I need to do this otherwise the external command
    // will print its output before a previously executed
    // builtin command.
    fflush(stdout);
    fflush(stderr);

    // handle single command (no piping)
    if (num_commands == 1)
    {
      parseline_no_subst(commands[0], argv, &argc);
      substitute_alias(argv, &argc);

      // aliased to nothing. (eg: alias test = '')
      if (argc == 0)
      {
        da_put(history_da, input_dup_for_history);
        free(input_dup_for_history);
        continue;
      }

      int res = check_builtins(argv, argc);
      if (res == 2) // 'exit' builtin.
      {
        free(input_dup_for_history);
        free_argv(argv, argc);
        return;
      }
      if (res == 1 || res == 0) // all other builtins.
      {
        free_argv(argv, argc);
      }
      else
      {
        // Execute single external command.
        int pid = fork();
        if (pid < 0)
        {
          perror("fork");
        }
        else if (pid == 0)
        {
          char *full_path = get_command_path(argv[0]);
          if (full_path != NULL)
          {
            execv(full_path, argv);
            free(full_path);
          }
          free(input_dup_for_history);
          free_argv(argv, argc);
          clean_exit(EXIT_FAILURE);
        }
        else
        {
          wait(NULL);
        }
        free_argv(argv, argc);
      }
    }
    else // handle piping (num_commands > 1)
    {
      // Create all pipes.
      int num_pipes = num_commands - 1;
      int pipes[num_pipes][2];
      for (int i = 0; i < num_pipes; i++)
      {
        if (pipe(pipes[i]) < 0)
        {
          perror("pipe");
          break;
        }
      }

      // Start all child processes.
      for (int i = 0; i < num_commands; i++)
      {
        int pid = fork();
        if (pid < 0)
        {
          perror("fork");
          break;
        }

        if (pid == 0)
        {
          if (i > 0)
          {
            dup2(pipes[i - 1][0], STDIN_FILENO);
          }

          if (i < num_commands - 1)
          {
            dup2(pipes[i][1], STDOUT_FILENO);
          }

          for (int j = 0; j < num_pipes; j++)
          {
            close(pipes[j][0]);
            close(pipes[j][1]);
          }

          parseline_no_subst(commands[i], argv, &argc);
          substitute_alias(argv, &argc);
          if (argc == 0)
          {
            wsh_warn(EMPTY_PIPE_SEGMENT);
            free(input_dup_for_history);
            clean_exit(EXIT_FAILURE);
          }

          int res = check_builtins(argv, argc);
          if (res == 0 || res == 1 || res == 2)
          {
            free_argv(argv, argc);
            free(input_dup_for_history);
            clean_exit(EXIT_SUCCESS);
          }

          char *full_path = get_command_path(argv[0]);
          if (full_path != NULL)
          {
            execv(full_path, argv);
            free(full_path);
          }
          free(input_dup_for_history);
          free_argv(argv, argc);
          clean_exit(EXIT_FAILURE);
        }
      }

      for (int i = 0; i < num_pipes; i++)
      {
        close(pipes[i][0]);
        close(pipes[i][1]);
      }
    }
    da_put(history_da, input_dup_for_history);
    free(input_dup_for_history);
  }
}

/**
 * @Brief Batch mode: read commands from script file line by line
 * execute each command and repeat until EOF
 *
 * @param script_file Path to the script file
 * @return EXIT_SUCCESS(0) on success, EXIT_FAILURE(1) on error
 */

int batch_main(const char *script_file)
{
  FILE *sfp = fopen(script_file, "r");
  if (sfp == NULL)
  {
    perror("fopen");
    clean_exit(EXIT_FAILURE);
  }

  char line[MAX_LINE + 1];
  char *argv[MAX_ARGS];
  int argc;
  int final_status = 0;

  while (fgets(line, sizeof(line), sfp) != NULL)
  {
    // parse commands and store into commands array.
    char *commands[MAX_ARGS];
    int num_commands = 0;
    char *line_copy = line;
    char *line_dup_for_history = strdup(line);
    char *subcommand;
    while ((subcommand = strsep(&line_copy, "|")) != NULL && num_commands < MAX_ARGS)
    {
      commands[num_commands++] = subcommand;
    }

    // Blank line.
    if (num_commands == 0)
    {
      free(line_dup_for_history);
      continue;
    }

    // I need to do this otherwise the current external command
    // will print its output before a previously executed
    // builtin command.
    fflush(stdout);
    fflush(stderr);

    // handle single command (no piping)
    if (num_commands == 1)
    {
      parseline_no_subst(commands[0], argv, &argc);
      substitute_alias(argv, &argc);

      if (argc == 0)
      {
        da_put(history_da, line_dup_for_history);
        free(line_dup_for_history);
        continue;
      }

      int res = check_builtins(argv, argc);

      if (res == 2) // 'exit' builtin.
      {
        free(line_dup_for_history);
        free_argv(argv, argc);
        fclose(sfp);
        return final_status;
      }
      else if (res == 1 || res == 0) // all other builtins
      {
        final_status = res;
        free_argv(argv, argc);
      }
      else
      {
        // Execute single external command.
        int pid = fork();
        if (pid < 0)
        {
          perror("fork");
        }
        else if (pid == 0)
        {
          fclose(sfp);
          char *full_path = get_command_path(argv[0]);
          if (full_path != NULL)
          {
            execv(full_path, argv);
            free(full_path);
          }
          free(line_dup_for_history);
          free_argv(argv, argc);
          clean_exit(EXIT_FAILURE);
        }
        else
        {
          // change parent's exit status to
          // whatever the child's exit status was.
          int status;
          waitpid(pid, &status, 0);
          if (WIFEXITED(status))
          {
            final_status = WEXITSTATUS(status);
          }
        }
        free_argv(argv, argc);
      }
    }
    else // handle piping (num_commands > 1)
    {
      // Create all pipes.
      int num_pipes = num_commands - 1;
      int pipes[num_pipes][2];
      for (int i = 0; i < num_pipes; i++)
      {
        if (pipe(pipes[i]) < 0)
        {
          perror("pipe");
          break;
        }
      }

      // Start all child processes.
      pid_t pids[num_commands];
      for (int i = 0; i < num_commands; i++)
      {
        pids[i] = fork();
        if (pids[i] < 0)
        {
          perror("fork");
          break;
        }
        else if (pids[i] == 0)
        {
          fclose(sfp);
          if (i > 0)
          {
            dup2(pipes[i - 1][0], STDIN_FILENO);
          }
          if (i < num_commands - 1)
          {
            dup2(pipes[i][1], STDOUT_FILENO);
          }

          for (int j = 0; j < num_pipes; j++)
          {
            close(pipes[j][0]);
            close(pipes[j][1]);
          }

          parseline_no_subst(commands[i], argv, &argc);
          substitute_alias(argv, &argc);
          if (argc == 0)
          {
            wsh_warn(EMPTY_PIPE_SEGMENT);
            free(line_dup_for_history);
            clean_exit(EXIT_FAILURE);
          }

          int res = check_builtins(argv, argc);
          if (res == 0 || res == 1 || res == 2)
          {
            free_argv(argv, argc);
            free(line_dup_for_history);
            clean_exit(EXIT_SUCCESS);
          }

          char *full_path = get_command_path(argv[0]);
          if (full_path != NULL)
          {
            execv(full_path, argv);
            free(full_path);
          }
          free(line_dup_for_history);
          free_argv(argv, argc);
          clean_exit(EXIT_FAILURE);
        }
      }

      for (int i = 0; i < num_pipes; i++)
      {
        close(pipes[i][0]);
        close(pipes[i][1]);
      }

      // parent wait for every child to finish.
      for (int i = 0; i < num_commands; i++)
      {
        int status;
        waitpid(pids[i], &status, 0);
        // only get the exit status of the last command.
        if (i == num_commands - 1 && WIFEXITED(status))
        {
          final_status = WEXITSTATUS(status);
        }
      }
    }
    da_put(history_da, line_dup_for_history);
    free(line_dup_for_history);
  }
  fclose(sfp);
  return final_status;
}

/***************************************************
 * Parsing
 ***************************************************/

/**
 * @Brief Parse a command line into arguments without doing
 * any alias substitutions.
 * Handles single quotes to allow spaces within arguments.
 *
 * @param cmdline The command line to parse
 * @param argv Array to store the parsed arguments (must be preallocated)
 * @param argc Pointer to store the number of parsed arguments
 */
void parseline_no_subst(const char *cmdline, char **argv, int *argc)
{
  if (!cmdline)
  {
    *argc = 0;
    argv[0] = NULL;
    return;
  }
  char *buf = strdup(cmdline);
  if (!buf)
  {
    perror("strdup");
    clean_exit(EXIT_FAILURE);
  }
  /* Replace trailing newline with space */
  const size_t len = strlen(buf);
  if (len > 0 && buf[len - 1] == '\n')
    buf[len - 1] = ' ';
  else
  {
    char *new_buf = realloc(buf, len + 2);
    if (!new_buf)
    {
      perror("realloc");
      free(buf);
      clean_exit(EXIT_FAILURE);
    }
    buf = new_buf;
    strcat(buf, " ");
  }

  int count = 0;
  char *p = buf;
  while (*p && *p == ' ')
    p++; /* skip leading spaces */

  while (*p)
  {
    char *token_start = p;
    char *token = NULL;
    if (*p == '\'')
    {
      token_start = ++p;
      token = strchr(p, '\'');
      if (!token)
      {
        /* Handle missing closing quote - Print `Missing closing quote` to
         * stderr */
        wsh_warn(MISSING_CLOSING_QUOTE);
        free(buf);
        for (int i = 0; i < count; i++)
          free(argv[i]);
        *argc = 0;
        argv[0] = NULL;
        return;
      }
      *token = '\0';
      p = token + 1;
    }
    else
    {
      token = strchr(p, ' ');
      if (!token)
        break;
      *token = '\0';
      p = token + 1;
    }
    argv[count] = strdup(token_start);
    if (!argv[count])
    {
      perror("strdup");
      for (int i = 0; i < count; i++)
        free(argv[i]);
      free(buf);
      clean_exit(EXIT_FAILURE);
    }
    count++;
    while (*p && (*p == ' '))
      p++;
  }
  argv[count] = NULL;
  *argc = count;
  free(buf);
}
