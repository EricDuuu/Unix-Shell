all: sshell

sshell: sshell.c
	gcc -o sshell sshell.c

clean:
	rm -f sshell
