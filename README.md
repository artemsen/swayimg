# Swayimg: image viewer for Sway/Wayland

Swayimg is a lightweight image viewer for Wayland display servers.

In a [Sway](https://swaywm.org) compatible mode, the viewer creates an "overlay"
above the currently active window, which gives the illusion that you are opening
the image directly in a terminal window.

![Screenshot](https://raw.githubusercontent.com/artemsen/swayimg/master/.github/screenshot.png)

## Supported image formats

- JPEG (via [libjpeg](http://libjpeg.sourceforge.net));
- JPEG XL (via [libjxl](https://github.com/libjxl/libjxl));
- PNG (via [libpng](http://www.libpng.org));
- GIF (via [giflib](http://giflib.sourceforge.net));
- SVG (via [librsvg](https://gitlab.gnome.org/GNOME/librsvg));
- WebP (via [libwebp](https://chromium.googlesource.com/webm/libwebp));
- HEIF/AVIF (via [libheif](https://github.com/strukturag/libheif));
- TIFF (via [libtiff](https://libtiff.gitlab.io/libtiff));
- EXR (via [OpenEXR](https://openexr.com));
- BMP (built-in);
- PNM (built-in).

## Usage

`swayimg [OPTIONS]... [FILE]...`

See `man swayimg` for details.

Examples:
- View multiple files:
  ```
  swayimg photo.jpg logo.png
  ```
- View all files (recursively) in the current directory in random order:
  ```
  swayimg --recursive --order=random
  ```
- View using pipes:
  ```
  wget -qO- https://www.kernel.org/theme/images/logos/tux.png | swayimg -
  ```

### Key bindings

Default key bindings can be overridden using a configuration file.

| Key | Action |
| --- | ------ |
| `Arrows` and vim-like moving keys (`hjkl`) | Move view point |
| `+` or `=`                   | Zoom in |
| `-`                          | Zoom out |
| `0`                          | Set scale to 100% |
| `Z`                          | Scale image to fit window |
| `z`                          | Scale image to fit window, but no more than 100% |
| `Backspace`                  | Reset position and scale to defaults |
| `F5` or `[`                  | Rotate 90 degrees anticlockwise |
| `F6` or `]`                  | Rotate 90 degrees clockwise |
| `F7`                         | Flip vertical |
| `F8`                         | Flip horizontal |
| `i`                          | Show/hide image properties |
| `F11` or `f`                 | Toggle full screen mode |
| `PgDown`, `Space`, or `n`    | Open next file |
| `PgUp` or `p`                | Open previous file |
| `N`                          | Open file from next directory |
| `P`                          | Open file from previous directory |
| `Home` or `g`                | Open the first file |
| `End` or `G`                 | Open the last file |
| `F2` or `O`                  | Show previous frame |
| `F3` or `o`                  | Show next frame |
| `F4` or `s`                  | Start/stop animation |
| `F9`                         | Start/stop slideshow mode |
| `a`                          | Enable/disable anti-aliasing |
| `r`                          | Reload current image |
| `e`                          | Execute system command for the current file |
| `Esc`, `Enter`, `F10` or `q` | Exit the program |

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

[![Packaging status](https://repology.org/badge/tiny-repos/swayimg.svg)](https://repology.org/project/swayimg/versions)

List of supported distributives can be found on the [Repology page](https://repology.org/project/swayimg/versions).

Arch users can install the program from the extra repository: [swayimg](https://archlinux.org/packages/extra/x86_64/swayimg) or from AUR [swayimg-git](https://aur.archlinux.org/packages/swayimg-git) package.

## Build

![CI](https://github.com/artemsen/swayimg/workflows/CI/badge.svg)

The project uses Meson build system:
```
meson build
ninja -C build
sudo ninja -C build install
```
