# Swayimg: image viewer for Sway/Wayland

![CI](https://github.com/artemsen/swayimg/workflows/CI/badge.svg)

The _swayimg_ utility can be used for preview images inside the currently
focused window (container).

## How it works

The program uses [Sway](https://swaywm.org) IPC to determine the geometry of the
currently focused workspace and window.
These data is used calculate the position and size for a new window.
Then the program adds two rules to the Sway: "floating enable" and
"move position" for _swayimg_ application, creates a new Wayland window and
draws an image from the specified file.

## Supported image formats

- PNG (via cairo);
- JPEG (via libjpeg);
- GIF (via giflib, without animation);
- BMP (without RLE compression).

## Usage

`swayimg [OPTIONS...] FILE`

See `swayimg --help` or `man swayimg` for details.

### Key bindings

- `Arrows` and vim-like moving keys (`hjkl`): Move view point;
- `+`, `=`: Zoom in;
- `-`: Zoom out;
- `Backspace`: Set optimal scale: 100% or fit to window;
- `i`: Show/hide image properties;
- `Esc`, `Enter`, `F10`, `q`, `e`, `x`: Exit the program.

## Build and install

```
meson build
ninja -C build
sudo ninja -C build install
```

Arch users can install the program via [AUR](https://aur.archlinux.org/packages/swayimg).
