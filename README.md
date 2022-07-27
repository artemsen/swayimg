# Swayimg: image viewer for Sway/Wayland

Now you can view images directly in the current terminal window!
![Screenshot](https://raw.githubusercontent.com/artemsen/swayimg/master/.github/screenshot.png)

## How it works

The program uses [Sway](https://swaywm.org) IPC to determine the geometry of the
currently focused container. This data is used to calculate the position and
size of the new "overlay" window that will be used to draw the image.
In the next step, _swayimg_ adds two Sway rules for the self window: "floating
enable" and "move position". Then it creates a new Wayland window and draws the
image from the specified file.

## Supported image formats

- JPEG (via [libjpeg](http://libjpeg.sourceforge.net));
- JPEG XL (via [libjxl](https://github.com/libjxl/libjxl));
- PNG (via [libpng](http://www.libpng.org));
- GIF (via [giflib](http://giflib.sourceforge.net));
- SVG (via [librsvg](https://gitlab.gnome.org/GNOME/librsvg));
- WebP (via [libwebp](https://chromium.googlesource.com/webm/libwebp));
- AV1 (via [libavif](https://github.com/AOMediaCodec/libavif));
- BMP (built-in).

## Known issues
- Animation and multiframe images are not supported, only the first frame will be displayed.

## Usage

`swayimg [OPTIONS]... [FILE]...`

See `man swayimg` for details.

Examples:
- View multiple files: `swayimg photo.jpg logo.png`
- View all files (recursively) in the current directory in random order: `swayimg -r -o random`
- View using pipes: `wget -O- https://www.kernel.org/theme/images/logos/tux.png 2> /dev/null | swayimg -`

### Key bindings

- `Arrows` and vim-like moving keys (`hjkl`): Move view point;
- `+` or `=`: Zoom in;
- `-`: Zoom out;
- `0`: Set scale to 100%;
- `Backspace`: Reset scale to default;
- `F5` or `[`: Rotate 90 degrees anticlockwise;
- `F6` or `]`: Rotate 90 degrees clockwise;
- `F7`: Flip vertical;
- `F8`: Flip horizontal;
- `i`: Show/hide image properties;
- `F11` or `f`: Toggle full screen mode;
- `PgDown`, `Space`, or `n`: Open next file;
- `PgUp` or `p`: Open previous file;
- `N`: Open file from next directory;
- `P`: Open file from previous directory;
- `Home`: Open the first file;
- `End`: Open the last file;
- `Esc`, `Enter`, `F10` or `q`: Exit the program.

## Configuration

The viewer searches for the configuration file with name `config` in the
following directories:
- `$XDG_CONFIG_HOME/swayimg`
- `$HOME/.config/swayimg`
- `$XDG_CONFIG_DIRS/swayimg`
- `/etc/xdg/swayimg`

Sample file is available [here](https://github.com/artemsen/swayimg/blob/master/extra/swayimgrc).

See `man swayimgrc` for details.

## Install

<a href="https://repology.org/project/swayimg/versions">
  <img src="https://repology.org/badge/vertical-allrepos/swayimg.svg" alt="Packaging status" align="right">
</a>

Arch users can install the program from AUR: [swayimg](https://aur.archlinux.org/packages/swayimg) or [swayimg-git](https://aur.archlinux.org/packages/swayimg-git) package.

Alpine users can install the program from [swayimg](https://pkgs.alpinelinux.org/packages?name=swayimg) or [swayimg-full](https://pkgs.alpinelinux.org/packages?name=swayimg-full) package.

Other users can [build](#build) from sources.

## Build

![CI](https://github.com/artemsen/swayimg/workflows/CI/badge.svg)

The project uses Meson build system:
```
meson build
ninja -C build
sudo ninja -C build install
```
