# Simple X11 Window Manager

A minimal window manager written using Xlib, inspired by TWM (Tab Window Manager).

## Features

- Basic window management
- Root menu (right-click or F1 key)
- Configurable application launcher
- Simple window decorations

## Requirements

- X11 development libraries
- GCC compiler
- Make

## Building

```bash
make
```

## Configuration

Edit `wm.conf` to customize the root menu. Each line should be in the format:
```
Menu Item Name:command to run
```

Example:
```
Terminal:xterm
File Manager:thunar
Web Browser:firefox
```

## Usage

1. Compile the window manager
2. Add the following to your `~/.xinitrc`:
   ```bash
   exec ./xlib-wm
   ```
3. Start X with `startx`

## Controls

- Right-click or F1: Show root menu
- Left-click menu items to launch applications
- Windows can be moved by dragging
- Windows can be resized using window edges

## License

This project is open source and available under the MIT License. 