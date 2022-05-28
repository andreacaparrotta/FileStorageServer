#!/bin/bash
./bin/filestorage -f ./etc/server_socket -w ./Test1,n=12 -D ./etc/victim_files -W ./Test1/test1_1.txt,./Test1/test1_2.txt -D ./etc/victim_files -r ./Test1/test1_1.txt,./Test1/Test3/test3_3.txt -d ./etc/saved_files -R n=5 -d ./etc/saved_files -c ./Test1/test1_2.txt -t 200 -p
sleep 1
pkill -SIGHUP memcheck