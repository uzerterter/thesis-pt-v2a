# MMAudio Standalone API - Persistent Logging Guide

## 🚀 Quick Start

### Option 1: Tmux Session (Recommended for Production)
```bash
# Inside Docker container
bash start_api_tmux.sh

# Access logs anytime (even if you disconnect)
tmux attach -t mmaudio-api

# Detach from session (API keeps running)
# Press: Ctrl+B, then D
```

### Option 2: File Logging (Simple)
```bash
# Start API with file logging
bash start_api_verbose.sh

# In another terminal: View logs anytime
bash view_logs.sh
```

## 📋 Log Management Commands

### View Logs (from anywhere)
```bash
# Follow live logs
bash view_logs.sh            # or: bash view_logs.sh tail

# Show complete log
bash view_logs.sh cat

# List all log files
bash view_logs.sh list

# Search logs
bash view_logs.sh grep "CACHE HIT"
bash view_logs.sh grep "ERROR"
bash view_logs.sh grep "generated"
```

### Tmux Session Management
```bash
# List all sessions
tmux ls

# Attach to API session
tmux attach -t mmaudio-api

# Kill session (stops API)
tmux kill-session -t mmaudio-api

# Detach from session
# Keyboard: Ctrl+B, then D
```

## 🐳 Docker Usage

### Start API in Container with Persistent Logging
```bash
# From host machine
docker exec -it mmaudio-api bash /workspace/model-tests/repos/thesis-pt-v2a/external/standalone-API/start_api_tmux.sh

# View logs from host
docker exec -it mmaudio-api bash /workspace/model-tests/repos/thesis-pt-v2a/external/standalone-API/view_logs.sh
```

### Access Logs from Host Machine
```bash
```bash
docker exec -it mmaudio-api bash /workspace/model-tests/repos/thesis-pt-v2a/standalone-API/start_api_tmux.sh
# OR (auto-attach):
docker exec -it mmaudio-api bash /workspace/model-tests/repos/thesis-pt-v2a/standalone-API/view_logs.sh
```

**Access logs from outside tmux:**
```bash
docker exec -it mmaudio-api tail -f /workspace/model-tests/repos/thesis-pt-v2a/standalone-API/logs/api_latest.log
```

# Or use docker logs (less detailed)
docker logs -f mmaudio-api
```

## 📁 Log Files Location

```
standalone-API/
├── logs/
│   ├── api_20251029_143022.log  # Timestamped log files
│   ├── api_20251029_150134.log
│   └── api_latest.log            # Symlink to most recent log
├── start_api_tmux.sh             # Start with tmux (persistent)
├── start_api_verbose.sh          # Start with file logging
└── view_logs.sh                  # View/search logs
```

## 🎯 Workflow Examples

### Development Workflow
```bash
# 1. Start API in tmux
bash start_api_tmux.sh

# 2. Detach (API keeps running)
# Press: Ctrl+B, D

# 3. Continue working, make requests
# API logs are continuously written

# 4. Check logs anytime
bash view_logs.sh grep "CACHE HIT"

# 5. Re-attach if needed
tmux attach -t mmaudio-api
```

### Production Workflow
```bash
# Start in tmux (survives SSH disconnects)
docker exec -d mmaudio-api bash /workspace/.../start_api_tmux.sh

# Monitor from anywhere
docker exec -it mmaudio-api bash /workspace/.../view_logs.sh
```

## 🔧 Troubleshooting

### "Session already exists"
```bash
# Attach to existing session
tmux attach -t mmaudio-api

# Or kill and restart
tmux kill-session -t mmaudio-api
bash start_api_tmux.sh
```

### "No log files found"
```bash
# Check if API is running
ps aux | grep python

# Start API first
bash start_api_verbose.sh
```

### Log file too large
```bash
# View only recent entries
tail -n 1000 logs/api_latest.log

# Or search specific time
grep "2025-10-29 14:" logs/api_latest.log
```

## 💡 Tips

- **Tmux shortcuts**:
  - `Ctrl+B, D` - Detach (API keeps running)
  - `Ctrl+B, [` - Scroll mode (use arrow keys, Q to exit)
  - `Ctrl+B, ?` - Help
  
- **Log rotation**: Old logs are kept, new ones created each restart
  
- **Performance**: Log files don't impact API performance (async writing)

## 🎓 Why This Setup?

1. **Persistent Sessions (tmux)**: SSH disconnect doesn't stop API
2. **File Logging**: Access logs without active terminal
3. **Easy Access**: Simple scripts for common tasks
4. **Searchable**: grep through logs for debugging
5. **Timestamped**: Historical logs for analysis
