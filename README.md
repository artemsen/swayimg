# Swayimg: image viewer for Wayland

Image viewer for Wayland/DRM with Lua support.

- Support for the most popular image formats:
  - JPEG (via [libjpeg](http://libjpeg.sourceforge.net)),
  - JPEG XL (via [libjxl](https://github.com/libjxl/libjxl));
  - PNG (via [libpng](http://www.libpng.org));
  - GIF (via [giflib](http://giflib.sourceforge.net));
  - SVG (via [librsvg](https://gitlab.gnome.org/GNOME/librsvg));
  - WebP (via [libwebp](https://chromium.googlesource.com/webm/libwebp));
  - HEIF/HEIC (via [libheif](https://github.com/strukturag/libheif));
  - AV1F/AVIFS (via [libavif](https://github.com/AOMediaCodec/libavif));
  - TIFF (via [libtiff](https://libtiff.gitlab.io/libtiff));
  - Sixel (via [libsixel](https://github.com/saitoha/libsixel));
  - Raw: CRW/CR2, NEF, RAF, etc (via [libraw](https://www.libraw.org));
  - EXR (via [OpenEXR](https://openexr.com));
  - BMP (built-in);
  - PNM (built-in);
  - TGA (built-in);
  - QOI (built-in);
  - DICOM (built-in);
  - Farbfeld (built-in).
- Fully customizable keyboard bindings, colors, and many other parameters via
  [Lua bindings](CONFIG.md);
- Loading images from files and pipes;
- Gallery and viewer modes with slideshow and animation support;
- Preload images in a separate thread;
- Cache in memory, no data is written to permanent storage (HDD/SSD);
- [Sway](https://swaywm.org) and [Hyprland](https://hypr.land/) integration
  mode: the application creates an "overlay" above the currently active window,
  which gives the illusion that you are opening the image directly in a terminal
  window (enabled in Sway by default).

![Viewer mode](.github/viewer.png)
![Gallery mode](.github/gallery.png)

## Usage

`swayimg [OPTIONS]... [FILE]...`

Examples:
- View multiple files:
  ```
  swayimg photo.jpg logo.png
  ```
- View using pipes:
  ```
  wget -qO- https://www.kernel.org/theme/images/logos/tux.png | swayimg -
  ```
- Loading stdout from external commands:
  ```
  swayimg "exec://wget -qO- https://www.kernel.org/theme/images/logos/tux.png" \
          "exec://curl -so- https://www.kernel.org/theme/images/logos/tux.png"
  ```

## Configuration

The Swayimg configuration file is a Lua script. Please refer to the official Lua
documentation for information about the file format.

The full list of available functions is described in the [documentation](CONFIG.md)
and in the [Lua source](extra/swayimg.lua) file.

[Example](extra/example.lua) of the configuration file is available after
installation in `/usr/share/swayimg/example.lua`.

## Install

[![Packaging status](https://repology.org/badge/tiny-repos/swayimg.svg)](https://repology.org/project/swayimg/versions)

List of supported distributives can be found on the
[Repology page](https://repology.org/project/swayimg/versions).

Arch users can install the program from the [extra](https://archlinux.org/packages/extra/x86_64/swayimg)
repository or from [AUR](https://aur.archlinux.org/packages/swayimg-git) package.

## Build

The project uses Meson build system:
```
meson setup my_build_dir
meson compile -C my_build_dir
meson install -C my_build_dir
```
