#!/bin/sh

rm -rf ./measure_1_out
mkdir ./measure_1_out


if [ -f "arandtrack" ]
then
    echo "Running single thread version"
    /usr/bin/time ./randtrack 1 100 >> ./measure_1_out/randtrack.out
   
   
fi

	



if [ -f "qrandtrack_global_lock" ]
then
    echo "Running global lock version"
    for i in 1 2 4; do
      /usr/bin/time ./randtrack_global_lock $i 100 > ./measure_1_out/randtrack_global_lock_$i.out
        
    done
fi

	


if [ -f "qrandtrack_tm" ]
then
    echo "Running transactional memory version"
    for i in 1 2 4; do
       /usr/bin/time ./randtrack_tm $i 100 > ./measure_1_out/randtrack_tm_$i.out
       
    done
fi

	

c=1
while [ $c -le 5 ]
do
if [ -f "arandtrack_list_lock" ]
then
    echo "Running list lock version"
    for i in 1 2 4; do
        /usr/bin/time ./randtrack_list_lock $i 100 > ./measure_1_out/randtrack_list_lock_$i.out
        
    done
fi

	(( c++ ))
done	


c=1
while [ $c -le 5 ]
do
if [ -f "randtrack_element_lock" ]
then
    echo "Running element lock version"
    for i in 1 2 4; do
        /usr/bin/time ./randtrack_element_lock $i 100 > ./measure_1_out/randtrack_element_lock_$i.out
        
    done
fi

	(( c++ ))
done	
	
if [ -f "arandtrack_reduction" ]
then
    echo "Running reduction version"
    for i in 1 2 4; do
        ./randtrack_reduction $i 50 > ./measure_1_out/randtrack_reduction_$i.out
        sort -n ./measure_out/randtrack_reduction_$i.out > ./measure_1_out/randtrack_reduction_$i.out
    done
fi
