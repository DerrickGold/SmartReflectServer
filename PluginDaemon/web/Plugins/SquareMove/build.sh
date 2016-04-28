#!/bin/bash

#comFile="$1"
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
#BIN="$DIR/move.bin"

#build, then execute the plugin
#gcc "$DIR/client.c" -lpthread -L/usr/local/lib -lwebsockets -o "$BIN" && \
#    chmod +x "$BIN" \
#    && "$BIN" "$comFile"

cd $DIR && make clean && make && ./move "$1"
