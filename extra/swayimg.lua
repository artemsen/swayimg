---@meta swayimg

--------------------------------------------------------------------------------
-- Main application class
--------------------------------------------------------------------------------

---@alias appmode_t
---| "viewer"    # Image viewer mode
---| "slideshow" # Slide show mode
---| "gallery"   # Gallery mode

---@alias pair {[1]:integer,[2]:integer}

---Main application class.
---@class swayimg
---@field mode appmode_t Which mode is the application in
---@field title string Window title text
---Mouse button for drag-and-drop to external apps.
---Configurable only at startup.
---@field drag_button string
---@field antialiasing boolean Enable/disable antialiasing
---Enable or disable window decoration (title, border, buttons).
---Available only in Wayland, the corresponding protocol must be
---supported by the composer.
---By default disabled in Sway and enabled in other compositors.
---@field decoration boolean
---Create a floating window with the same coordinates and size as the currently
---focused window. This variable can be set only once.
---Sway and Hyprland compositors only.
---By default enabled in Sway and disabled in other compositors.
---@field overlay boolean
swayimg = {}

---Exit from application.
---@param code? integer Program exit code, `0` by default
function swayimg.exit(code) end

---Get application window size.
---@return integer width in pixels
---@return integer height in pixels
function swayimg.get_window_size() end

---Set application window size.
---@param width integer Width of the window in pixels
---@param height integer Height of the window in pixels
function swayimg.set_window_size(width, height) end

---Toggle full screen mode.
---@return boolean # True if full screen is enabled
function swayimg.toggle_fullscreen() end

---Get mouse pointer coordinates.
---@return integer x
---@return integer y
function swayimg.get_mouse_pos() end

---Set the initialization completion handler.
---Called after all subsystems have been initialized.
---@param fn function Initialization completion callback
function swayimg.on_initialized(fn) end

---Add a callback function called when a new image is selected.
---@param fn fun():(boolean?) Handler, return true to detach
function swayimg.on_image_change(fn) end

---Add a callback function called when main window is resized.
---@param fn fun():(boolean?) Handler, return true to detach
function swayimg.on_window_resize(fn) end

---Bind the signal event to a handler.
---@param signal string Signal name (`USR1`, `USR2`, etc.)
---@param fn fun():(boolean?) Handler, return true to detach
function swayimg.on_signal(signal, fn) end

--------------------------------------------------------------------------------
-- Image entry
--------------------------------------------------------------------------------

---A single image entry from the image list. READONLY object
---@class image_entry
---@field index integer Index in the image list
---@field path string Absolute path to the image file
---@field size integer File size in bytes
---@field mtime string File modification time
---@field marked boolean Whether the image is marked

--------------------------------------------------------------------------------
-- Image list
--------------------------------------------------------------------------------

---@alias order_t
---| "none"    # Unsorted (system-dependent)
---| "alpha"   # Lexicographic sort: 1,10,2,20,a,b,c
---| "numeric" # Numeric sort: 1,2,3,10,100,a,b,c
---| "mtime"   # Modification time sort
---| "size"    # Size sort
---| "random"  # Random order

---Image list.
---@class imagelist
---@field order order_t Image list sort order
---@field reverse boolean Reverse the sort order
---@field recursive boolean Recursive directory reading
---@field adjacent boolean Open adjacent files from the same directory
swayimg.imagelist = {}

---Get size of image list.
---@return integer # Number of entries in the image list
function swayimg.imagelist.size() end

---Get list of all entries in the image list.
---@return image_entry[] # Array with all entries
function swayimg.imagelist.get() end

---Add entry to the image list.
---@param path string Path to add
function swayimg.imagelist.add(path) end

---Remove entry from the image list.
---@param path string Path to remove
function swayimg.imagelist.remove(path) end

--------------------------------------------------------------------------------
-- Text overlay layer
--------------------------------------------------------------------------------

---Text overlay layer.
---@class text
---Toggle for the entire text layer (regardless of active mode).
---Specify a number (float) to use a timeout in seconds after which the entire text layer is hidden.
---@field visible boolean|number
---@field font string Font face name
---@field size integer Font size in pixels
---@field padding integer Padding from window edges in pixels
---@field foreground integer Foreground text color in ARGB format, e.g. `0xff00aa99`
---@field background integer Background text color in ARGB format, e.g. `0xff00aa99`
---@field shadow integer Shadow text color in ARGB format, e.g. `0xff00aa99`
---@field status_timer number Timeout in seconds after which the status message is hidden
swayimg.text = {}

---Show status message for the duration of `swayimg.text.status_timer` seconds.
---@param status string Status text to show
function swayimg.text.set_status(status) end

--------------------------------------------------------------------------------
-- Base mode class
--------------------------------------------------------------------------------

---Each string in array is a printable template. The template includes text and
---fields surrounded by curly braces. The following fields are supported:
---* `{name}`: File name of the currently viewed/selected image;
---* `{dir}`: Parent directory name of the currently viewed/selected image;
---* `{path}`: Absolute path to the currently viewed/selected image;
---* `{size}`: File size in bytes;
---* `{sizehr}`: File size in human-readable format;
---* `{time}`: File modification time;
---* `{format}`: Brief image format description;
---* `{scale}`: Current image scale in percent;
---* `{list.index}`: Current index of image in the image list;
---* `{list.total}`: Total number of files in the image list;
---* `{frame.width}`: Current frame width in pixels;
---* `{frame.height}`: Current frame height in pixels;
---* `{meta.*}`: Image meta info: EXIF, tags etc. List of available tags can be
---  found at [Exiv2 website](https://exiv2.org/tags.html) or printed using
---  utility exiv2: `exiv2 -pa photo.jpg`.
---@alias text_template_t string

---Base class providing text overlay layout fields shared by all display modes.
---@class mode_base
---@field text_tl text_template_t[] Text layer scheme for top-left corner
---@field text_tr text_template_t[] Text layer scheme for top-right corner
---@field text_bl text_template_t[] Text layer scheme for bottom-left corner
---@field text_br text_template_t[] Text layer scheme for bottom-right corner
local mode_base = {}

---Map an input event to an action.
---@param bind string|string[] 1 or more mouse or keyboard events to map - `Ctrl+ScrollDown`, etc.
---@param cb string|function shellcmd to execute (% for current image) or callback function to run
function mode_base.map(bind, cb) end

---Remove all existing key/mouse/signal bindings.
function mode_base.bind_reset() end

---Mark or unmark the current image.
---@param state? boolean Should the image be marked or not
function mode_base.mark_current_image(state) end

--------------------------------------------------------------------------------
-- Viewer mode
--------------------------------------------------------------------------------

---@alias vdir_t
---| "first" # First file in image list
---| "last" # Last file in image list
---| "next" # Next file
---| "prev" # Previous file
---| "next_dir" # First file in next directory
---| "prev_dir" # Last file in previous directory
---| "random" # Random file in image list

---@alias fixed_scale_t
---| "optimal" # 100% or less to fit to window
---| "width" # Fit image width to window width
---| "height" # Fit image height to window height
---| "fit" # Fit to window
---| "fill" # Crop image to fill the window
---| "real" # Real size (100%)
---| "keep" # Keep the same scale as for previously viewed image

---@alias fixed_position_t
---| "center" # Vertical and horizontal center of the window
---| "topcenter" # Top (vertical) and center (horizontal) of the window
---| "bottomcenter" # Bottom (vertical) and center (horizontal) of the window
---| "leftcenter" # Left (horizontal) and center (vertical) of the window
---| "rightcenter" # Right (horizontal) and center (vertical) of the window
---| "topleft" # Top left corner of the window
---| "topright" # Top right corner of the window
---| "bottomleft" # Bottom left corner of the window
---| "bottomright" # Bottom right corner of the window

---@alias rotation_t
---| 90 # 90 degrees
---| 180 # 180 degrees
---| 270 # 270 degrees

---@alias bkgmode_t
---| "extend" # Fill window with the current image and blur it
---| "mirror" # Fill window with the mirrored current image and blur it
---| "auto" # Fill the window background in `extend` or `mirror` mode depending on the image aspect ratio

---@class viewer.image_entry: image_entry
---@field width integer Current frame width in pixels
---@field height integer Current frame height in pixels
---@field format string Brief image format description
---Image meta information - `meta.Exif.Photo.ExposureTime` etc.
---see <https://exiv2.org/tags.html>
---@field [string] table<string,string>

---@class viewer : mode_base
---@field default_scale fixed_scale_t Default scale applied to newly opened images
---@field default_position fixed_position_t Default position applied to newly opened images
---@field window_background integer|bkgmode_t Window background: solid ARGB color or fill mode
---@field image_background integer Background color for transparent images (ARGB); disables checkerboard grid
---@field mark_color integer Mark icon color in ARGB format
---@field freemove boolean Free move mode TODO: needs a more detailed explanation
---@field loop boolean Image list loop mode
---@field preload_limit integer Number of images to preload in a separate thread
---@field history_limit integer Number of previously viewed images to keep in cache
swayimg.viewer = {}

---Go to the next file in the specified direction.
---@param direction vdir_t Next file direction
function swayimg.viewer.switch_image(direction) end

---Get information about currently viewed image.
---@return viewer.image_entry # Currently viewed image entry
function swayimg.viewer.get_current_image() end

---Get current image scale.
---@return number # Absolute scale value (1.0 = 100%)
function swayimg.viewer.get_scale() end

---Set absolute image scale with optional zoom center.
---@param scale number Absolute value (1.0 = 100%)
---@param x? integer X coordinate of center point, empty for window center
---@param y? integer Y coordinate of center point, empty for window center
function swayimg.viewer.set_abs_scale(scale, x, y) end

---Set fixed image scale.
---@param scale fixed_scale_t Fixed scale name
function swayimg.viewer.set_fix_scale(scale) end

---Reset scale to default value.
---@see swayimg.viewer.default_scale
function swayimg.viewer.reset_scale() end

---Get image position.
---@return integer x
---@return integer y
function swayimg.viewer.get_position() end

---Set absolute image position.
---@param x integer Horizontal image position on the window
---@param y integer Vertical image position on the window
function swayimg.viewer.set_abs_position(x, y) end

---Set fixed image position.
---@param pos fixed_position_t Fixed image position
function swayimg.viewer.set_fix_position(pos) end

---Show next frame from multi-frame image (animation).
---This function also stops the animation.
---@see swayimg.viewer.animation_stop
---@see swayimg.viewer.animation_resume
---@return integer # Index of the currently shown frame
function swayimg.viewer.next_frame() end

---Show previous frame from multi-frame image (animation).
---This function also stops the animation.
---@see swayimg.viewer.animation_stop
---@see swayimg.viewer.animation_resume
---@return integer # Index of the currently shown frame
function swayimg.viewer.prev_frame() end

---Flip image vertically.
function swayimg.viewer.flip_vertical() end

---Flip image horizontally.
function swayimg.viewer.flip_horizontal() end

---Rotate image.
---@param angle rotation_t Rotation angle
function swayimg.viewer.rotate(angle) end

---Stop animation.
function swayimg.viewer.animation_stop() end

---Resume animation.
function swayimg.viewer.animation_resume() end

---Set parameters for the checkerboard grid used as background for transparent images.
---@param size number Size of single grid cell in pixels
---@param color1 number First color in ARGB format, e.g. `0xff00aa99`
---@param color2 number Second color in ARGB format, e.g. `0xff00aa99`
function swayimg.viewer.set_image_grid(size, color1, color2) end

---Export currently viewed frame to PNG file.
---@param path string Path to file
function swayimg.viewer.export(path) end

---Bind the mouse button to image drag operation.
---@param button string Button description, for example `MouseLeft`
function swayimg.viewer.bind_drag(button) end

--------------------------------------------------------------------------------
-- Slide show mode
--------------------------------------------------------------------------------

---@class slideshow : viewer
---@field timeout number Timeout in seconds after which the next image is opened
swayimg.slideshow = {}

--------------------------------------------------------------------------------
-- Gallery mode
--------------------------------------------------------------------------------

---@alias gdir_t
---| "first" # Select first thumbnail in image list
---| "last" # Select last thumbnail in image list
---| "up" # Select the thumbnail above the current one
---| "down" # Select the thumbnail below the current one
---| "left" # Select the thumbnail to the left of the current one
---| "right" # Select the thumbnail to the right of the current one
---| "pgup" # Select the thumbnail on the previous page
---| "pgdown" # Select the thumbnail on the next page

---@alias aspect_t
---| "fit" # Fit image into a square thumbnail
---| "fill" # Fill square thumbnail with the image
---| "keep" # Adjust thumbnail size to the aspect ratio of the image

---@class gallery : mode_base
---@field aspect aspect_t Thumbnail aspect ratio
---@field thumb_size integer Thumbnail size in pixels
---@field padding_size integer Padding between thumbnails in pixels
---@field border_size integer Border size for the selected thumbnail in pixels
---@field border_color integer Border color for the selected thumbnail in ARGB format
---@field selected_scale number Scale factor for the selected thumbnail (1.0 = 100%)
---@field selected_color integer Background color for the selected thumbnail in ARGB format
---@field background_color integer Background color for unselected thumbnails in ARGB format
---@field window_color integer Window background color in ARGB format
---@field mark_color integer Mark icon color in ARGB format
---@field cache_size integer Max number of thumbnails stored in memory cache
---@field preload boolean Preload invisible thumbnails
---@field pstore boolean Persistent storage for thumbnails
---@field pstore_path string Custom path to the directory for persistent thumbnail storage
swayimg.gallery = {}

---Select the next thumbnail from the gallery.
---@param dir gdir_t Next thumbnail direction
function swayimg.gallery.switch_image(dir) end

---Get information about currently selected image.
---@return image_entry # Currently selected image entry
function swayimg.gallery.get_current_image() end
