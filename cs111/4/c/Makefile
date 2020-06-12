.SILENT:

default:
	gcc -lm -Wall -Wextra -lmraa -g lab4c_tcp.c -o lab4c_tcp
	gcc -lssl -lcrypto -lm -Wall -Wextra -lmraa -g lab4c_tls.c -o lab4c_tls

clean:
	rm -f lab4c_tcp lab4c_tls *.o *.gz *.txt

dist:
	tar -czvf lab4c-000111000.tar.gz Makefile README lab4c_tcp.c lab4c_tls.c
