#!/bin/bash

#tests for lab2b_1.png
nthreads=(1, 2, 4, 8, 12, 16, 24)
niter=1000
synctype=(m, s)

echo "tests for lab2_1.png and lab2_2.png"
for i in "${nthreads[@]}"; do
    for k in "${synctype[@]}"; do
	./lab2_list --threads=$i --iterations=$niter --sync=$k >> lab2b_list.csv
    done
done

#iter=(1|2|4|8|16) for no sync and
#iter=(10|20|40|80) for spin and mutex


echo "tests for lab2_3.png"
nthreads2=(1, 4, 8, 12, 16)
niter2nosync=(1, 2, 4, 8, 16)
niter2sync=(10, 20, 40, 80)
nlists2=4

for i in "${nthreads2[@]}"; do
    for k in "${niter2nosync[@]}"; do
	./lab2_list --threads=$i --iterations=$k --yield=id --lists=$nlists2 >> lab2b_list.csv
    done
done

for i in "${nthreads2[@]}"; do
    for k in "${niter2sync[@]}"; do
	for j in "${synctype[@]}"; do
            ./lab2_list --threads=$i --iterations=$k --yield=id --sync=$j --lists=$nlists2 >> lab2b_list.csv
        done
    done
done



echo "tests for lab2_4.png and lab2_5.png"
nthreads3=(1, 2, 4, 8, 12)
niter3=1000
nlists3=(1, 4, 8, 16)

for i in "${nthreads3[@]}"; do
    for k in "${nlists3[@]}"; do
	for j in "${synctype[@]}"; do
	    ./lab2_list --threads=$i --iterations=$niter3 --sync=$j --lists=$k >> lab2b_list.csv
	done
    done
done
