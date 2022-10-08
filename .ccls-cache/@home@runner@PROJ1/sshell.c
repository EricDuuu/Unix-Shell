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

// Uses string manipulation to separate the command and meta char and inserts
// data into command struct. Notable string functions: strcmp, strcspn, strndup
int parseRedirect(char **ptr, char **token, struct command **current,
                  int *totalLen, int *argLen, char *symbol) {

  // Case: command < input and command< input
  if (*(*(ptr) + 1) == '\0') {

    // Case: command< input requires parsing of string prior to meta char
    if (strcmp(*token, symbol) != 0) {
      int pos = strcspn(*token, symbol);
      (*current)->args[*argLen++] = strndup(*token, pos);
    }
    // Grab the isolated input
    *token = strtok(NULL, " ");
    (*current)->input = strdup(*token);

  } else { // Case: command <input and command<input

    // grab's input from after the meta char
    (*current)->input = strdup(*(ptr) + 1);

    // Case: command<input requires parsing of string prior to meta char
    if (**ptr != **token) {
      int pos = strcspn(*token, symbol);
      (*current)->args[*argLen++] = strndup(*token, pos);
    }
  }
}

// RETURN;  -1: error, errors are printed in the error function
// strtok to parse through each separated arguments, returns argLen for
// further use
int parseArgs(struct command *cmd, char *buffer) {
  struct command *current = cmd;

  // totalLen for the cases where there is pipelining
  int argLen = 0;
  int totalLen = 0;

  char *token = strtok(buffer, " ");
  while (token != NULL) {

    char *ptr = strpbrk(token, "><|");
    char *test = current->input;
    if (ptr != NULL) {
      // Cases: command< input, command <input, command<input
      // where there are no spaces in between

      switch (*ptr) {
      case '<':
        parseRedirect(&ptr, &token, &current, &totalLen, &argLen, "<");
        totalLen++;
        argLen++;
        break;

      case '>':
        parseRedirect(&ptr, &token, &current, &totalLen, &argLen, ">");
        totalLen++;
        argLen++;
        break;

      case '|':
        // Cases: cmd| cmd cmd |cmd
        if ((*(ptr + 1) == '\0' || *ptr != *token) && strcmp(token, "|") == 0) {
          int pos = strcspn(token, "|");
          current->args[argLen++] = strndup(token, pos);
        }
        // Cases: cmd | cmd
        current->args[argLen] = NULL;
        current->next = (struct command *)malloc(sizeof(struct command));
        current = current->next;
        argLen = 0;

        if (*(ptr + 1) != '\0') // Case: command |command, command|command
          current->args[argLen++] = ptr + 1;
        break;
      }

    } else { // Case where there aren't any special symbols
      current->args[argLen++] = token;
      totalLen++;
    }
    token = strtok(NULL, " ");
  }
  // Last command in pipeline or when there are no pipelines
  current->args[argLen] = NULL;
  return totalLen;
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

static void execute(struct command *cmd, int *retval) {
  int pid;
  struct command *current = cmd;

  while (!current) {
    char wdir[ARGCHAR_MAX];

    // Performs checks for cd or pwd
    if (strcmp(current->args[0], "cd")) { // current cmd is cd
      chdir(current->args[1]);

      if (chdir(current->args[1]) != 0) {
        perror("naur");
      }
    } else if (strcmp(current->args[0], "pwd")) { // current cmd is pwd
      printf("%s\n", getcwd(wdir, ARGCHAR_MAX));
    } else if (!fork()) { // Fork off child process
      // execvp automatically locates to $PATH
      execvp(current->args[0], current->args); // Execute command
      perror("execv");                         // Coming back here is an error
      exit(1);
    } else {
      // Parent
      waitpid(-1, retval, 0); // Wait for child to exit
    }
    current = current->next;
  }
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

    execute(&cmd, &retval);

    fprintf(stdout, "Return status value for '%s': %d\n", buffer, retval);
  }

  return EXIT_SUCCESS;
}