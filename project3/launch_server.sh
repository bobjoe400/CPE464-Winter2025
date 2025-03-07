#!/bin/bash
# launch_server.sh - Wrapper to launch the server, capture its port and PID, then exit.

rm -f .server_port .server_pid .server_output.log

server_exe="./server"

# Start the server detached in a subshell, then detach that subshell
(setsid nohup stdbuf -oL "$server_exe" "$@" > .server_output.log 2>&1 < /dev/null &) &

subshell_pid=$! # Get the pid of the subshell

# Optionally, disown the process (though setsid should already detach it)
disown $subshell_pid

# Poll the log file for the server PID (with a timeout)
server_pid=""
timeout=10 # seconds
while [[ $timeout -gt 0 && -z "$server_pid" ]]; do
    sleep 1
    server_pid=$(grep -oP "Server PID:\s*\K\d+" .server_output.log)
    timeout=$((timeout - 1))
done

if [ -n "$server_pid" ]; then
    echo "$server_pid"
    echo "$server_pid" > .server_pid

    # Now poll for the port (after getting the PID)
    port=""
    timeout=10 # seconds
    while [[ $timeout -gt 0 && -z "$port" ]]; do
        sleep 1
        port=$(grep -oP "Server using Port #:\s*\K\d+" .server_output.log)
        timeout=$((timeout - 1))
    done

    if [ -n "$port" ]; then
        echo "Captured port: $port"
        echo "$port" > .server_port
    else
        echo "Failed to capture port within timeout"
    fi
else
    echo "Failed to capture server PID within timeout"
fi

exit 0