# Swayimg: image viewer for Wayland

Fully customizable and lightweight image viewer for Wayland based display servers.

- Support for the most popular image formats:
  - JPEG (via [libjpeg](http://libjpeg.sourceforge.net)),
  - JPEG XL (via [libjxl](https://github.com/libjxl/libjxl));
  - PNG (via [libpng](http://www.libpng.org));
  - GIF (via [giflib](http://giflib.sourceforge.net));
  - SVG (via [librsvg](https://gitlab.gnome.org/GNOME/librsvg));
  - WebP (via [libwebp](https://chromium.googlesource.com/webm/libwebp));
  - HEIF/AVIF (via [libheif](https://github.com/strukturag/libheif));
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
- Fully customizable keyboard bindings, colors, and [many other](https://github.com/artemsen/swayimg/blob/master/extra/swayimgrc) parameters;
- Loading images from files and pipes;
- Gallery and viewer modes with slideshow and animation support;
- Preload images in a separate thread;
- Cache in memory, no data is written to permanent storage (HDD/SSD);
- [Sway](https://swaywm.org) integration mode: the application creates an "overlay"
above the currently active window, which gives the illusion that you are opening
the image directly in a terminal window.

![Viewer mode](https://raw.githubusercontent.com/artemsen/swayimg/master/.github/viewer.png)
![Gallery mode](https://raw.githubusercontent.com/artemsen/swayimg/master/.github/gallery.png)

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
- Loading stdout from external commands:
  ```
  swayimg "exec://wget -qO- https://www.kernel.org/theme/images/logos/tux.png" \
          "exec://curl -so- https://www.kernel.org/theme/images/logos/tux.png"
  ```
- View all images from the current directory in gallery mode:
  ```
  swayimg --gallery
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

The project uses Meson build system:
```
meson setup _build_dir
meson compile -C _build_dir
meson install -C _build_dir
```

## Tips

### Copy text from images using OCR

Copying text from images can be done with [Tesseract OCR](https://github.com/tesseract-ocr/tesseract). Here's a practical configuration:

```config
Shift+y = exec tesseract "%" - | wl-copy
```

If you'd like to select a specific area of the screen to copy, you can combine [Grim](https://github.com/emersion/slurp) and [Slurp](https://github.com/emersion/slurp). This combination lets you interactively select an area of the image:

```config
Shift+y = exec grim -g "$(slurp)" - | tesseract - - | wl-copy;
```

### Mail image with Neomutt

To send emails directly from Swayimg, you can configure keybindings to launch Neomutt in a terminal. Since Swayimg waits for child processes to complete _(introduced in version 3.9)_, it's advisable to use the window manager's process management capabilities to prevent potential issues with zombie processes.

#### Sway

```config
Ctrl+m = exec swaymsg exec -- $TERMINAL -e neomutt -a "%";
```

#### Hyprland

```config
Ctrl+m = exec hyprctl dispatch exec "$TERMINAL -e neomutt -a '%'";
```
