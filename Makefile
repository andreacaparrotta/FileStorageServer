CC = gcc
CFLAGS = -I./source -Wall -g

DBG = valgrind
DBGFLAGS = --track-origins=yes --leak-check=full --show-leak-kinds=all -s

server_dep = ./source/server/server_main.c ./source/server/worker.h ./source/server/storage_manager.h ./source/server/request_queue.h ./source/server/resolved_queue.h ./source/definitions.h
server_bin = ./bin/server

client_dep = ./source/client/client_main.c ./source/client/api.h ./source/definition.h
client_bin = ./bin/filestorage
client_args = -f ./etc/server_socket -w ./Test1,n=2 -p

./bin/server: $(server_dep)
			  $(CC) $(CFLAGS) $< -o $@

./bin/filestorage: $(client_dep)
			  	   $(CC) $(CFLAGS) $< -o $@


all:
	$(CC) $(CFLAGS) ./source/server/server_main.c -o $(server_bin) -pthread
	$(CC) $(CFLAGS) ./source/client/client_main.c -o $(client_bin) -lm -lrt

clean:
	rm ./etc/server_socket -f
	rm ./etc/log.txt -f
	rm ./etc/saved_files/* -f
	rm ./etc/victim_files/* -f
	rm ./bin/server -f
	rm ./bin/filestorage -f

test1:
	make all
	echo "# Il numero di thread che compongono il thread pool\nn_thread:1\n# La dimensione massima dello storage espressa in Mbyte\nb_storage:128\n# Il numero massimo di file che possono essere presenti contemporaneamente nello storage\nn_file_storage:10000\n# Il filename del socket di ascolto del server\nsoc_filename:./etc/server_socket\n# Il numero massimo di connessioni in attesa di essere accettate\nmax_conn_wait:10\n# Il numero massimo di connessioni attive contemporaneamente\nmax_active_conn:20\n# Il timeout di attesa del server\nmanager_timeout:10\n# Il file name del file di log\nlog_filename:./etc/log.txt\n# Il timeout per chiudere le connessioni inutilizzate con i client, specificato in secondi\nclient_timeout:60" > ./etc/config.txt
	$(DBG) --leak-check=full $(server_bin) &
	chmod +x ./bin/test1.sh
	sed -i 's/\r//' bin/test1.sh
	./bin/test1.sh
	
test2:
	make all
	echo "# Il numero di thread che compongono il thread pool\nn_thread:4\n# La dimensione massima dello storage espressa in Mbyte\nb_storage:1\n# Il numero massimo di file che possono essere presenti contemporaneamente nello storage\nn_file_storage:10\n# Il filename del socket di ascolto del server\nsoc_filename:./etc/server_socket\n# Il numero massimo di connessioni in attesa di essere accettate\nmax_conn_wait:10\n# Il numero massimo di connessioni attive contemporaneamente\nmax_active_conn:20\n# Il timeout di attesa del server\nmanager_timeout:10\n# Il file name del file di log\nlog_filename:./etc/log.txt\n# Il timeout per chiudere le connessioni inutilizzate con i client, specificato in secondi\nclient_timeout:60" > ./etc/config.txt
	$(server_bin) &
	chmod +x ./bin/test2.sh
	sed -i 's/\r//' bin/test2.sh
	./bin/test2.sh
	
test3:
	make all
	echo "# Il numero di thread che compongono il thread pool\nn_thread:8\n# La dimensione massima dello storage espressa in Mbyte\nb_storage:32\n# Il numero massimo di file che possono essere presenti contemporaneamente nello storage\nn_file_storage:100\n# Il filename del socket di ascolto del server\nsoc_filename:./etc/server_socket\n# Il numero massimo di connessioni in attesa di essere accettate\nmax_conn_wait:10\n# Il numero massimo di connessioni attive contemporaneamente\nmax_active_conn:20\n# Il timeout di attesa del server\nmanager_timeout:10\n# Il file name del file di log\nlog_filename:./etc/log.txt\n# Il timeout per chiudere le connessioni inutilizzate con i client, specificato in secondi\nclient_timeout:60" > ./etc/config.txt
	$(server_bin) &
	chmod +x ./bin/test3.sh
	sed -i 's/\r//' bin/test3.sh
	./bin/test3.sh