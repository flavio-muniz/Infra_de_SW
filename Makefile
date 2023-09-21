all: shell.c
	gcc shell.c -g -o shell -pthread
	
clean:
	rm shell