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

// Helper function to check if a char is any of these symbols
int isMeta(char symbol) {
  return symbol == '<' || symbol == '>' || symbol == '|';
}

// Uses string manipulation to separate the command and meta char and inserts
// data into command struct. Notable string functions: strcmp, strcspn, strndup
// RETURN -1 for parsing errors
int parseRedirect(char **ptr, char **token, struct command **current,
                  int *argLen, char *symbol) {

  // Case: command < input and command< input
  if (*(*(ptr) + 1) == '\0') {

    // Case: command< input requires parsing of string prior to meta char
    if (strcmp(*token, symbol) != 0) {
      int pos = strcspn(*token, symbol);
      (*current)->args[(*argLen)] = strndup(*token, pos);
      (*argLen)++;
    }

    // Grab the isolated input
    *token = strtok(NULL, " ");
    if (*token == NULL || isMeta(**token)) { // Error: command < |
      switch (*symbol) {
      case '>':
        fprintf(stderr, "Error: no output file\n");
        break;
      case '<':
        fprintf(stderr, "Error: no input file\n");
        break;
      }
      return -1;
    }
    (*current)->input = strdup(*token);

  } else { // Case: command <input and command<input

    if (*(*(ptr) + 1) == '\0' || isMeta(*(*(ptr) + 1))) { // Error: command <|
      switch (*symbol) {
      case '>':
        fprintf(stderr, "Error: no output file\n");
        break;
      case '<':
        fprintf(stderr, "Error: no input file\n");
        break;
      }
      return -1;
    }
    // grabs input from after the meta char
    (*current)->input = strdup(*(ptr) + 1);

    // Case: command<input requires parsing of string prior to meta char
    if (**ptr != **token) {
      int pos = strcspn(*token, symbol);
      (*current)->args[(*argLen)] = strndup(*token, pos);
      (*argLen)++;
    }
  }

  return 0;
}

// RETURN -1 if length of args go past max length
int errorArgLen(const int *argLen) {
  if (*argLen > ARG_MAX - 1) {
    fprintf(stderr, "Error: too many process arguments\n");
    return -1;
  }
  return 0;
}

// Modular pipeline parser, pass by reference to modify linked list.
// New node is allocated and pointer is modified to the last element in linked
// list.
// RETURN -1 for parsing errors
int parsePipeline(char **ptr, char **token, struct command **current,
                  int *argLen) {
  // Cases: cmd| cmd cmd |cmd
  if ((*((*ptr) + 1) == '\0' || **ptr != **token) && strcmp(*token, "|") != 0) {
    int pos = strcspn(*token, "|");
    (*current)->args[(*argLen)] = strndup(*token, pos);
    (*argLen)++;
  }
  // Cases: cmd | cmd
  (*current)->args[(*argLen)] = NULL;
  (*current)->next = (struct command *)malloc(sizeof(struct command));
  (*current) = (*current)->next;
  // Initialize Vars
  (*current)->next = NULL;
  (*current)->input = NULL;
  (*current)->output = NULL;
  (*argLen) = 0;

  if (*((*ptr) + 1) != '\0') // Case: command |command, command|command
  {
    if (isMeta(*((*ptr) + 1))) { // Error: command |<
      fprintf(stderr, "Error: missing command\n");
      return -1;
    }

    (*current)->args[(*argLen)] = (*ptr) + 1;
    (*argLen)++;
  } else {
    *token = strtok(NULL, " ");
    if (*token == NULL || isMeta(**token)) { // Error: command | , command | <
      fprintf(stderr, "Error: missing command\n");
      return -1;
    }
    (*current)->args[(*argLen)] = *token;
    (*argLen)++;
  }

  return 0;
}

// Finds mislocated redirects by traversing linked list
int mislocated(const struct command *head, const struct command *tail) {

  // Error: cmd > output | cmd, cmd| cmd < input
  if (head->next != NULL && head->output != NULL && tail->input != NULL) {
    fprintf(stderr, "Error: mislocated output redirection\n");
    return -1;
  }

  // Error cmd | cmd > cmd | cmd , cmd | cmd < cmd | cmd
  head = head->next;
  while (head != NULL) {
    if (head == tail)
      break;
    if (head->input != NULL || head->output != NULL) {
      fprintf(stderr, "Error: mislocated output redirection\n");
      return -1;
    }
    head = head->next;
  }

  return 0;
}

// RETURN;  -1: error, errors are printed in respective error function
// strtok to parse through each separated arguments, returns argLen for
// further use
int parseArgs(struct command *cmd, char *buffer) {
  struct command *current = cmd;
  int argLen = 0;

  char *token = strtok(buffer, " ");
  while (token != NULL) {

    if (errorArgLen(&argLen) == -1)
      return -1;

    if ((*token == '<' || *token == '>' || *token == '|') && argLen == 0) {
      fprintf(stderr, "Error: missing command\n");
      return -1;
    }

    // strpbrk sets pointer to the char found in the string
    char *ptr = strpbrk(token, "><|");
    char *test = current->input;

    if (ptr != NULL) {
      switch (*ptr) {
      case '<':
        if (parseRedirect(&ptr, &token, &current, &argLen, "<") == -1)
          return -1;
        break;

      case '>':
        if (parseRedirect(&ptr, &token, &current, &argLen, ">") == -1)
          return -1;
        break;

      case '|':
        if (parsePipeline(&ptr, &token, &current, &argLen) == -1)
          return -1;
        break;
      }

    } else { // Case where there aren't any special symbols
      current->args[argLen++] = token;
    }
    token = strtok(NULL, " ");
  }
  // Last command in pipeline or when there are no pipelines
  current->args[argLen] = NULL;

  if (mislocated(cmd, current) == -1)
    return -1;

  return 0;
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

// Code to free up a linked list
// I forgot how to do so I looked it up:
// https://stackoverflow.com/questions/6417158/c-how-to-free-nodes-in-the-linked-list
void freeList(struct command *head) {
  // head is not dynamically allocated

  // strndup and strdup both allocate memory and need to be freed
  if (head->input != NULL)
    free(head->input);
  else if (head->output != NULL)
    free(head->output);

  struct command *next = head->next;
  while (next) {
    struct command *nextTemp = next;
    next = next->next;
    if (nextTemp->input != NULL)
      free(nextTemp->input);
    if (nextTemp->output != NULL)
      free(nextTemp->output);
    free(nextTemp);
  }
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

  while (1) {
    struct command cmd;
    cmd.next = NULL;
    cmd.input = NULL;
    cmd.output = NULL;
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

    // Parsing Errors are automatically passed back
    if (parseArgs(&cmd, bufferCopy) == -1) {
      freeList(&cmd);
      continue;
    }

    execute(&cmd, &retval);

    fprintf(stdout, "Return status value for '%s': %d\n", buffer, retval);
    freeList(&cmd);
  }

  return EXIT_SUCCESS;
}