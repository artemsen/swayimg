SWAYIMG(1) "swayimg" "Swayimg manual"

# NAME

swayimg - image viewer for Wayland display servers

# SYNOPSIS

*swayimg* [OPTIONS]... [FILE]...

# DESCRIPTION

If no input files or directories are specified, the viewer will try to read all
files in the current directory.

Use '-' as FILE to read image data from stdin.

Use prefix *exec://* to get image data from stdout printed by external command.

# OPTIONS

*-h, --help*
	Display help message and exit.

*-v, --version*
	Display version information and list of supported image formats.

*-g, --gallery*
	Start in gallery mode.

*-F, --from-file*
	Interpret _FILE_ as text list of image files.

*-r, --recursive*
	Read directories recursively.

*-o, --order*=_ORDER_
	Set order of the image list:
	- _none_: unsorted;
	- _alpha_: sorted alphabetically;
	- _numeric_: sorted numerically;
	- _mtime_: sorted by file modification time;
	- _size_: sorted by file size;
	- _random_: randomize list.

*-s, --scale*=_MODE_
	Set image scale:
	- _optimal_: 100% or less to fit to window (default);
	- _width_: fit image width to window width;
	- _height_: fit image height to window height;
	- _fit_: fit to window;
	- _fill_: crop image to fill the window;
	- _real_: real size (100%);
	- _keep_: keep the same scale as for previously viewed image.

*-l, --slideshow*
	Run slideshow on startup.

*-f, --fullscreen*
	Start in full screen mode.

*-p, --position*=_parent_|X,Y
	Set window position, this option valid only in Sway WM:
	- _parent_: set position from parent (currently active) window;
	- X,Y: absolute coordinates of the top left corner, e.g. _100,200_.

*-w, --size*=_fullscreen_|_parent_|_image_|W,H
	Set window size:
	- _fullscreen_: use full screen mode;
	- _parent_: [Only in Sway WM] set size from parent (currently active)
	  window;
	- _image_: set size from the first loaded image;
	- W,H: absolute size of the window in pixels, e.g. _320,240_.

*-a, --class*=_NAME_
	Set window class/app_id.

*-i, --ipc*=_FILE_
	Start IPC server based on UNIX sockets. _FILE_ must point to the UNIX socket
	file to create.
	The IPC protocol is a simple text-based one-way protocol that allows
	clients to submit actions for execution. For a list of actions and their
	parameters, see _swayimgrc_(5).

*-c, --config*=_SECTION.KEY=VALUE_
	Override configuration parameter, see _swayimgrc_(5) for a list of sections
	and their parameters.

*-C, --config-file*=_FILE_
	Load configuration from specified _FILE_.

# SWAY/HYPRLAND INTEGRATION

Sway/Hyprland integration mode is automatically enabled if IPC for these
compositors is present in the system.
This mode provides some features such as setting the window position and
getting the workspace layout.
By default, the application creates an "overlay" above the currently active
window, which gives the illusion that the image is opened directly inside
the terminal window.

# ENVIRONMENT

_SWAYSOCK_
	Path to the socket file used for Sway IPC.

_HYPRLAND_INSTANCE_SIGNATURE_, _XDG_RUNTIME_DIR_
	Path to the socket file used for Hyprland IPC.

_XDG_CONFIG_HOME_, _XDG_CONFIG_DIRS_, _HOME_
	Prefix of the path to the application config file, see _swayimgrc_(5) for
	details.

_XDG_CACHE_HOME_, _HOME_
	Prefix for the path used as persistent storage for thumbnails.

_SHELL_
	Shell for executing an external command and loading an image from stdout.

# SIGNALS

_SIGUSR1_, _SIGUSR2_
	Perform the actions specified in the config file.

# EXIT STATUS

The exit status is 0 if the program completed successfully and 1 if an
error occurred.

# EXAMPLES

View multiple files:
```
swayimg photo.jpg logo.png
```

Start slideshow for all files (recursively) in the current directory in random
order:
```
swayimg --slideshow --recursive --order=random
```

View using pipes:
```
wget -qO- https://www.kernel.org/theme/images/logos/tux.png | swayimg -
```

Loading stdout from external commands:
```
swayimg "exec://wget -qO- https://www.kernel.org/theme/images/logos/tux.png" \\
        "exec://curl -so- https://www.kernel.org/theme/images/logos/tux.png"
```

View all images from the current directory in gallery mode:
```
swayimg --gallery
```

# SEE ALSO
_swayimgrc_(5)

# BUGS

For suggestions, comments, bug reports, etc. visit the project homepage
https://github.com/artemsen/swayimg.
