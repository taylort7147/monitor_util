# monitor_util

## Usage:

```
monitor_util [--monitor/-m INDEX] [--info/-i] [--capabilities/-c] [(--get/-g ADDRESS) | (--set/-s ADDRESS VALUE ) | (--toggle)] [--verify/-v]
```

### Example: Get monitor information

```
monitor_util --info -m 0
Monitor Info
------------
Name: \\.\DISPLAY1
Primary: true

monitor_util --info -m 1
Monitor Info
------------
Name: \\.\DISPLAY2
Primary: false
```

### Example: Toggle main monitor

```
monitor_util --toggle
```

### Example: Toggle specific monitor

This is my main use case. I bind these to keyboard shortcuts via AutoHotKey to toggle individual monitors.

```
monitor_util --toggle -m 0
```

### Example: Select a specific input

This example selects a specific input (as opposed to toggling). I use this to bind to a "activate this source's monitors" script, so based on which host is running the script it select's that host as the input on all the monitors. The `0x60` address is 

```
monitor_util.exe -m 0 --set 0x60 0x11

monitor_util.exe -m 1 --set 0x60 0x11
```
