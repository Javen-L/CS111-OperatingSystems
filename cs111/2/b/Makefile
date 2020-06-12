.SILENT:

default:
	gcc -lprofiler -pthread -lrt -Wall -Wextra -g SortedList.c  -o lab2_list lab2_list.c 

clean: 
	rm -rf lab2b-000111000.tar.gz lab2_list

tests: default
	rm -rf lab2b_list.csv *.png
	chmod +x tests.sh
	-./tests.sh

profile: default
	rm -f raw.gperf
	LD_PRELOAD=/u/eng/class/classyou/lab2b/gperf/lib/libprofiler.so
	CPUPROFILE=raw.gperf ./lab2_list --threads=12 --iterations=1000 --sync=s
	/u/eng/class/classyou/gperftools/src/pprof --text lab2_list raw.gperf > profile.out
	/u/eng/class/classyou/gperftools/src/pprof --list=threadFun lab2_list raw.gperf >> profile.out
	rm -f raw.gperf

graphs: #tests
	rm -f *.png
	chmod +x lab2_list.gp
	./lab2_list.gp

dist: clean default tests graphs
	tar -czvf lab2b-404610095.tar.gz Makefile README SortedList.h SortedList.c \
	lab2_list.c profile.out tests.sh *.csv *.png *.gp

