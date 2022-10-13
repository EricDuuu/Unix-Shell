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

// Directory stack, Linked List
// Source http://www.cprogrammingnotes.com/question/dynamic-stack.html
struct dirstack {
  char memdir[CMDLINE_MAX];
  struct dirstack *next;
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
  // Allocate new command struct for pipeline
  (*current)->args[(*argLen)] = NULL;
  (*current)->next = (struct command *)malloc(sizeof(struct command));
  (*current) = (*current)->next;
  // Initialize Vars
  (*current)->next = NULL;
  (*current)->input = NULL;
  (*current)->output = NULL;
  (*argLen) = 0;

  if (*((*ptr) + 1) != '\0') { // Case: command |command, command|command
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
  if (fgets(buffer, CMDLINE_MAX, stdin) == NULL) {
    perror("fgets():");
    exit(1);
  }

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

void freeStack(struct dirstack **head) {
  struct dirstack *next = *head;
  
  while (next) {
    struct dirstack *nextTemp = next;
    next = next->next;
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

// Function which handles pushd
int pushd(struct dirstack **head, char *filename) {
  struct dirstack *target = (struct dirstack *)malloc(sizeof(struct dirstack));

  if (chdir(filename) == -1) {
    fprintf(stderr, "Error: no such directory\n");
    return 1;
  }
  // Gets current directory
  if (getcwd(target->memdir, CMDLINE_MAX) == NULL) {
    perror("getcwd():");
    exit(1);
  }

  // Moves the current directory to the top of the stack
  target->next = (*head);
  (*head) = target;
  return 0;
}

// Function which handles popd
int popd(struct dirstack **head) {
  struct dirstack *current = (*head);

  // Exit if there is no previous directory
  if ((*head)->next == NULL) {
    (*head) = (*head)->next;
    free(current);
    fprintf(stderr, "Error: directory stack empty\n");
    return 1;
  }

  // Change back to previous directory
  if (chdir(current->next->memdir) == -1) {
    fprintf(stderr, "Error: cannot cd into directory\n");
    return 1;
  }

  // Move top of the stack
  (*head) = (*head)->next;
  free(current);
  return 0;
}

// Function which handles dirs
void dirs(struct dirstack **head) {
  struct dirstack *current = (*head);
  // While there is a directory in the stack, print it and move to the next in
  // list
  while (current != NULL) {
    printf("%s\n", current->memdir);
    current = current->next;
  }
}

// Helper function to create pipes in the main process
int countPipes(struct command *cmd) {
  int count = 0;
  
  while (cmd != NULL) {
    count++;
    cmd = cmd->next;
  }
  return count - 1;
}

int builtin(struct command **current, struct dirstack **head) {
  int status = 0;
  char wdir[CMDLINE_MAX];

  if (strcmp((*current)->args[0], "pushd") == 0) { // Current cmd is pushd
    pushd(head, (*current)->args[1]);
    return status;

  } else if (strcmp((*current)->args[0], "popd") == 0) { // Current cmd is popd
    status = popd(head);
    return status;

  } else if (strcmp((*current)->args[0], "dirs") == 0) { // Current cmd is dirs
    dirs(head);
    return 0;

  } else if (strcmp((*current)->args[0], "cd") == 0) { // Current cmd is cd
    if (chdir((*current)->args[1]) == -1) {
      fprintf(stderr, "Error: cannot cd into directory\n");
      return 1;
    }
    return 0;

  } else if (strcmp((*current)->args[0], "pwd") == 0) { // Current cmd is pwd
    fprintf(stdout, "%s\n", getcwd(wdir, CMDLINE_MAX));
    return 0;
  }
  return -1;
}

// Function which handles execution of commands, piplining, and redirection
// Assume that input is sanitized for parsing
// RETURN -1 or 1 indicates a launching error
int execute(struct dirstack **head, struct command *cmd, char *buffer) {
  struct command *current = cmd;
  int pipeCount = countPipes(cmd);
  int wpipe[pipeCount][2];

  // Create all pipes beforehand
  for (int i = 0; i < pipeCount; i++) {
    if (pipe(wpipe[i]) < 0) {
      perror("Pipe creation:");
      exit(1);
    }
  }
  
  int statusLen = 0;
  int statusArr[ARG_MAX * pipeCount];
  int status = 0;

  if ((status = builtin(&current, head)) != -1) {
    fprintf(stderr, "+ completed '%s' [%d]\n", buffer, status);
    return status;
  }

  int currCommand = 0;

  // Reference
  // https://stackoverflow.com/questions/916900/having-trouble-with-fork-pipe-dup2-and-exec-in-c/
  while (current != NULL) {
    int pid = fork();

    if (pid == 0) { // Fork off child process
      
      // Child
      redirect(current);

      if (current != cmd) {
        if (dup2(wpipe[currCommand - 1][0], STDIN_FILENO) < 0) {
          perror("dup2");
          exit(1);
        }
      }

      if (current->next) {
        if (dup2(wpipe[currCommand][1], STDOUT_FILENO) < 0) {
          perror("dup2");
          exit(1);
        }
      }

      for (int i = 0; i < pipeCount; i++) {
        close(wpipe[i][0]);
        close(wpipe[i][1]);
      }

      execvp(current->args[0], current->args); // Execute command
      fprintf(stderr, "Error: command not found\n");
      exit(1);
      
    } else {
      currCommand++;
      current = current->next;
    }
  }

  for (int i = 0; i < pipeCount; i++) {
    close(wpipe[i][0]);
    close(wpipe[i][1]);
  }

  for (int i = 0; i < pipeCount + 1; i++) {
    wait(&status);
    
    if (i != 0 && status != 0) {
      statusArr[statusLen++] = WEXITSTATUS(status + 1);
      
    } else {
      statusArr[statusLen++] = WEXITSTATUS(status);
    }
  }
  fprintf(stderr, "+ completed '%s' ", buffer);
  
  for (int j = 0; j < statusLen; j++) {
    fprintf(stderr, "[%d]", statusArr[j]);
  }
  fprintf(stderr, "\n");
  return 0;
}

int main(void) {
  char buffer[CMDLINE_MAX];

  // Initialize directory stack head
  struct dirstack *dstack = (struct dirstack *)malloc(sizeof(struct dirstack));
  dstack->next = NULL;

  if (getcwd(dstack->memdir, CMDLINE_MAX) == NULL) {
    perror("getcwd():");
    exit(1);
  }

  while (1) {
    struct command cmd;
    cmd.next = NULL;
    cmd.input = NULL;
    cmd.output = NULL;

    // Print prompt
    printf("sshell@ucd$ ");
    fflush(stdout);
    getCmd(buffer);

    // Builtin command
    if (!strcmp(buffer, "exit")) {
      fprintf(stderr, "Bye...\n");
      fprintf(stderr, "+ completed '%s' [%d]\n", buffer, 0);
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
    execute(&dstack, &cmd, buffer);
    freeList(&cmd);
  }
  freeStack(&dstack);
  return EXIT_SUCCESS;
}