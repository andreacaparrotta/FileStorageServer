#!/bin/bash
./bin/filestorage -f ./etc/server_socket -w ./Test1 -D ./etc/victim_files -p -t 200 &
./bin/filestorage -f ./etc/server_socket -W ./Test1/test1_4.txt,./Test1/Test3/test3_2.txt -p -t 400
sleep 2
./bin/filestorage -f ./etc/server_socket -w ./Test1 -D ./etc/victim_files -p -t 200 &
sleep 1
pkill -SIGHUP server