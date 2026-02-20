#!/bin/bash
# Simple IRC client wrapper that adds CRLF for proper IRC protocol
# Usage: ./irc_client.sh [host] [port]

HOST=${1:-localhost}
PORT=${2:-6667}

echo "Connecting to IRC server at $HOST:$PORT"
echo "Type your messages and press Enter. Press Ctrl+C to exit."
echo "---"

# Use named pipes for bidirectional communication
FIFO=$(mktemp -u)
mkfifo "$FIFO"

# Start netcat in background, reading from FIFO
nc "$HOST" "$PORT" < "$FIFO" &
NC_PID=$!

# Cleanup function
cleanup() {
    kill $NC_PID 2>/dev/null
    rm -f "$FIFO"
    exit 0
}
trap cleanup EXIT INT TERM

# Read user input and write to FIFO with CRLF
while IFS= read -r line; do
    printf "%s\r\n" "$line" > "$FIFO"
done

cleanup
