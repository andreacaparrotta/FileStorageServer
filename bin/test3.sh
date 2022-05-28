#!/bin/bash
startTime=$(date +%s);
check=0;
limit=30;
./bin/filestorage -f ./etc/server_socket -w ./Test1
while [ $check -lt $limit ]
do
    ./bin/filestorage -f ./etc/server_socket -W ./Test1/Test2/test2_3.txt,./Test1/Test3/test3_2.txt,./Test1/Test2/test2_1.txt -r ./Test1/Test2/test2_3.txt,./Test1/Test3/test3_2.txt,./Test1/Test2/test2_1.txt -l ./Test1/Test2/test2_3.txt -c ./Test1/Test2/test2_3.txt &
    endTime=$(date +%s);
    check=$(($endTime - $startTime));
done
pkill -SIGINT server