---@meta swayimg

---Main class.
---@class swayimg
swayimg = {}

---Image list.
---@class imagelist
swayimg.imagelist = {}

---Text overlay.
---@class text
swayimg.text = {}

---Viewer mode.
---@class viewer
swayimg.viewer = {}

---Slideshow mode.
---@class slideshow
swayimg.slideshow = {}

---Gallery mode.
---@class gallery
swayimg.gallery = {}

--------------------------------------------------------------------------------
-- General functionality
--------------------------------------------------------------------------------

---@alias appmode_t
---| "viewer" # Image viewer mode
---| "slideshow" # Slide show mode
---| "gallery" # Gallery mode

---Exit from application.
---@param code? number Program exit code, `0` by default
function swayimg.exit(code) end

---Set window title.
---@param title string Window title text
function swayimg.set_title(title) end

---Show status message.
---@param status string Status text to show
function swayimg.set_status(status) end

---Switch to specified mode.
---@param mode appmode_t Mode to activate
function swayimg.set_mode(mode) end

---Get current mode.
---@return appmode_t # Currently active mode
function swayimg.get_mode() end

---Get application window size.
---@return table # (width, height) tuple with window size in pixels
function swayimg.get_window_size() end

---Set application window size.
---@param width number Width of the window in pixels
---@param height number Height of the window in pixels
function swayimg.set_window_size(width, height) end

---Get mouse pointer coordinates.
---@return table # (x, y) tuple with mouse pointer coordinates
function swayimg.get_mouse_pos() end

---Bind the mouse button for drag-and-drop image to external applications.
---This function can be called only at startup.
---@param button string Button description, for example `MouseRight`
function swayimg.set_drag_button(button) end

---Enable or disable antialiasing.
---@param enable boolean Enable/disable flag to set
function swayimg.enable_antialiasing(enable) end

---Enable or disable window decoration (title, border, buttons).
---This function available only in Wayland, the corresponding protocol must be
---supported by the composer.
---By default disabled in Sway and enabled in other compositors.
---@param enable boolean Enable/disable flag to set
function swayimg.enable_decoration(enable) end

---Enable or disable window overlay mode.
---Sway and Hyprland compositors only.
---Create a floating window with the same coordinates and size as the currently
---focused window. This variable can be set only once.
---By default enabled in Sway and disabled in other compositors.
---@param enable boolean Enable/disable flag to set
function swayimg.enable_overlay(enable) end

---Set the initialization completion handler.
---Called after all subsystems have been initialized.
---@param fn function Initialization completion callback
function swayimg.on_initialized(fn) end

--------------------------------------------------------------------------------
-- Image list
--------------------------------------------------------------------------------

---@alias order_t
---| "none" # Unsorted (system depended)
---| "alpha" # Lexicographic sort: 1,10,2,20,a,b,c
---| "numeric" # Numeric sort: 1,2,3,10,100,a,b,c
---| "mtime" # Modification time sort
---| "size" # Size sort
---| "random" # Random order

---Get size of image list.
---@return number # Number of entries in the image list
function swayimg.imagelist.size() end

---Get list of all entries in the image list.
---@return table[] # Array with all entries
function swayimg.imagelist.get() end

---Add entry to the image list.
---@param path string Path to add
function swayimg.imagelist.add(path) end

---Remove entry from the image list.
---@param path string Path to remove
function swayimg.imagelist.remove(path) end

---Set, clear or toggle mark for currently viewed/selected image.
---@see swayimg.viewer.set_mark_color
---@see swayimg.slideshow.set_mark_color
---@see swayimg.gallery.set_mark_color
---@param state? boolean Mark state to set, toggle if the state is not specified
function swayimg.imagelist.mark(state) end

---Set image list order.
---@param order order_t List order
function swayimg.imagelist.set_order(order) end

---Enable or disable reverse order.
---@param enable boolean Enable/disable flag to set
function swayimg.imagelist.enable_reverse(enable) end

---Enable or disable recursive directory reading.
---@param enable boolean Enable/disable flag to set
function swayimg.imagelist.enable_recursive(enable) end

---Enable or disable opening adjacent files from the same directory.
---@param enable boolean Enable/disable flag to set
function swayimg.imagelist.enable_adjacent(enable) end

--------------------------------------------------------------------------------
-- Text layer
--------------------------------------------------------------------------------

---Set font face.
---@param name string Font name
function swayimg.text.set_font(name) end

---Set font size.
---@param size number Font size in pixels
function swayimg.text.set_size(size) end

---Set the padding from window edges.
---@param size number Padding size in pixels
function swayimg.text.set_padding(size) end

---Set foreground text color.
---@param color number Color in ARGB format, e.g. `0xff00aa99`
function swayimg.text.set_foreground(color) end

---Set background text color.
---@param color number Color in ARGB format, e.g. `0xff00aa99`
function swayimg.text.set_background(color) end

---Set shadow text color.
---@param color number Color in ARGB format, e.g. `0xff00aa99`
function swayimg.text.set_shadow(color) end

---Set a timeout after which the entire text layer will be hidden.
---@param seconds number Timeout in seconds
function swayimg.text.set_overall_timer(seconds) end

---Set a timeout after which the status message will be hidden.
---@param seconds number Timeout in seconds
function swayimg.text.set_status_timer(seconds) end

---Show text layer and stop the time.
function swayimg.text.show() end

---Hide text layer and stop the timer.
function swayimg.text.hide() end

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

---Open next file at specified direction.
---@param dir vdir_t Next file direction
function swayimg.viewer.open(dir) end

---Get information about currently viewed image.
---@return table # Dictionary with image properties: path, size, meta data, etc
function swayimg.viewer.current_image() end

---Get current image scale.
---@return number # Absolute scale value (1.0 = 100%)
function swayimg.viewer.get_scale() end

---Set absolute image scale.
---@param scale number Absolute value (1.0 = 100%)
---@param x? number X coordinate of center point, empty for window center
---@param y? number Y coordinate of center point, empty for window center
function swayimg.viewer.set_abs_scale(scale, x, y) end

---Set fixed image scale.
---@param scale fixed_scale_t Fixed scale name
function swayimg.viewer.set_fix_scale(scale) end

---Reset scale to default value.
---@see swayimg.viewer.set_default_scale
function swayimg.viewer.reset_scale() end

---Set default image scale for newly opened images.
---@param scale number|fixed_scale_t Absolute value (1.0 = 100%) or one the predefined names
function swayimg.viewer.set_default_scale(scale) end

---Get image position.
---@return table # (x, y) tuple with image coordinates on the window
function swayimg.viewer.get_position() end

---Set absolute image position.
---@param x number Horizontal image position on the window
---@param y number Vertical image position on the window
function swayimg.viewer.set_abs_position(x, y) end

---Set fixed image position.
---@param pos fixed_position_t Fixed image position
function swayimg.viewer.set_fix_position(pos) end

---Set default image position for newly opened images.
---@param pos fixed_position_t Fixed image position
function swayimg.viewer.set_default_position(pos) end

---Show next frame from multi-frame image (animation).
---This function also stops the animation.
---@see swayimg.viewer.animation_stop
---@see swayimg.viewer.animation_resume
---@return number # Index of the currently shown frame
function swayimg.viewer.next_frame() end

---Show previous frame from multi-frame image (animation).
---This function also stops the animation.
---@see swayimg.viewer.animation_stop
---@see swayimg.viewer.animation_resume
---@return number # Index of the currently shown frame
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

---Set window background color and mode.
---@param bkg number|bkgmode_t Solid color or one of the predefined mode
function swayimg.viewer.set_window_background(bkg) end

---Set background color for transparent images.
---This disables grid drawing.
---@param color number Color in ARGB format, e.g. `0xff00aa99`
function swayimg.viewer.set_image_background(color) end

---Set parameters for grid used as background for transparent images.
---@param size number Size of single grid cell in pixels
---@param color1 number First color in ARGB format, e.g. `0xff00aa99`
---@param color2 number Second color in ARGB format, e.g. `0xff00aa99`
function swayimg.viewer.set_image_grid(size, color1, color2) end

---Set mark icon color.
---@see swayimg.imagelist.mark
---@param color number Color in ARGB format, e.g. `0xff00aa99`
function swayimg.viewer.set_mark_color(color) end

---Add/replace/remove meta info for current image.
---@param key string Meta key name
---@param value string Meta value, empty value to remove
function swayimg.viewer.set_meta(key, value) end

---Export currently viewed frame to PNG file.
---@param path string Path to file
function swayimg.viewer.export(path) end

---Add a callback function called when a new image is opened.
---@param fn function Callback handler
function swayimg.viewer.on_change_image(fn) end

---Add a callback function called when main window is resized.
---@param fn function Callback handler
function swayimg.viewer.on_window_resize(fn) end

---Bind the key press event to a handler.
---@param key string Key description, for example `Ctrl-a`
---@param fn function Key press handler
function swayimg.viewer.on_key(key, fn) end

---Bind the mouse button press event to a handler.
---@param button string Button description, for example `Ctrl-Alt-MouseRight`
---@param fn function Button press handler
function swayimg.viewer.on_mouse(button, fn) end

---Bind the mouse state to image drag operation.
---@param button string Button description, for example `MouseLeft`
function swayimg.viewer.bind_drag(button) end

---Bind the signal event to a handler.
---@param signal string Signal name: `USR1` or `USR2`
---@param fn function Signal handler
function swayimg.viewer.on_signal(signal, fn) end

---Remove all existing key/mouse/signal bindings.
function swayimg.viewer.bind_reset() end

---Enable or disable free move mode.
---@param enable boolean Enable/disable flag to set
function swayimg.viewer.enable_freemove(enable) end

---Enable or disable image list loop mode.
---@param enable boolean Enable/disable flag to set
function swayimg.viewer.enable_loop(enable) end

---Set number of images to preload in a separate thread.
---@param size number Number of images to preload
function swayimg.viewer.set_preload_limit(size) end

---Set number of previously viewed images to store in cache.
---@param size number Number of images to store
function swayimg.viewer.set_history_limit(size) end

---Set text layer scheme for top left corner of the window.
---@param scheme string[] Array with text overlay scheme
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
function swayimg.viewer.set_text_tl(scheme) end

---Set text layer scheme for top right corner of the window.
---@see swayimg.viewer.set_text_tl for scheme description
---@param scheme string[] Text overlay scheme
function swayimg.viewer.set_text_tr(scheme) end

---Set text layer scheme for bottom left corner of the window.
---@see swayimg.viewer.set_text_tl for scheme description
---@param scheme string[] Text overlay scheme
function swayimg.viewer.set_text_bl(scheme) end

---Set text layer scheme for bottom right corner of the window.
---@see swayimg.viewer.set_text_tl for scheme description
---@param scheme string[] Text overlay scheme
function swayimg.viewer.set_text_br(scheme) end

--------------------------------------------------------------------------------
-- Slide show mode
--------------------------------------------------------------------------------

---Set a timeout after which next image should be opened.
---@param seconds number Timeout in seconds
function swayimg.slideshow.set_time(seconds) end

---Open next file at specified direction.
---@param dir vdir_t Next file direction
function swayimg.slideshow.open(dir) end

---Get information about currently viewed image.
---@return table # Dictionary with image properties: path, size, meta data, etc
function swayimg.slideshow.current_image() end

---Get current image scale.
---@return number # Absolute scale value (1.0 = 100%)
function swayimg.slideshow.get_scale() end

---Set absolute image scale.
---@param scale number Absolute value (1.0 = 100%)
---@param x? number X coordinate of center point, empty for window center
---@param y? number Y coordinate of center point, empty for window center
function swayimg.slideshow.set_abs_scale(scale, x, y) end

---Set fixed image scale.
---@param scale fixed_scale_t Fixed scale name
function swayimg.slideshow.set_fix_scale(scale) end

---Reset scale to default value.
---@see swayimg.slideshow.set_default_scale
function swayimg.slideshow.reset_scale() end

---Set default image scale for newly opened images.
---@param scale number|fixed_scale_t Absolute value (1.0 = 100%) or one the predefined names
function swayimg.slideshow.set_default_scale(scale) end

---Get image position.
---@return table # (x, y) tuple with image coordinates on the window
function swayimg.slideshow.get_position() end

---Set absolute image position.
---@param x number Horizontal image position on the window
---@param y number Vertical image position on the window
function swayimg.slideshow.set_abs_position(x, y) end

---Set fixed image position.
---@param pos fixed_position_t Fixed image position
function swayimg.slideshow.set_fix_position(pos) end

---Set default image position for newly opened images.
---@param pos fixed_position_t Fixed image position
function swayimg.slideshow.set_default_position(pos) end

---Show next frame from multi-frame image (animation).
---@see swayimg.slideshow.animation_stop
---@see swayimg.slideshow.animation_resume
---@return number # Index of the currently shown frame
function swayimg.slideshow.next_frame() end

---Show previous frame from multi-frame image (animation).
---@see swayimg.slideshow.animation_stop
---@see swayimg.slideshow.animation_resume
---@return number # Index of the currently shown frame
function swayimg.slideshow.prev_frame() end

---Flip image vertically.
function swayimg.slideshow.flip_vertical() end

---Flip image horizontally.
function swayimg.slideshow.flip_horizontal() end

---Rotate image.
---@param angle rotation_t Rotation angle
function swayimg.slideshow.rotate(angle) end

---Stop animation.
function swayimg.slideshow.animation_stop() end

---Resume animation.
function swayimg.slideshow.animation_resume() end

---Set window background color and mode.
---@param bkg number|bkgmode_t Solid color or one of the predefined mode
function swayimg.slideshow.set_window_background(bkg) end

---Set background color for transparent images.
---This disables grid drawing.
---@param color number Color in ARGB format, e.g. `0xff00aa99`
function swayimg.slideshow.set_image_background(color) end

---Set parameters for grid used as background for transparent images.
---@param size number Size of single grid cell in pixels
---@param color1 number First color in ARGB format, e.g. `0xff00aa99`
---@param color2 number Second color in ARGB format, e.g. `0xff00aa99`
function swayimg.slideshow.set_image_grid(size, color1, color2) end

---Set mark icon color.
---@see swayimg.imagelist.mark
---@param color number Color in ARGB format, e.g. `0xff00aa99`
function swayimg.slideshow.set_mark_color(color) end

---Add/replace/remove meta info for current image.
---@param key string Meta key name
---@param value string Meta value, empty value to remove
function swayimg.slideshow.set_meta(key, value) end

---Export currently viewed frame to PNG file.
---@param path string Path to file
function swayimg.slideshow.export(path) end

---Add a callback function called when a new image is opened.
---@param fn function Callback handler
function swayimg.slideshow.on_change_image(fn) end

---Add a callback function called when main window is resized.
---@param fn function Callback handler
function swayimg.slideshow.on_window_resize(fn) end

---Bind the key press event to a handler.
---@param key string Key description, for example `Ctrl-a`
---@param fn function Key press handler
function swayimg.slideshow.on_key(key, fn) end

---Bind the mouse button press event to a handler.
---@param button string Button description, for example `Ctrl-Alt-MouseRight`
---@param fn function Button press handler
function swayimg.slideshow.on_mouse(button, fn) end

---Bind the mouse state to image drag operation.
---@param button string Button description, for example `MouseLeft`
function swayimg.slideshow.bind_drag(button) end

---Bind the signal event to a handler.
---@param signal string Signal name: `USR1` or `USR2`
---@param fn function Signal handler
function swayimg.slideshow.on_signal(signal, fn) end

---Remove all existing key/mouse/signal bindings.
function swayimg.slideshow.bind_reset() end

---Enable or disable free move mode.
---@param enable boolean Enable/disable flag to set
function swayimg.slideshow.enable_freemove(enable) end

---Enable or disable image list loop mode.
---@param enable boolean Enable/disable flag to set
function swayimg.slideshow.enable_loop(enable) end

---Set number of images to preload in a separate thread.
---@param size number Number of images to preload
function swayimg.slideshow.set_preload_limit(size) end

---Set number of previously viewed images to store in cache.
---@param size number Number of images to store
function swayimg.slideshow.set_history_limit(size) end

---Set text layer scheme for top left corner of the window.
---@param scheme string[] Array with text overlay scheme
---@see swayimg.viewer.set_text_tl for scheme description
function swayimg.slideshow.set_text_tl(scheme) end

---Set text layer scheme for top right corner of the window.
---@see swayimg.viewer.set_text_tl for scheme description
---@param scheme string[] Text overlay scheme
function swayimg.slideshow.set_text_tr(scheme) end

---Set text layer scheme for bottom left corner of the window.
---@see swayimg.viewer.set_text_tl for scheme description
---@param scheme string[] Text overlay scheme
function swayimg.slideshow.set_text_bl(scheme) end

---Set text layer scheme for bottom right corner of the window.
---@see swayimg.viewer.set_text_tl for scheme description
---@param scheme string[] Text overlay scheme
function swayimg.slideshow.set_text_br(scheme) end

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

---Select the next thumbnail from the gallery.
---@param dir gdir_t Next thumbnail direction
function swayimg.gallery.select(dir) end

---Get information about currently selected image.
---@return table # Dictionary with image properties: path, size, etc
function swayimg.gallery.current_image() end

---Set thumbnail aspect ratio.
---@param aspect aspect_t Thumbnail aspect ratio
function swayimg.gallery.set_aspect(aspect) end

---Get thumbnail size.
---@return number # Thumbnail size in pixels
function swayimg.gallery.get_thumb_size() end

---Set thumbnail size.
---@param size number Thumbnail size in pixels
function swayimg.gallery.set_thumb_size(size) end

---Set the padding size between thumbnails.
---@param size number Padding size in pixels
function swayimg.gallery.set_padding_size(size) end

---Set the border size for currently selected thumbnail.
---@param size number Border size in pixels
function swayimg.gallery.set_border_size(size) end

---Set border color for currently selected thumbnail.
---@param color number Color in ARGB format, e.g. `0xff00aa99`
function swayimg.gallery.set_border_color(color) end

---Set the scale factor for currently selected thumbnail.
---@param scale number Scale factor, 1.0 = 100%
function swayimg.gallery.set_selected_scale(scale) end

---Set background color for currently selected thumbnail.
---@param color number Color in ARGB format, e.g. `0xff00aa99`
function swayimg.gallery.set_selected_color(color) end

---Set background color for unselected thumbnails.
---@param color number Color in ARGB format, e.g. `0xff00aa99`
function swayimg.gallery.set_background_color(color) end

---Set window background color.
---@param color number Color in ARGB format, e.g. `0xff00aa99`
function swayimg.gallery.set_window_color(color) end

---Set mark icon color.
---@see swayimg.imagelist.mark
---@param color number Color in ARGB format, e.g. `0xff00aa99`
function swayimg.gallery.set_mark_color(color) end

---Add a callback function called when a new image is selected.
---@param fn function Callback handler
function swayimg.gallery.on_change_image(fn) end

---Add a callback function called when main window is resized.
---@param fn function Callback handler
function swayimg.gallery.on_window_resize(fn) end

---Bind the key press event to a handler.
---@param key string Key description, for example `Ctrl-a`
---@param fn function Key press handler
function swayimg.gallery.on_key(key, fn) end

---Bind the mouse button press event to a handler.
---@param button string Button description, for example `MouseRight`
---@param fn function Button press handler
function swayimg.gallery.on_mouse(button, fn) end

---Bind the signal event to a handler.
---@param signal string Signal name: `USR1` or `USR2`
---@param fn function Signal handler
function swayimg.gallery.on_signal(signal, fn) end

---Remove all existing key/mouse/signal bindings.
function swayimg.gallery.bind_reset() end

---Set max number of thumbnails stored in memory cache.
---@param size number Cache size
function swayimg.gallery.set_cache_size(size) end

---Enable or disable preloading invisible thumbnails.
---@param enable boolean Enable/disable flag to set
function swayimg.gallery.enable_preload(enable) end

---Enable or disable persistent storage for thumbnails.
---@param enable boolean Enable/disable flag to set
function swayimg.gallery.enable_pstore(enable) end

---Set custom path for persistent storage for thumbnails.
---@param path string Path to the directory
function swayimg.gallery.set_pstore_path(path) end

return swayimg
