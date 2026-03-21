# Swayimg usage

## NAME

`swayimg` - image viewer for Wayland/DRM with Lua scripting support

## SYNOPSIS

*swayimg* [_OPTION_]... [_FILE_]...

If no input files or directories are specified, the viewer will try to read all
files in the current directory.

Use `-` as _FILE_ to read image data from stdin.

Use prefix `exec://` to get image data from stdout printed by external command.

## DESCRIPTION

Swayimg is a lightweight Wayland-native image viewer optimized for the Sway
and Hyprland window managers, featuring an overlay mode that simulates terminal
image display. The application supports slideshow and gallery modes, reads from
stdin and external commands, and is highly customizable via Lua scripting.

## OPTIONS

*-h, --help*
    Display help message and exit.

*-V, --version*
    Display version information and list of supported image formats.

*-v, --viewer*
    Force start in viewer mode.

*-g, --gallery*
    Force start in gallery mode.

*-s, --slideshow*
    Force start in slideshow mode.

*-f, --from-file*=_FILE_
    Load file list from text _FILE_.

*-P, --position*=_X,Y_
    Set initial position of the window (Sway/Hyperland only).

*-S, --size*=_W,H_
    Set initial size of the window.

*-F, --fullscreen*
    Start in full screen mode.

*-c, --config*=_FILE_
    Load the specified Lua _FILE_ instead of default one.

*-e, --execute*=_LUA_
    Execute the specified Lua script after loading the configuration.

*--class*=_NAME_
    Set window class name (application ID).

*--verbose*
    Enable verbose output.

## KEYS

### Viewer and Slideshow

- `Esc`: Exit from application
- `Enter`: Switch to gallery mode
- `s`: Enable/disable to slideshow mode
- `Insert`: Mark/unmark currently displayed image
- `f`: Enable/disable full screen mode
- `a`: Enable/disable anti-aliasing
- `]`: Rotate the image 90 degrees clockwise
- `[`: Rotate the image 90 degrees counterclockwise
- `m`: Flip image vertically
- `Shift-m`: Flip image horizontally
- `t`: Show/hide text info layer
- `PgDown`: Show next image
- `PgUp`: Show previous image
- `Shift-PgDown`: Show next frame
- `Shift-PgUp`: Show previous image
- `+`: Zoom in
- `-`: Zoom out
- `Left/Right/Up/Down`: Move around the image
- `Backspace`: Reset scale and image position to defaults

Mouse bindings:
- `ScrollUp/ScrollDown/ScrollLeft/ScrollRight`: Move image
- `Ctrl-ScrollUp`: Zoom in
- `Ctrl-ScrollDown`: Zoom out
- `MouseLeft`: Move image (drag)
- `MouseRight`: Drag-and-drop image to external applications

### Gallery

- `Esc`: Exit from application
- `Home`: Select first thumbnail in image list
- `End`: Select last thumbnail in image list
- `Left`: Select the thumbnail to the left of the current one
- `Right`: Select the thumbnail to the right of the current one
- `Up`: Select the thumbnail above the current one
- `Down`: Select the thumbnail below the current one
- `PgUp`: Select the thumbnail on the previous page
- `PgDown`: Select the thumbnail on the next page
- `Enter`: Switch to viewer mode
- `s`: Switch to slideshow mode
- `Insert`: Mark/unmark currently selected image
- `f`: Enable/disable full screen mode
- `a`: Enable/disable anti-aliasing
- `+`: Increase thumbnail size
- `-`: Decrease thumbnail size
- `t`: Show/hide text info layer

Mouse bindings:
- `MouseLeft`: Open currently selected image in viewer
- `MouseRight`: Drag-and-drop image to external applications
- `Ctrl-ScrollUp`: Increase thumbnail size
- `Ctrl-ScrollDown`: Decrease thumbnail size
- `ScrollUp/ScrollDown/ScrollLeft/ScrollRight`: Select next image

## ENVIRONMENT

*SWAYSOCK*
    Path to the socket file used for Sway IPC.

*HYPRLAND_INSTANCE_SIGNATURE*, *XDG_RUNTIME_DIR*
    Path to the socket file used for Hyprland IPC.

*XDG_CONFIG_HOME*, *XDG_CONFIG_DIRS*, *HOME*
    Prefix for the path to the default application config file.

*SHELL*
    Shell used for executing an external command and loading an image from
    stdout.

## SIGNALS

*SIGUSR1*, *SIGUSR2*
    Perform the actions specified in the config file.

## EXIT STATUS

The exit status is 0 if the program completed successfully and 1 if an
error occurred. Other codes can be specified via Lua script.

## EXAMPLES

Viewing multiple files:
```
swayimg photo.jpg logo.png
```

Displaying an image received via a pipe:
```
wget -qO- https://www.kernel.org/theme/images/logos/tux.png | swayimg -
```

Loading stdout from external commands:
```
swayimg "exec://wget -qO- https://www.kernel.org/theme/images/logos/tux.png" \
        "exec://curl -so- https://www.kernel.org/theme/images/logos/tux.png"
```

## FILES

The application searches for and processes the `init.lua` file at startup. The
file is a Lua script that sets initial parameters, adds hooks and changes the
behavior of the program.

Possible file locations:
- `$XDG_CONFIG_HOME/swayimg/init.lua`
- `$HOME/.config/swayimg/init.lua`
- `$XDG_CONFIG_DIRS/swayimg/init.lua`
- `/etc/xdg/swayimg/init.lua`

## BUGS

For suggestions, comments, bug reports, etc. visit the project homepage
https://github.com/artemsen/swayimg.
