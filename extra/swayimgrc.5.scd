SWAYIMGRC(5) "swayimg" "Swayimg configuration"

# NAME

swayimgrc - configuration file for the Swayimg viewer

# SYNOPSIS

The Swayimg configuration file is a text-based INI file used to override the
default settings.

# LOCATION

Swayimg searches for a config file in the following locations, in this order:
- $XDG_CONFIG_HOME/swayimg/config
- $HOME/.config/swayimg/config
- $XDG_CONFIG_DIRS/swayimg/config
- /etc/xdg/swayimg/config

# DESCRIPTION

The structure of the INI file consists of key-value pairs for properties and
sections that organize properties.

The number sign (#) at the beginning of the line indicates a comment.
Empty lines and comments are ignored.

The basic element contained in the INI file is the key or property.
Every key has a name and a value, delimited by an equals sign (=).
The name appears to the left of the equals sign. The value can contain any
characters.

Keys are grouped into named sections. The section name appears on a line by
itself, in square brackets. All keys after the section declaration are
associated with that section.

Any option can be overridden using the *--config* argument in the command line,
for instance: *swayimg  --config="general.mode=gallery"*.

# SECTIONS

## General

General configuration for the entire application: _[general]_.

*mode* = _viewer_|_gallery_
	Mode used at startup, _viewer_ by default.

*position* = _parent_|X,Y
	Initial window position, this option valid only in Sway WM:
	- _parent_: set position from parent (currently active) window (default);
	- X,Y: absolute coordinates of the top left corner, e.g. _100,200_.

*size* = _fullscreen_|_parent_|_image_|W,H
	Initial window size:
	- _fullscreen_: use full screen mode;
	- _parent_: [Only in Sway WM] set size from parent (currently active) window
	  (default);
	- _image_: set size from the first loaded image;
	- W,H: absolute size of the window in pixels, e.g. _320,240_.

*decoration* = _yes_|_no_
	Use window decoration (borders and title), _no_ by default.

*sigusr1* = ACTION
	Action to be performed when the SIGUSR1 signal is triggered, _reload_ by
	default.

*sigusr2* = ACTION
	Action to be performed when the SIGUSR2 signal is triggered, _next_file_ by
	default.

*app_id* = NAME
	Application ID used as window class name, _swayimg_ by default.

## Viewer

Configuration specific to the viewer mode: _[viewer]_.

*window* = #COLOR
	Window background color in RGB or RGBA format, _#00000000_ by default.

*transparency* = _grid_|#COLOR
	Background for transparent images:
	- _grid_: draw chessboard (default);
	- #COLOR: solid color in RGB or RGBA, e.g _#10ff4280_.

*scale* = MODE
	Initial image scale:
	- _optimal_: 100% or less to fit to window (default);
	- _width_: fit image width to window width;
	- _height_: fit image height to window height;
	- _fit_: fit to window;
	- _fill_: crop image to fill the window;
	- _real_: real size (100%);
	- _keep_: keep the same scale as for previously viewed image.

*position* = POSITION
	Initial image position on the window:
	- _top_: move image to top and center by width;
	- _center_: center by width and height (default);
	- _bottom_: move to bottom and center by width;
	- _left_: move to left and center by height;
	- _right_: move to right and center by height;
	- _topleft_: move to top and left;
	- _topright_: move to top and right;
	- _bottomleft_: move to bottom and left;
	- _bottomright_: move to bottom and right;
	- _free_: like center, but freely movable (not fixed).

*antialiasing* = METHOD
	Anti-aliasing method when scaling images.
	In general, the methods improve in quality and decrease in performance from
	top to bottom:
	- _none_: nearest-neighbor, or no anti-aliasing;
	- _box_: nearest-neighbor on upscale, average in a box on downscale;
	- _bilinear_: bilinear;
	- _bicubic_: bicubic with the Catmull-Rom spline;
	- _mks13_: Magic Kernel with the 2013 Sharp approximation (default).

*slideshow* = _yes_|_no_
	Run slideshow at startup, _no_ by default.

*slideshow_time* = SECONDS
	Slideshow image duration in seconds, _3_ by default.

*history* = SIZE
	Number of previously viewed images to store in cache, _1_ by default.

*preload* = SIZE
	Number of images to preload in a separate thread, _1_ by default.

## Gallery

Configuration specific to the gallery mode: _[gallery]_.

*size* = PIXELS
	Height and width of each thumbnail in pixels, _200_ by default.

*cache* = SIZE
	Max number of invisible thumbnails stored in memory cache, _100_ by default.

*preload* = _yes_|_no_
	Load not only visible but also adjacent thumbnails, _no_ by default.
	The *cache* parameter limits the number of preloaded thumbnails.

*pstore* = _yes_|_no_
	Enable/disable storing thumbnails in persistent storage, _no_ by default.
	If enabled, thumbnails will be stored in _XDG_CACHE_HOME/swayimg_.

*fill* = _yes_|_no_
	Fill the entire tile with thumbnail, yes by default.

*antialiasing* = METHOD
	Anti-aliasing method when scaling images.
	In general, the methods improve in quality and decrease in performance from
	top to bottom:
	- _none_: nearest-neighbor, or no anti-aliasing;
	- _box_: nearest-neighbor on upscale, average in a box on downscale;
	- _bilinear_: bilinear;
	- _bicubic_: bicubic with the Catmull-Rom spline;
	- _mks13_: Magic Kernel with the 2013 Sharp approximation (default).

*window* = #COLOR
	Window background color in RGB or RGBA format, _#00000000_ by default.

*background* = #COLOR
	Background color of non-selected tiles, _#202020ff_ by default.

*select* = #COLOR
	Background color of the selected tile, _#404040ff_ by default.

*border* = #COLOR
	Border color of the selected tile, _#000000ff_ by default.

*shadow* = #COLOR
	Shadow color of the selected tile, _#000000ff_ by default.

## Image list

Configuration of the image list: _[list]_.

*order* = ORDER
	Order of the image list:
	- _none_: unsorted;
	- _alpha_: sorted alphabetically (default);
	- _numeric_: sorted numerically;
	- _mtime_: sorted by file modification time;
	- _size_: sorted by file size;
	- _random_: randomize list.

*reverse* = _yes_|_no_
	Reverse sort order, _no_ by default.

*loop* = _yes_|_no_
	Looping file list mode, _yes_ by default.

*recursive* = _yes_|_no_
	Read directories recursively, _no_ by default.

*all* = _yes_|_no_
	Add all files from the same directory as the first file. This option is
	ignored if more than one input file is specified. Default value is _no_.

*fsmon* = _yes_|_no_
	Enable file system monitoring to automatically update the image list, _yes_
	by default.

## Font

Font configuration: _[font]_.

*name* = NAME
	Font name, _monospace_ by default.

*size* = SIZE
	Font size (in pt), _14_ by default.

*color* = #COLOR
	Text color, _#ccccccff_ by default.

*shadow* = #COLOR
	Text shadow color, _#000000d0_ by default.
	To disable shadow use fully transparent color _#00000000_.

*background* = #COLOR
	Text background color, _#00000000_ (fully transparent) by default.

## Text info: general configuration

General configuration of the displayed text layer: _[info]_.

*show* = _yes_|_no_
	Enable or disable info text at startup, _yes_ by default.

*info_timeout* = SECONDS
	Timeout of image information displayed on the screen, _0_ to always show,
	_5_ by default.

*status_timeout* = SECONDS
	Timeout of the status message displayed on the screen, _3_ by default.

## Text info: viewer mode

Layout of fields for displaying text info in viewer mode: _[info.viewer]_.
The layout describes what metadata should be displayed in each corner of the
window.
The list can contain any number of fields separated by commas. A plus sign
preceding a field name adds the field title to the display.

*top_left* = LIST
	Top left corner of the window.
	Default is _+name,+format,+filesize,+imagesize,+exif_.

*top_right* = LIST
	Top right corner of the window.
	Default is _index_.

*bottom_left* = LIST
	Bottom left corner of the window.
	Default is _scale,frame_.

*bottom_right* = LIST
	Bottom right corner of the window.
	Default is _status_.

Avalable fields:
- _name_: File name of the currently viewed/selected image.
- _dir_: Parent directory name of the currently viewed/selected image.
- _path_: Absolute path or special source string of the currently
  viewed/selected image.
- _filesize_: File size in human-readable format.
- _format_: Brief image format description.
- _imagesize_: Size of the image (or its current frame) in pixels.
- _exif_: List of EXIF data.
- _frame_: Current and total number of frames.
- _index_: Current and total index of image in the image list.
- _scale_: Current image scale in percent.
- _status_: Status message.
- _none_: Empty field (ignored).

## Text info: gallery mode

Layout of fields for displaying text info in gallery mode: _[info.gallery]_.
The format is the same as _[info.viewer]_:

*top_left* = LIST
	Top left corner of the window.
	Default is _none_.

*top_right* = LIST
	Top right corner of the window.
	Default is _none_.

*bottom_left* = LIST
	Bottom left corner of the window.
	Default is _none_.

*bottom_right* = LIST
	Bottom right corner of the window.
	Default is _name,status_.

## Key bindings

Key bindings configurations are stored in sections _[keys.viewer]_ and
_[keys.gallery]_.
Each line associates a key with a list of actions and optional parameters.
Actions are separated by semicolons. One or more key modifiers (_Ctrl_, _Alt_,
_Shift_) can be specified in the key name.
The key name can be obtained with the xkbcli tool: *xkbcli interactive-wayland*.

Predefined names for mouse scroll:
- _ScrollUp_: Mouse wheel up;
- _ScrollDown_: Mouse wheel down;
- _ScrollLeft_: Mouse scroll left;
- _ScrollRight_: Mouse scroll right.

# Actions

*none* Dummy, can be used to remove a built-in action.

*help* Show/hide help.

*first_file* Go to the first file.

*last_file* Go to the last file.

*prev_dir* (Viewer mode only) Go to previous directory.

*next_dir* (Viewer mode only) Go to next directory.

*prev_file* Go to previous file.

*next_file* Go to next file.

*rand_file* (Viewer mode only) Go to random file.

*step_left* (Gallery mode only) Select previous image.

*step_right* (Gallery mode only) Select image above.

*step_up* (Gallery mode only) Select image above.

*step_down* (Gallery mode only) Select image below.

*page_up* (Gallery mode only) Scroll page up.

*page_down* (Gallery mode only) Scroll page down.

*skip_file* Skip the current file (remove it from the image list).

*prev_frame* (Viewer mode only) Show previous frame.

*next_frame* (Viewer mode only) Show next frame.

*animation* (Viewer mode only) Start/stop animation.

*slideshow* (Viewer mode only) Start/stop slideshow.

*fullscreen* Toggle full-screen mode.

*mode* [_viewer_|_gallery_]
	Switch between viewer and gallery modes.

*step_left* [PERCENT]
	(Viewer mode only) Move viewport left, default is _10%_.

*step_right* [PERCENT]
	(Viewer mode only) Move viewport right, default is _10%_.

*step_up* [PERCENT]
	(Viewer mode only) Move viewport up, default is _10%_;

*step_down* [PERCENT]
	(Viewer mode only) Move viewport down, default is _10%_;

*zoom* [SCALE]
	(Viewer mode only) Manage scaling, _SCALE_ is:
	- one of the *viewer.scale* modes (_fit_/_fill_/etc);
	- percentage number with + or - sign (e.g. _+10_) to zoom in or out;
	- absolute value in percentage to set the scale, e.g. _200_;
	- empty value to switch to the next available scaling mode.

*rotate_left* (Viewer mode only) Rotate image anticlockwise.

*rotate_right* (Viewer mode only) Rotate image clockwise.

*flip_vertical* (Viewer mode only) Flip image vertically.

*flip_horizontal* (Viewer mode only) Flip image horizontally.

*thumb* SIZE
	(Gallery mode only) Resize thumbnails, _SIZE_ is:
	- delta in pixels with + or - sign (e.g. _+10_);
	- absolute size in pixels, e.g. _200_.

*reload* Reset cache and reload current image.

*antialiasing* [MODE]
	Set anti-aliasing mode or cycle through them, _MODE_ is:
	- one of the *viewer.antialiasing* modes (_none_/_mks13_/etc);
	- _next_ or empty value: switch to next mode;
	- _prev_: switch to previous mode.

*info* [MODE]
	Set text info mode or cycle through them, _MODE_ is on of
	_off_/_viewer_/_gallery_ or empty value to switch to next mode.

*exec* COMMAND
	Execute an external command, use % to substitute the full path to the
	current image, %% to escape %.

*export* FILE
	Export currently displayed image to PNG file.

*status* TEXT
	Set and show status text.

*exit* Exit the application.

# EXAMPLES
Sample file is available on the project repository:
https://github.com/artemsen/swayimg/blob/master/extra/swayimgrc or
locally after installation: /usr/share/swayimg/swayimgrc.

```
# comment
[list]
order = random
[font]
size = 16
[keys]
Ctrl+Alt+e = exec echo "%" > mylist.txt
```

# SEE ALSO
_swayimg_(1)

# BUGS

For suggestions, comments, bug reports, etc. visit the project homepage
https://github.com/artemsen/swayimg.
