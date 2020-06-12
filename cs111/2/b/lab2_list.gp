#! /usr/bin/gnuplot
#
# purpose:
#	 generate graphs the complex critical section project (2b)
#
# input: lab2b_list.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
#	4. # lists
#	5. # operations performed (threads x iterations x (ins + lookup + delete))
#	6. run time (ns)
#	7. run time per operation (ns)
#	8. total wait time
#
# output:
#	lab2b_1.png ... throughput vs. number of threads for mutex and spin-lock synchronized list operations.
#	lab2b_2.png ... mean time per mutex wait and mean time per operation for mutex-synchronized list operations.
#	lab2b_3.png ... successful iterations vs. threads for each synchronization method.
#	lab2b_4.png ... throughput vs. number of threads for mutex synchronized partitioned lists.
#	lab2b_5.png ... throughput vs. number of threads for spin-lock-synchronized partitioned lists.
#
# Note:
#	Managing data is simplified by keeping all of the results in a single
#	file.  But this means that the individual graphing commands have to
#	grep to select only the data they want.
#
#	Early in your implementation, you will not have data for all of the
#	tests, and the later sections may generate errors for missing data.
#

# general plot parameters
set terminal png
set datafile separator ","

#1 Throughput vs. number of threads for mutex and spin-lock
set output 'lab2b_1.png'
set title "Total throughput for spin-lock and mutex"
set xlabel "# of Threads"
set logscale x 2
set logscale y 10
set ylabel "Throughput (operations/sec)"

# grep out only nthreads=(1|2|4|8|12|16|24) and iter=1000 for spin and mutex
plot \
     "< grep -E 'list-none-m,(1|2|4|8|12|16|24),1000,' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'mutex' with linespoints lc rgb 'blue', \
     "< grep -E 'list-none-s,(1|2|4|8|12|16|24),1000,' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'spin-lock' with linespoints lc rgb 'green'

#2 Mean wait time and average operation time vs nthreads for mutex
set output 'lab2b_2.png'
set title "Mean wait time and average operation time for mutex"
set xlabel "# of threads"
set xrange [0.75:]
set logscale y 10
set ylabel "time (nsec)"

# grep out only nthreads=(1|2|4|8|16|24) and iter=1000 for spin and mutex
plot \
     "< grep -E 'list-none-m,(1|2|4|8|16|24),1000,' lab2b_list.csv" using ($2):($7) \
     title 'average time/op' with linespoints lc rgb 'blue', \
     "< grep -E 'list-none-m,(1|2|4|8|16|24),1000,' lab2b_list.csv" using ($2):($8) \
     title 'avg wait time' with linespoints lc rgb 'green'



#3 Successful iterations vs. nthreads for mutex and spin-lock.
set output 'lab2b_3.png'
set title "Successful iterations vs nthreads"
set xlabel "# of threads"
set xrange [0.75:]
set logscale y 2
set ylabel "time (nsec)"

# grep out only nthreads=(1|4|8|12|16) for all, iter=(1|2|4|8|16) for no sync and
#       	   			        iter=(10|20|40|80) for spin and mutex
plot \
     "< grep -E 'list-id-none,(1|4|8|12|16),(1|2|4|8|16),' lab2b_list.csv" using ($2):($3) \
     title 'yield-none' with linespoints lc rgb 'blue', \
     "< grep -E 'list-id-m,(1|4|8|12|16),(10|20|40|80),' lab2b_list.csv" using ($2):($3) \
     title 'yield-mutex' with linespoints lc rgb 'green', \
     "< grep -E 'list-id-s,(1|4|8|12|16),(10|20|40|80),' lab2b_list.csv" using ($2):($3) \
     title 'yield-spin' with linespoints lc rgb 'red'

#4 Throughput vs. number of threads for mutex and spin-lock
set output 'lab2b_4.png'
set title "Mutex throughput for separated lists"
set xlabel "# of Threads"
set logscale x 2
set logscale y 10
set ylabel "Throughput (operations/sec)"

# grep out only nthreads=(1|2|4|8|12) and iter=1000 and list=(1,4,8,12) for mutex
plot \
     "< grep -E 'list-none-m,(1|2|4|8|12),1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'list=1' with linespoints lc rgb 'blue', \
     "< grep -E 'list-none-m,(1|2|4|8|12),1000,4' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'list=4' with linespoints lc rgb 'green', \
     "< grep -E 'list-none-m,(1|2|4|8|12),1000,8' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'list=8' with linespoints lc rgb 'red', \
     "< grep -E 'list-none-m,(1|2|4|8|12),1000,16' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'list=12' with linespoints lc rgb 'cyan'

#4 Throughput vs. number of threads for mutex and spin-lock
set output 'lab2b_5.png'
set title "Spin-lock throughput for separated lists"
set xlabel "# of Threads"
set logscale x 2
set logscale y 10
set ylabel "Throughput (operations/sec)"

# grep out only nthreads=(1|2|4|8|12) and iter=1000 and list=(1,4,8,12) for mutex
plot \
     "< grep -E 'list-none-s,(1|2|4|8|12),1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'list=1' with linespoints lc rgb 'blue', \
     "< grep -E 'list-none-s,(1|2|4|8|12),1000,4' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'list=4' with linespoints lc rgb 'green', \
     "< grep -E 'list-none-s,(1|2|4|8|12),1000,8' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'list=8' with linespoints lc rgb 'red', \
     "< grep -E 'list-none-s,(1|2|4|8|12),1000,16' lab2b_list.csv" using ($2):(1000000000/($7)) \
     title 'list=12' with linespoints lc rgb 'cyan'