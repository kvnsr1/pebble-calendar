#!/bin/sh
set -eu
. "$(dirname "$0")/env.sh"
cd "$PROJECT_DIR"
node --test test/*.test.js
mkdir -p test-output
cc -std=c11 -Wall -Wextra -Werror -Itest/native -Isrc/c test/native/test_calendar.c src/c/event_model.c src/c/event_store.c src/c/room_notes.c src/c/room_input.c -o test-output/test-calendar
./test-output/test-calendar
cc -std=c11 -Wall -Wextra -Werror -Itest/native -Isrc/c test/native/test_room.c src/c/event_model.c src/c/event_store.c src/c/room_notes.c src/c/room_input.c -o test-output/test-room
./test-output/test-room
