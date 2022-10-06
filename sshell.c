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

// Implemented with pipelining and redirection in mind
// all command info is stored here.
struct command {
  char *args[ARG_MAX];
  char *input;
  char *output;
  struct command *next;
};

// strtok to parse through each separated arguments, returns argLen for further
// use
int parseArgs(struct command *cmd, char *buffer) {
  int argLen = 0;

  char *token = strtok(buffer, " ");
  while (token != NULL) {

    // Case where the symbol is spaced between commands
    if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0 ||
        strcmp(token, "|") == 0) {
    }

    char *ptr = strpbrk(token, "><|");

    cmd->args[argLen] = token;
    argLen++;
    token = strtok(NULL, " ");
  }

  // Piping and redirection; Should move to parsing
  for (int i = 1; i < argLen; i++) {
    char *ptr = strpbrk(cmd->args[i], "><|");

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

  cmd->args[argLen] = NULL;
  return argLen;
}

// Grab the command from stdin and sanitize for execution given by skeleton code
static void getCmd(char *buffer) {
  char *nl;

  // Get command line
  char *temp = fgets(buffer, CMDLINE_MAX, stdin);

  // Print command line if stdin is not provided by terminal
  if (!isatty(STDIN_FILENO)) {
    printf("%s", buffer);
    fflush(stdout);
  }

  // Remove trailing newline from command line
  nl = strchr(buffer, '\n');
  if (nl)
    *nl = '\0';
}

int main(void) {
  char buffer[CMDLINE_MAX];
  struct command cmd;

  while (1) {
    int retval;

    /* Print prompt */
    printf("The One Piece is REAL!$ ");
    fflush(stdout);
    getCmd(buffer);

    /* Builtin command */
    if (!strcmp(buffer, "exit")) {
      fprintf(stderr, "Bye...\n");
      break;
    }

    // Copy cmd because strtok replaces the string's pointers with NULL when
    // parsing
    char temp[CMDLINE_MAX];
    strcpy(temp, buffer);

    int argLen = parseArgs(&cmd, temp);

    if (argLen > ARG_MAX)
      perror("Error: too many process arguments");

    if (!fork()) { /* Fork off child process */

      // execvp automatically locates to $PATH

      execvp(cmd.args[0], cmd.args); /* Execute command */
      perror("execv");               /* Coming back here is an error */
      exit(1);
    } else {
      /* Parent */
      waitpid(-1, &retval, 0); /* Wait for child to exit */
    }

    fprintf(stdout, "Return status value for '%s': %d\n", buffer, retval);
  }

  return EXIT_SUCCESS;
}