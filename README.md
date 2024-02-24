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
- AV1F/AVIFS (via [libavif](https://github.com/AOMediaCodec/libavif));
- TIFF (via [libtiff](https://libtiff.gitlab.io/libtiff));
- EXR (via [OpenEXR](https://openexr.com));
- BMP (built-in);
- PNM (built-in);
- TGA (built-in).

## Usage

`swayimg [OPTIONS]... [FILE]...`

See `man swayimg` for details.

Examples:
- View multiple files:
  ```
  swayimg photo.jpg logo.png
  ```
- Start slideshow for all files (recursively) in the current directory in random order:
  ```
  swayimg --slideshow --recursive --order=random
  ```
- View using pipes:
  ```
  wget -qO- https://www.kernel.org/theme/images/logos/tux.png | swayimg -
  ```

## Configuration

The viewer searches for the configuration file with name `config` in the
following directories:
- `$XDG_CONFIG_HOME/swayimg`
- `$HOME/.config/swayimg`
- `$XDG_CONFIG_DIRS/swayimg`
- `/etc/xdg/swayimg`

Sample file is available [here](https://github.com/artemsen/swayimg/blob/master/extra/swayimgrc) or locally `/usr/share/swayimg/swayimgrc`.

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
