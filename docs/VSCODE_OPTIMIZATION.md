# VS Code Performance Optimization Guide

## Current Issue
VS Code is hanging due to high system load from the intensive real-time dashboard application with multiple WebSocket connections and frequent updates.

## Immediate Solutions Applied

### 1. Stopped Resource-Intensive Processes
- Terminated all dashboard demo processes
- Killed WebSocket connections to exchanges
- Reduced background CPU/memory usage

### 2. System Resource Optimization

#### Current System Load
- Load Average: 3.55 (high)
- CPU Usage: 16.55% user, 12.41% sys
- Memory: 6986M used, 648M unused
- Multiple background processes consuming resources

## VS Code Performance Optimization Tips

### 1. VS Code Settings Optimization
```json
{
    "extensions.autoUpdate": false,
    "extensions.autoCheckUpdates": false,
    "workbench.enableExperiments": false,
    "telemetry.telemetryLevel": "off",
    "search.followSymlinks": false,
    "search.useIgnoreFiles": true,
    "files.watcherExclude": {
        "**/node_modules/**": true,
        "**/build/**": true,
        "**/external/**": true,
        "**/.git/**": true
    },
    "typescript.updateImportsOnFileMove.enabled": "never",
    "typescript.suggest.autoImports": false
}
```

### 2. Workspace Optimization
- Exclude heavy directories from file watching
- Disable unnecessary extensions for this workspace
- Reduce IntelliSense scope

### 3. System-Level Optimizations

#### Memory Management
```bash
# Clear system caches
sudo purge

# Check memory pressure
memory_pressure

# Monitor system resources
top -o cpu
```

#### Process Management
```bash
# Kill VS Code cleanly if it hangs
pkill -f "Visual Studio Code"

# Restart VS Code with reduced extensions
code --disable-extensions
```

### 4. Project-Specific Optimizations

#### Exclude Build Artifacts
```json
{
    "files.exclude": {
        "**/build/**": true,
        "**/external/**": true,
        "**/.git": true,
        "**/logs/**": true,
        "**/test_logs/**": true
    }
}
```

#### Disable Heavy Features
- Turn off real-time linting for large files
- Disable auto-save for performance-critical sessions
- Reduce IntelliSense suggestions

### 5. Dashboard Optimization (For Future Use)

#### Reduced Update Frequency
- Change auto-refresh from 3 seconds to 10 seconds
- Implement lazy loading for heavy components
- Use pagination for large data sets

#### Connection Optimization
- Limit concurrent WebSocket connections
- Implement connection pooling
- Add circuit breakers for failed connections

## Quick Recovery Steps

1. **If VS Code is currently hanging:**
   ```bash
   # Force quit VS Code
   pkill -f "Visual Studio Code"
   
   # Clear VS Code workspace state
   rm -rf ~/.vscode/User/workspaceStorage/*
   
   # Restart with minimal extensions
   code --disable-extensions
   ```

2. **Gradual restoration:**
   - Re-enable essential extensions one by one
   - Monitor system performance
   - Adjust settings as needed

3. **For future dashboard testing:**
   - Use the lightweight version
   - Limit to single exchange connection
   - Reduce update frequency

## Monitoring Commands

```bash
# Check current system load
uptime

# Monitor memory usage
top -o mem | head -20

# Check VS Code processes
ps aux | grep -i "visual studio code"

# Monitor file handles (if too many are open)
lsof | wc -l
```

## Prevention

1. **Resource Monitoring**: Always check system load before running intensive applications
2. **Incremental Testing**: Start with single exchange connections before adding multiple
3. **Regular Cleanup**: Periodically clean build artifacts and logs
4. **Extension Management**: Keep only necessary extensions enabled
5. **Workspace Separation**: Use separate VS Code windows for different project types

## Contact
If issues persist, consider:
- Restarting the system to clear all caches
- Checking for macOS updates
- Monitoring disk space and cleaning up if needed
- Using Activity Monitor to identify resource-heavy processes
