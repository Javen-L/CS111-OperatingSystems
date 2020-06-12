.SILENT:

default: lab1b-server.c lab1b-client.c
	gcc -Wall -Wextra -g -o lab1b-server lab1b-server.c -lmcrypt
	gcc -Wall -Wextra -g -o lab1b-client lab1b-client.c -lmcrypt
clean: 
	rm -f *.o *.txt *.tar.gz lab1b-server lab1b-client

dist:
	tar -czvf lab1b-000111000.tar.gz lab1b-server.c lab1b-client.c my.key README Makefile 

