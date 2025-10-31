#!/bin/bash
# Persistent API Startup Script with Logging

cd /workspace/thesis-pt-v2a/standalone-API

# Create logs directory
mkdir -p logs

# Generate log filename with timestamp
LOG_FILE="logs/api_$(date +%Y%m%d_%H%M%S).log"
LATEST_LOG="logs/api_latest.log"

echo "=========================================="
echo "Starting MMAudio Standalone API"
echo "Log file: $LOG_FILE"
echo "=========================================="
echo ""

# Start with logging to file AND stdout (using tee)
# -u flag ensures unbuffered output (real-time logging)
python3 -u main.py 2>&1 | tee "$LOG_FILE"

# Create symlink to latest log for easy access
ln -sf "$(basename "$LOG_FILE")" "$LATEST_LOG"
