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

// Implemented with pipelining and redirection in mind, Linked List
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
  struct command *current = cmd;

  int argLen = 0;

  char *token = strtok(buffer, " ");
  while (token != NULL) {

    char *ptr = strpbrk(token, "><|");
    char *test = current->input;
    // Case where the symbol is spaced between commands
    if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0 ||
        strcmp(token, "|") == 0) {

      switch (*token) {
      // TODO: Redirection, < is only allowed in first command pipeline, > is
      // only allowed at last command of pipeline
      case '<':
        token = strtok(NULL, " ");
        current->input = strdup(token);
        break;

      case '>':
        token = strtok(NULL, " ");
        current->output = strdup(token);
        break;

      case '|':
        // Next iteration begins at the command after the pipeline
        current->next = (struct command *)malloc(sizeof(struct command));
        current = current->next;
        break;
      }

      argLen++;

    } else if (ptr != NULL) { // Case where there are spaces
      argLen++;
    } else { // Case where there aren't any special symbols
      current->args[argLen++] = token;
    }

    token = strtok(NULL, " ");
  }

  // Piping and redirection; Should move to parsing

  /*
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
  */

  // Needs to end in NULL to pass into execvp
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
  cmd.next = NULL;

  while (1) {
    int retval;

    // Print prompt
    printf("The One Piece is REAL!$ ");
    fflush(stdout);
    getCmd(buffer);

    // Builtin command
    if (!strcmp(buffer, "exit")) {
      fprintf(stderr, "Bye...\n");
      break;
    }

    // We copy buffer because strtok replaces delimiters with NULL chars
    char bufferCopy[CMDLINE_MAX];
    strcpy(bufferCopy, buffer);
    int argLen = parseArgs(&cmd, bufferCopy);

    if (argLen > ARG_MAX)
      perror("Error: too many process arguments");

    if (!fork()) { // Fork off child process

      // execvp automatically locates to $PATH
      execvp(cmd.args[0], cmd.args); // Execute command
      perror("execv");               // Coming back here is an error
      exit(1);
    } else {
      // Parent
      waitpid(-1, &retval, 0); // Wait for child to exit
    }

    fprintf(stdout, "Return status value for '%s': %d\n", buffer, retval);
  }

  return EXIT_SUCCESS;
}