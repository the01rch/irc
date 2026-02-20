#!/bin/bash
# IRC Server Monitor - Restarts ircserv if not running

WORK_DIR="/home/deploy/42_irc"
LOG_FILE="/home/deploy/ircserv_monitor.log"

# Check if ircserv is running
if ! pgrep -x ircserv > /dev/null; then
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] ircserv not running, restarting..." >> "$LOG_FILE"
    
    cd "$WORK_DIR" || exit 1
    nohup ./ircserv >> ircserv.log 2>&1 &
    
    sleep 2
    
    if pgrep -x ircserv > /dev/null; then
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] ircserv restarted successfully" >> "$LOG_FILE"
    else
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] FAILED to restart ircserv" >> "$LOG_FILE"
    fi
fi
