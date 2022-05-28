#!/bin/bash
openlock_counter=0
write_counter=0
write_bytes=0
read_counter=0
read_bytes=0
lockfile_counter=0
unlockfile_counter=0
closefile_counter=0
max_storage_size=0
max_n_storage_size=0
replaced_file=0
max_active_con=0

while IFS= read -r line
do
    if [[ "${line:0:8}" = "openlock" ]]; then
        openlock_counter=$(($openlock_counter + 1))
    fi

    if [[ "${line:0:6}" = "write:" ]]; then
        bytes=${line#write:}

        write_bytes=$(( $write_bytes+$bytes ))
        write_counter=$(( $write_counter+1 ))
    fi

    if [[ "${line:0:5}" = "read:" ]]; then
        bytes=${line#read:}

        read_bytes=$(( $read_bytes + $bytes ))
        read_counter=$(( $read_counter + 1 ))
    fi

    if [[ "${line:0:8}" = "lockfile" ]]; then
        lockfile_counter=$(($lockfile_counter + 1))
    fi

    if [[ "${line:0:10}" = "unlockfile" ]]; then
        unlockfile_counter=$(($unlockfile_counter + 1))
    fi

    if [[ "${line:0:9}" = "closefile" ]]; then
        closefile_counter=$(($closefile_counter + 1))
    fi

    if [[ "$line" == *"maxsize"* ]]; then
        max_storage_size=${line#maxsize:}
    fi

    if [[ "$line" == *"maxnsize"* ]]; then
        max_n_storage_size=${line#maxnsize:}
    fi

    if [[ "$line" == *"replacedfiles"* ]]; then
        replaced_file=${line#replacedfiles:}
    fi

    if [[ "$line" == *"maxactiveconn"* ]]; then
        max_active_con=${line#maxactiveconn:}
    fi

    if [[ "$line" == *"servedrequest"* ]]; then
        echo "Numero di richieste servite dal worker ${line:14:1}: ${line:16:10}"
    fi
done < "./etc/log.txt"

if [[ $write_counter -ne 0 ]]; then
    avg_w=`echo "($write_bytes/$write_counter)/1000000" | bc -l`
fi


if [[ $read_counter -ne 0 ]]; then
    avg_r=`echo "($read_bytes/$read_counter)/1000000" | bc -l`
fi

avg_w=`echo "$avg_w" | sed -r 's/[.]+/,/g'`
avg_r=`echo "$avg_r" | sed -r 's/[.]+/,/g'`

echo "Numero di open con flag O_LOCK settato: $openlock_counter"
printf "Numero di scritture: $write_counter: %0.6f Mbyte scritti in media\n" "$avg_w"
printf "Numero di letture: $read_counter: %0.6f Mbyte letti in media\n" "$avg_r"
echo "Numero di lock: $lockfile_counter"
echo "Numero di unlock: $unlockfile_counter"
echo "Numero di chiusure di file: $closefile_counter"
echo "Dimensione massima raggiunta dallo storage: $max_storage_size byte"
echo "Dimensione massima in numero di file raggiunta dallo storage: $max_n_storage_size"
echo "Numero di volte che è stato usato l'algoritmo di rimpiazzamento: $replaced_file"

echo "Numero massimo di connessioni attive contemporaneamente: $max_active_con"

