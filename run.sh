#!/bin/sh
array=(1000 10000 100000 1000000 10000000)
name=("./cceh1.log" "./cceh2.log" "./cceh3.log" "./cceh4.log" "./cceh5.log")
for((k=1;k<=10;++k))
do
    for((j=0;j<10;++j))
    do
        for((i=0;i<5;++i))
        do
            make clean;
            make CFLAG=-DDEBUG_TIME;
            ./dynamic_hash ${array[i]} ${k} >> ${name[j]};
        done
    done
done