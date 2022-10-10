#include <fcntl.h>
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
    if (*symbol == '<')
      (*current)->input = strdup(*token);
    else
      (*current)->output = strdup(*token);

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
    // grabs input/output from after the meta char
    if (*symbol == '<')
      (*current)->input = strdup(*ptr + 1);
    else
      (*current)->output = strdup(*ptr + 1);

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
  if (head->next != NULL && head->output != NULL) {
    fprintf(stderr, "Error: mislocated output redirection\n");
    return -1;
  }

  if (tail->input != NULL && tail != head) {
    fprintf(stderr, "Error: mislocated input redirection\n");
    return -1;
  }

  // Error cmd | cmd > cmd | cmd , cmd | cmd < cmd | cmd
  head = head->next;
  while (head != NULL) {
    if (head == tail)
      break;

    if (head->output != NULL) {
      fprintf(stderr, "Error: mislocated output redirection\n");
      return -1;
    } else if (head->input != NULL) {
      fprintf(stderr, "Error: mislocated input redirection\n");
      return -1;
    }
    head = head->next;
  }

  return 0;
}

// Check if input and output files are valid files
int fileCheck(struct command *head, struct command *tail) {
  int fd = 0;
  if (head->input != NULL && open(head->input, O_RDONLY) == -1) {
    fprintf(stderr, "Error: cannot open input file\n");
    return -1;
  }
  fd = open(tail->output, O_WRONLY | O_CREAT, 0644);
  if (tail->output != NULL && fd == -1) {
    fprintf(stderr, "Error: cannot open output file\n");
    return -1;
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

  if (fileCheck(cmd, current) == -1)
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

// Function which handles input and output redirection
static void redirect(struct command *current) {

  if (current->input != NULL) {
    int fd = open(current->input, O_RDONLY);
    dup2(fd, STDIN_FILENO);
    close(fd);
  } else if (current->output != NULL) {
    int fd = open(current->output, O_WRONLY | O_TRUNC);
    dup2(fd, STDOUT_FILENO);
    close(fd);
  }
}

// Function which handles execution of commands, piplining, and redirection
// Assume that input is sanitized for parsing
// RETURN -1 indicates a launching error
int execute(struct command *cmd, int *retval) {
  struct command *current = cmd;

  while (current != NULL) {
    char wdir[ARGCHAR_MAX];
    int pid;

    if (!fork()) { // Fork off child process
      redirect(current);

      // Performs checks for cd or pwd
      if (strcmp(current->args[0], "cd") == 0) { // current cmd is cd
        if (chdir(current->args[1]) == -1) {
          fprintf(stderr, "Error: cannot cd into directory\n");
          *retval = 1;
          return -1;
        }
      } else if (strcmp(current->args[0], "pwd") == 0) { // current cmd is pwd
        printf("%s\n", getcwd(wdir, ARGCHAR_MAX));
      } else {
        execvp(current->args[0], current->args); // Execute command
        perror("execv");                         // Coming back here is an error
        exit(1);
      }
    } else {
      // Parent
      waitpid(-1, retval, 0); // Wait for child to exit
    }

    current = current->next;
  }
  return 0;
}

// NOTE: used for testing purposes (will delete in final product)
void printCMD(struct command *head) {
  // Print args[] array
  struct command *current = head;
  int i;

  while (current != NULL) {
    for (i = 0; i < ARG_MAX && current->args[i] != NULL; i++) {
      printf("%s ", current->args[i]);
    }
    printf("\n");
    if (current->input != NULL) {
      printf("<%s\n", current->input);
    }
    if (current->output != NULL) {
      printf(">%s\n", current->output);
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
    printf("sshell@ucd$ ");
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

    // printCMD(&cmd);

    execute(&cmd, &retval);

    /*
    if (!fork()) { // Fork off child process
      redirect(&cmd);
      // execvp automatically locates to $PATH
      execvp(cmd.args[0], cmd.args); // Execute command
      perror("execv");               // Coming back here is an error
      exit(1);
    } else {
      // Parent
      waitpid(-1, &retval, 0); // Wait for child to exit
    }
    */

    fprintf(stdout, "+ completed '%s' [%d]\n", buffer, retval);
    freeList(&cmd);
  }

  return EXIT_SUCCESS;
}