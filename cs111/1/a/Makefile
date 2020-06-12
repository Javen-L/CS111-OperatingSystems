.SILENT:

default:
	gcc -Wall -Wextra -g -o lab1a lab1a.c

clean: checkIO checkCatch
	rm -f *.o *.txt *.tar.gz

dist:
	tar -czvf lab1a-000111000.tar.gz lab1a.c README Makefile 

