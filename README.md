# Sigma360
Hackathon 2026 Project!

Echo360 but as a terminal client/user interface

## Requirements
- cJSON.h
- notcurses
- ffmpeg
- python-requests
- python-playwright

```
$ sudo pacman -S cjson notcurses ffmpeg python-requests python-playwright
$ playwright install chromium
```

## Installation
```
git clone https://github.com/MiiKaa3/Sigma360.git
cd ./Sigma360
cmake -S . -B ./build
cmake --build build
```

## Usage
```
$ ./build/sigma360 --help
Usage: sigma360 (arguments)
Arguments:
  --version   Show version information
  --help      Show this help message
```
