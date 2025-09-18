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
HashMap *alias_hm = NULL;

/***************************************************
 * Helper Functions
 ***************************************************/
/**
 * @Brief Free any allocated global resources
 */
void wsh_free(void) {
  // Free any allocated resources here
  if (alias_hm != NULL) {
    hm_free(alias_hm);
    alias_hm = NULL;
  }
}

/**
 * @Brief Cleanly exit the shell after freeing resources
 *
 * @param return_code The exit code to return
 */
void clean_exit(int return_code) {
  wsh_free();
  exit(return_code);
}

/**
 * @Brief Print a warning message to stderr and set the return code
 *
 * @param msg The warning message format string
 * @param ... Additional arguments for the format string
 */
void wsh_warn(const char *msg, ...) {
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
int main(int argc, char **argv) {
  alias_hm = hm_create();
  setenv("PATH", "/bin", 1);
  if (argc > 2) {
    wsh_warn(INVALID_WSH_USE);
    return EXIT_FAILURE;
  }
  switch (argc) {
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

/***************************************************
 * Modes of Execution
 ***************************************************/

/**
 * @Brief Interactive mode: print prompt and wait for user input
 * execute the given input and repeat
 */
void interactive_main(void) {
  while (1) {
    printf("%s", PROMPT);

    char input[MAX_LINE];
    if (fgets(input, sizeof(input), stdin) == NULL) {
      fprintf(stderr, "fgets error\n");
      wsh_free();
      clean_exit(EXIT_FAILURE);
    }

    char *argv[MAX_ARGS + 1];
    int argc;
    parseline_no_subst(input, argv, &argc);

    if (strcmp(argv[0], "exit") == 0) {
      break;
    }

    int rc = fork();
    if (rc < 0) {
      fprintf(stderr, "fork error\n");
      clean_exit(EXIT_FAILURE);
    } else if (rc == 0) {
      execvp(argv[0], argv);
      wsh_warn(CMD_NOT_FOUND, argv[0]);
      clean_exit(EXIT_FAILURE);
    } else {
      wait(NULL);
    }

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

int batch_main(const char *script_file) {
  FILE *sfp = fopen(script_file, "r");
  if (sfp == NULL) {
    perror("fopen");
    return EXIT_FAILURE;
  }

  char command[MAX_LINE];

  
  while (fgets(command, sizeof(command), sfp)) {
    char *argv[MAX_ARGS + 1];
    int argc;
    parseline_no_subst(command, argv, &argc);

    if (strcmp(argv[0], "exit") == 0) {
      break;
    }

    int rc = fork();
    if (rc < 0) {
      fprintf(stderr, "fork error\n");
      return EXIT_FAILURE;
    } else if (rc == 0) {
      execvp(argv[0], argv);
      wsh_warn(CMD_NOT_FOUND, argv[0]);
      fclose(sfp);
      return EXIT_FAILURE;
    } else {
      wait(NULL);
    }
  }
  fclose(sfp);
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
void parseline_no_subst(const char *cmdline, char **argv, int *argc) {
  if (!cmdline) { 
    *argc = 0;
    argv[0] = NULL;
    return;
  }
  char *buf = strdup(cmdline);
  if (!buf) {
    perror("strdup");
    clean_exit(EXIT_FAILURE);
  }
  /* Replace trailing newline with space */
  const size_t len = strlen(buf);
  if (len > 0 && buf[len - 1] == '\n')
    buf[len - 1] = ' ';
  else {
    char *new_buf = realloc(buf, len + 2);
    if (!new_buf) {
      perror("realloc");
      free(buf);
      clean_exit(EXIT_FAILURE);
    }
    buf = new_buf;
    strcat(buf, " ");
  }

  int count = 0;
  char *p = buf;
  while (*p && *p == ' ') p++; /* skip leading spaces */

  while (*p) {
    char *token_start = p;
    char *token = NULL;
    if (*p == '\'') {
      token_start = ++p;
      token = strchr(p, '\'');
      if (!token) {
        /* Handle missing closing quote - Print `Missing closing quote` to
         * stderr */
        wsh_warn(MISSING_CLOSING_QUOTE);
        free(buf);
        for (int i = 0; i < count; i++) free(argv[i]);
        *argc = 0;
        argv[0] = NULL;
        return;
      }
      *token = '\0';
      p = token + 1;
    } else {
      token = strchr(p, ' ');
      if (!token) break;
      *token = '\0';
      p = token + 1;
    }
    argv[count] = strdup(token_start);
    if (!argv[count]) {
      perror("strdup");
      for (int i = 0; i < count; i++) free(argv[i]);
      free(buf);
      clean_exit(EXIT_FAILURE);
    }
    count++;
    while (*p && (*p == ' ')) p++;
  }
  argv[count] = NULL;
  *argc = count;
  free(buf);
}
