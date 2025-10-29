#!/bin/bash
# View API Logs - Access logs from anywhere

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="$SCRIPT_DIR/logs"
LATEST_LOG="$LOG_DIR/api_latest.log"

# Check if logs exist
if [ ! -d "$LOG_DIR" ]; then
    echo "❌ No logs directory found. API might not have been started yet."
    exit 1
fi

# Function to show available logs
list_logs() {
    echo "📋 Available log files:"
    echo "===================="
    ls -lht "$LOG_DIR"/*.log 2>/dev/null | head -10
    echo ""
}

# Parse arguments
case "${1:-tail}" in
    list|ls)
        list_logs
        ;;
    tail|follow|f)
        if [ -f "$LATEST_LOG" ]; then
            echo "📡 Following latest API logs (Ctrl+C to stop)..."
            echo "Log file: $(readlink -f "$LATEST_LOG")"
            echo ""
            tail -f "$LATEST_LOG"
        else
            echo "❌ No latest log file found."
            list_logs
            exit 1
        fi
        ;;
    cat|view|show)
        if [ -f "$LATEST_LOG" ]; then
            echo "📄 Showing complete log file:"
            echo "=============================="
            cat "$LATEST_LOG"
        else
            echo "❌ No latest log file found."
            list_logs
            exit 1
        fi
        ;;
    grep|search)
        if [ -z "$2" ]; then
            echo "Usage: $0 grep <pattern>"
            echo "Example: $0 grep 'CACHE HIT'"
            exit 1
        fi
        if [ -f "$LATEST_LOG" ]; then
            echo "🔍 Searching for: $2"
            echo "===================="
            grep --color=always "$2" "$LATEST_LOG"
        else
            echo "❌ No latest log file found."
            exit 1
        fi
        ;;
    help|--help|-h)
        echo "MMAudio API Log Viewer"
        echo "====================="
        echo ""
        echo "Usage: $0 [command]"
        echo ""
        echo "Commands:"
        echo "  tail, follow, f    - Follow logs in real-time (default)"
        echo "  cat, view, show    - Show complete log file"
        echo "  list, ls           - List all log files"
        echo "  grep <pattern>     - Search for pattern in logs"
        echo "  help               - Show this help"
        echo ""
        echo "Examples:"
        echo "  $0                 # Follow logs (same as 'tail')"
        echo "  $0 list            # List all log files"
        echo "  $0 grep 'CACHE'    # Search for cache-related logs"
        echo "  $0 grep 'ERROR'    # Find errors"
        ;;
    *)
        echo "❌ Unknown command: $1"
        echo "Run '$0 help' for usage information"
        exit 1
        ;;
esac
