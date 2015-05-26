#!/bin/bash

i=2 # i is size (number of nodes)
while [ $i -le 6 ]
do
  ./waf --run "scratch/dus-line-net --size=$i --time=100" > output.txt
  i=$(($i + 1))
done
