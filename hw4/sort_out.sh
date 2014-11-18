#!/bin/sh

rm -rf ./out
mkdir ./out

if [ -f "randtrack" ]
then
    echo "Running single thread version"
    ./randtrack 1 50 > ./out/randtrack.out
    sort -n ./out/randtrack.out > ./out/randtrack.out
fi

if [ -f "randtrack_global_lock" ]
then
    echo "Running global lock version"
    for i in 1 2 4; do
        ./randtrack_global_lock $i 50 > ./out/randtrack_global_lock_$i.out
        sort -n ./out/randtrack_global_lock_$i.out > ./out/randtrack_global_lock_$i.out
    done
fi