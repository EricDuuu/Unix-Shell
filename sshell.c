#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Assumptions given by project page
#define CMDLINE_MAX 512
#define ARGCHAR_MAX 32
#define ARG_MAX 16

struct arguments {
  char *args[ARG_MAX];
};

// strtok to parse through each separated tokens, returns argLen for further use
int parseArgs(char **args, char *cmd) {
  int argLen = 0;
  char *token;
  token = strtok(cmd, " ");
  while (token != NULL) {
    args[argLen++] = token;
    token = strtok(NULL, " ");
  }
  args[argLen] = NULL;
  return argLen;
}

int main(void) {
  char cmd[CMDLINE_MAX];

  while (1) {
    char *nl;
    int retval;

    /* Print prompt */
    printf("sshell$ ");
    fflush(stdout);

    /* Get command line */
    char *temps = fgets(cmd, CMDLINE_MAX, stdin);

    /* Print command line if stdin is not provided by terminal */
    if (!isatty(STDIN_FILENO)) {
      printf("%s", cmd);
      fflush(stdout);
    }

    /* Remove trailing newline from command line */
    nl = strchr(cmd, '\n');
    if (nl)
      *nl = '\0';

    /* Builtin command */
    if (!strcmp(cmd, "exit")) {
      fprintf(stderr, "Bye...\n");
      break;
    }

    // Copy cmd because strtok replaces the string's pointers with NULL when
    // parsing
    char temp[CMDLINE_MAX];
    strcpy(temp, cmd);
    char *args[ARG_MAX];
    int argLen = parseArgs(args, temp);
    if (argLen > ARG_MAX)
      perror("Error: too many process arguments");

    // Piping and redirection
    for (int i = 1; i < argLen; i++) {
      char *ptr = strpbrk(args[i], "><|");

      if (ptr != NULL)

        switch (*ptr) {
        case '<':
          break;

        case '>':
          break;

        case '|':
          break;
        }
    }

    if (!fork()) { /* Fork off child process */
      // execvp automatically locates to $PATH no matter what
      execvp(args[0], args); /* Execute command */
      perror("execv");       /* Coming back here is an error */
      exit(1);
    } else {
      /* Parent */
      waitpid(-1, &retval, 0); /* Wait for child to exit */
    }

    fprintf(stdout, "Return status value for '%s': %d\n", cmd, retval);
  }

  return EXIT_SUCCESS;
}