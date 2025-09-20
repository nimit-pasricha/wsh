#define _GNU_SOURCE
#include "wsh.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "dynamic_array.h"
#include "hash_map.h"
#include "utils.h"

int rc;
HashMap *alias_hm;

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
    char *res = strdup(command);
    if (res == NULL)
    {
      perror("strdup");
    }
    return res;
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
  // if (argc == 1)
  // {
  //   return 0;
  // }
  // else
  // {
  //   wsh_warn(INVALID_EXIT_USE);
  //   return 1;
  // }
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
  while (keep_going)
  {
    printf("%s", PROMPT);

    char input[MAX_LINE + 1];
    if (fgets(input, sizeof(input), stdin) == NULL)
    {
      fprintf(stderr, "fgets error\n");
      continue;
    }

    char *argv[MAX_ARGS + 1];
    int argc;
    parseline_no_subst(input, argv, &argc);

    if (argc == 0)
    {
      // Do nothing.
    }
    else if (strcmp(argv[0], "exit") == 0)
    {
      if (exit_shell(argc) == 0)
      {
        keep_going = 0;
      }
    }
    else
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
  while (fgets(command, sizeof(command), sfp) != NULL)
  {
    char *argv[MAX_ARGS + 1];
    int argc;
    parseline_no_subst(command, argv, &argc);

    if (argc == 0)
    {
      continue;
    }

    // Handle "exit"
    if (strcmp(argv[0], "exit") == 0)
    {
      if (exit_shell(argc) == 0)
      {
        fclose(sfp);
        clean_exit(EXIT_SUCCESS);
      }
      else
      {
        continue;
      }
    }

    // Handle "alias"
    if (strcmp(argv[0], "alias") == 0)
    {
      // TODO: handle errors.
    }

    int rc = fork();
    if (rc < 0)
    {
      perror("fork");
      fclose(sfp);
      free_argv(argv, argc);
      continue;
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
    free_argv(argv, argc);
  }

  if (feof(sfp))
  {
    fclose(sfp);
    return EXIT_SUCCESS;
  }
  else if (ferror(sfp))
  {
    fprintf(stderr, "fgets error\n");
    fclose(sfp);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
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
