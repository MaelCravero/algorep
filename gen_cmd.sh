#!/bin/sh

[ $# -ne 4 ] && { echo "usage: $0 NSERVER NCLIENT CMD_FILE NCMD"; exit 1; }

NSERVER=$1
NCLIENT=$2
CMD_FILE=$3
NCMD=$4

for i in $(seq $((NSERVER + 1)) $((NCLIENT + NSERVER))); do
    shuf -n $NCMD $CMD_FILE > ".commands_$i.txt"
done
