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

// TODO: make const.
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
      free_argv(argv, *argc);
      argv[0] = "";
      *argc = 0;
      argv[1] = NULL;
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
      printf("%s\n", name);
      return 0;
    }
    else
    {
      wsh_warn(WHICH_NOT_FOUND, name);
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
  wsh_warn(WHICH_NOT_FOUND, name);
  return 1;
}

int path_set_and_get(char *argv[], int argc)
{
  if (argc > 2)
  {
    wsh_warn(INVALID_PATH_USE);
    return 1;
  }

  char *path = getenv("PATH");
  if (argc == 1)
  {
    if (path == NULL || *path == '\0')
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
}

int change_directory(char *argv[], int argc)
{
  if (argc == 2)
  {
    if (chdir(argv[1]) == -1)
    {
      wsh_warn(INVALID_CD_USE);
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
      wsh_warn(INVALID_CD_USE);
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
 * Checks if the command entered uses a builtin.
 * Returns: 0 if builtin used was 'exit'
 *          1 if builtin used was anything but 'exit'
 *         -1 if no builtin was found.
 */
int check_builtins(char *argv[], int argc)
{
  if (argc == 0)
  {
    return 1;
  }
  else if (strcmp(argv[0], "exit") == 0)
  {
    if ((rc = exit_shell(argc)) == 0)
    {
      return 0;
    }
  }
  else if (strcmp(argv[0], "alias") == 0)
  {
    if (create_alias(argv, argc) != 0)
    {
      wsh_warn(INVALID_ALIAS_USE);
    }
    return 1;
  }
  else if (strcmp(argv[0], "unalias") == 0)
  {
    if (unalias(argv, argc) != 0)
    {
      wsh_warn(INVALID_UNALIAS_USE);
    }
    return 1;
  }
  else if (strcmp(argv[0], "which") == 0)
  {
    which_command(argv, argc);
    return 1;
  }
  else if (strcmp(argv[0], "path") == 0)
  {
    path_set_and_get(argv, argc);
    return 1;
  }
  else if (strcmp(argv[0], "cd") == 0)
  {
    change_directory(argv, argc);
    return 1;
  }
  else if (strcmp(argv[0], "history") == 0)
  {
    show_history(argv, argc);
    return 1;
  }

  return -1;
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
  int keep_going = 1;
  char *argv[MAX_ARGS];
  int argc;
  while (keep_going)
  {
    printf("%s", PROMPT);

    char input[MAX_LINE + 1];
    if (fgets(input, sizeof(input), stdin) == NULL)
    {
      fprintf(stderr, "fgets error\n");
      continue;
    }

    parseline_no_subst(input, argv, &argc);

    if (argc == 0)
    {
      continue;
    }

    substitute_alias(argv, &argc);

    keep_going = check_builtins(argv, argc);
    if (keep_going == -1)
    {
      int rc = fork();
      if (rc < 0)
      {
        perror("fork");
      }
      else if (rc == 0)
      {
        char *full_path = get_command_path(argv[0]);
        if (full_path != NULL)
        {
          execv(full_path, argv);
          free(full_path);
          wsh_warn(CMD_NOT_FOUND, argv[0]);
        }
        free_argv(argv, argc);
        clean_exit(EXIT_FAILURE);
      }
      else
      {
        wait(NULL);
      }
    }

    da_put(history_da, input);
    free_argv(argv, argc);
    fflush(stderr);
    fflush(stdout);
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

  char command[MAX_LINE + 1];
  int keep_going = 1;
  char *argv[MAX_ARGS];
  int argc;
  while (fgets(command, sizeof(command), sfp) != NULL && keep_going)
  {

    parseline_no_subst(command, argv, &argc);

    if (argc == 0)
    {
      continue;
    }

    substitute_alias(argv, &argc);

    keep_going = check_builtins(argv, argc);
    if (keep_going == -1)
    {
      int rc = fork();
      if (rc < 0)
      {
        perror("fork");
      }
      else if (rc == 0)
      {
        char *full_path = get_command_path(argv[0]);
        if (full_path != NULL)
        {
          execv(full_path, argv);
          free(full_path);
          wsh_warn(CMD_NOT_FOUND, argv[0]);
        }
        fclose(sfp);
        free_argv(argv, argc);
        clean_exit(EXIT_FAILURE);
      }
      else
      {
        wait(NULL);
      }
    }

    da_put(history_da, command);
    free_argv(argv, argc);
  }

  int res = EXIT_SUCCESS;
  if (feof(sfp))
  {
    res = EXIT_SUCCESS;
  }
  else if (ferror(sfp))
  {
    fprintf(stderr, "fgets error\n");
    res = EXIT_FAILURE;
  }
  fclose(sfp);
  return res;
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
