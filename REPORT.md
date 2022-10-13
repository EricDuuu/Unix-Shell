# SShell: Simple Shell
## Summary
The program, `sshell`, simulates a shell which can take the input of the 
user from the command line and executes them. It supports output and input 
redirection along with pipelining of commands. Additional features include 
optional arguments and built in commands such as `cd` `ls` and `pwd`.

## Implementation
There are many steps to implement this project:

1. Parsing commands from the user, redirects, built in commands, and
   pipelines and store data in a linked list.
2. Execution of the parsed commands and properly redirect and pipeline
   input and output files.

### Parsing
When executed, `sshell` initializes important storage data structures
`struct command` and `struct dirstack`. `struct command` is a linked
list that stores the command, input, and output of a single command.
`struct dirstack` is a linked list implemented as a stack which 
initializes with the current working directory. Stepping into the 
main loop, we use `getCMD()` to read the command line, store it in 
a buffer, and add a trailing NULL char. `parseArgs()` modifies and adds
to the `struct command` using string manipulation. This function calls
subfunctions `parsePipeline()` and `parseRedirect()`, which parses their
respective conditions and edge cases. Edge cases in this case are dealt
with string manipulation such as `strcmp`, `strcspn`, `strndup`, `strtok`. 
Note that `strndup` and `strdup` both allocate new memory so we need to 
free those blocks when cleaning up. `parseArgs()` and it's subfunctions all 
return -1 if there is an error which passes back to the main loop to cancel 
the operation without exiting the process. Error checking is also done by 
two helper functions which check if redirections are `mislocated()` and if 
the file redirecting to or from are valid using `fileCheck()`.

`struct command` is implemented as a linked list due to it's simularities 
to a pipeline and it's ability to have an arbitrary amount of nodes.

Splitting `parseArgs()` into subfunctions allows a cleaner code, but in
return we have to deal with pointers to pointers.

`struct command` should come out completely sanitized and all edge cases 
are covered and is ready to passed onto execution.

### Pipelining
Before executing, pipelines need to be setup beforehand due to the 
transfer limits between a single pipeline. We want all the commands
to be able to execute concurrently for any arbitrary input size.
First, we find how many pipelines are required using `countPipes()`.
Then, the pipes are created using a 2d array for ease of access.
Finally, the pipes are linked so the output of the first command 
goes to the input of the next command and so forth. The file descriptors
have to be closed inside of the child and the parent to stop errors.

### Redirection
`sshell` may perform both output and input redirection by way of the 
function `redirect()` found in the function `execute()`. By passing in a 
`struct command`, `redirect()` may access the strings `input` and `output`
associated with the structure. If there is a string assigned to `input`,
`redirect()` will open the corresponding file and bind its file
destination to that of standard input's, redirecting its contents into
`args[]`. This is done through `dup2()` found in the GNU C Library
manual. Following this process, the file is closed. `redirect()` may
redirect output through a similar process, instead checking for a string
assigned to `output`. The corresponding file will be opened, binded to
standard output by again utilizing `dup2()`, and then closing the file.

### Execution
After parsing and pipelining, `struct command` can be passed 
into `execution()`, which handles the execution, redirection, 
and built in commands. Additionally handles the errors in it's functions
and subfunctions such as the built in command in `builtin()`. 
`execution()` loops through `struct command` as the head of the linked
list is passed in. We create a temporary variable `current` which is
our iterator for the linked list. The process is forked and the child 
sets up the pipelines and executes the functions. `execvp()` is called
to execute the commands which locates to `PATH`. 

### Built In Commands
Once the commands and arguments entered into the prompt have been neatly 
parsed into the `struct command` linked list, function `execute()` calls 
function `builtin()` to recognize the built in commands cd, pwd, pushd, 
popd, and dirs through string comparison. These five commands can then
be executed with assistance from the following three functions.

Function `pushd()` utilizes `chdir()` from the GNU C Library manual to 
change into a specified directory passed in by way of `filename`. Then,
the absolute pathway is grabbed from the current working directory and
placed within a buffer `target`. `pushd()` then assigns this information
to the front of the `struct dirstack` linked list, pointed to by `next`
in the `dirstack` structure.

Function `popd()` determines if there exists a previous directory by 
accessing the next `struct dirstack` in the linked list, pointed to by 
`next` and checking if its value is NULL. If so, `popd()` will exit. 
Otherwise, it will change to this previous directory and remove its 
information from the front of the linked list, reassigning its data
to match that is pointed to by `next`.

Function `dirs` utilizes a buffer to iterate through all structures in
the `struct dirstack` linked list, printing each one to standard output.

Commands cd and pwd are directly executed through `builtin()` by calling 
`chdir()` and printing the results of `getcwd()` to standard output 
respectively. All other standard commands are executed through `execvp()`
in `execute()`. The `chdir()`, `getcwd()`, and `execvp()` functions have 
been provided by the GNU C Library manual.

### Error Management
Error management is done in their respective functions and all return `-1`
or `-1` for errors and `0` if the operation was successful. The errors for
parsing are passed back into the main loop from `parseArgs()` to continue 
the next input. Execution errors should break out of their `execute()`, 
but do nothing and continue onto the next input.

### Memory Management
Memory is allocated when a new node for `struct command` or `struct dirstack`
is created. Additionally `strdup` and `strndup` also allocate memory, but 
we store the pointers in `char* input` and `char* output` which are elements
in `struct command`. Thus, we free them both simultaneously in `freeList()`
and `freeStack()` for `struct command` and `struct dirstack` respectively.

### Sources
1. [Stack Reference](http://www.cprogrammingnotes.com/question/dynamic-stack.html)
2. [Freeing a Linked List](https://stackoverflow.com/questions/6417158/c-how-to-free-nodes-in-the-linked-list)
3. [Multi-pipeline execution](https://stackoverflow.com/questions/916900/having-trouble-with-fork-pipe-dup2-and-exec-in-c/)
