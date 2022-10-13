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
respective conditions and edge cases. 

### Redirection


### Built In Commands
Once the commands and arguments entered into the prompt have been neatly 
parsed into the `struct command` linked list, function `execute()` calls 
function `builtin()` to recognize the built in commands cd, pwd, pushd, 
popd, and dirs through string comparison. These five commands can then be 
executed with assistance from the following three functions.

Function `pushd()` grabs the absolute pathway from the current working 
directory and places it within a buffer `target`. `pushd()` then assigns
this information to the front of the `struct dirstack` linked list, 
pointed to by `next` in the `dirstack` structure.

Function `popd()` determines if there exists a previous directory by 
accessing the next `struct dirstack` in the linked list, pointed to by 
`next` and checking if its value is NULL. If so, `popd()` will exit. 
Otherwise, it will change to this previous directory and remove its 
information from the front of the linked list.

Function `dirs` utilizes a buffer to iterate through all structures in
the `struct dirstack` linked list, printing each one to standard output.

Commands cd and pwd are directly executed through `builtin()` by calling 
`chdir()` and printing the results of `getcwd()` to standard output 
respectively. All other standard commands are executed through `execvp()`. 
These three functions have been provided by the GNU C Library manual.

### Pipelining

