# Swayimg: image viewer for Sway/Wayland

![CI](https://github.com/artemsen/swayimg/workflows/CI/badge.svg)

Now you can view images directly in the current terminal window!

## How it works

The program uses [Sway](https://swaywm.org) IPC to determine the geometry of the
currently focused container. This data is used to calculate the position and
size of the new "overlay" window that will be used to draw the image.
In the next step, _swayimg_ adds two Sway rules for the self window: "floating
enable" and "move position". Then it creates a new Wayland window and draws the
image from the specified file.

## Supported image formats

- PNG (via cairo);
- JPEG (via libjpeg);
- GIF (via giflib, without animation);
- BMP (limited support);
- SVG (via librsvg);
- WebP (via libwebp);
- AV1 (via libavif, first frame only).

## Usage

`swayimg [OPTIONS...] FILE...`

See `swayimg --help` or `man swayimg` for details.

### Key bindings

- `Arrows` and vim-like moving keys (`hjkl`): Move view point;
- `+`, `=`: Zoom in;
- `-`: Zoom out;
- `Backspace`: Set optimal scale: 100% or fit to window;
- `[`, `]`: Rotate;
- `i`: Show/hide image properties;
- `f`, `F11`: Toggle full screen mode;
- `PgDown`, `Space`, `n`: Open next file;
- `PgUp`, `p`: Open previous file;
- `Esc`, `Enter`, `F10`, `q`: Exit the program.

## Build and install

```
meson build
ninja -C build
sudo ninja -C build install
```

Arch users can install the program via [AUR](https://aur.archlinux.org/packages/swayimg).

AppImage is available in the [release assets](https://github.com/artemsen/swayimg/releases).
