all: sshell

sshell: sshell.c
	gcc -g -o sshell sshell.c

clean:
	rm -f sshell
