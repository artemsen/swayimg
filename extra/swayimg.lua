---@meta swayimg

---Application mode
---@alias appmode_t
---| "viewer"    # Image viewer mode
---| "slideshow" # Slide show mode
---| "gallery"   # Gallery mode

---ARGB color in hex format: AARRGGBB, for example `0xff00aa99`
---@alias color_t integer

---Image list order
---@alias order_t
---| "none"    # Unsorted (system-dependent)
---| "alpha"   # Lexicographic sort: 1,10,2,20,a,b,c
---| "numeric" # Numeric sort: 1,2,3,10,100,a,b,c
---| "mtime"   # Modification time sort
---| "size"    # Size sort
---| "random"  # Random order

---Direction for opening next file in viewer and slideshow modes
---@alias vdir_t
---| "first"    # First file in image list
---| "last"     # Last file in image list
---| "next"     # Next file
---| "prev"     # Previous file
---| "next_dir" # First file in next directory
---| "prev_dir" # Last file in previous directory
---| "random"   # Random file in image list

---Fixed scale for images in viewer and slideshow modes
---@alias fixed_scale_t
---| "optimal" # 100% or less to fit to window
---| "width"   # Fit image width to window width
---| "height"  # Fit image height to window height
---| "fit"     # Fit to window
---| "fill"    # Crop image to fill the window
---| "real"    # Real size (100%)
---| "keep"    # Keep the same scale as for previously viewed image

---Fixed position for images in viewer and slideshow modes
---@alias fixed_position_t
---| "center"       # Vertical and horizontal center of the window
---| "topcenter"    # Top (vertical) and center (horizontal) of the window
---| "bottomcenter" # Bottom (vertical) and center (horizontal) of the window
---| "leftcenter"   # Left (horizontal) and center (vertical) of the window
---| "rightcenter"  # Right (horizontal) and center (vertical) of the window
---| "topleft"      # Top left corner of the window
---| "topright"     # Top right corner of the window
---| "bottomleft"   # Bottom left corner of the window
---| "bottomright"  # Bottom right corner of the window

---Fixed rotation angles for images in viewer and slideshow modes
---@alias rotation_t
---| 90  # 90 degrees
---| 180 # 180 degrees
---| 270 # 270 degrees

---Fixed rotation angles for images in viewer and slideshow modes
---@alias bkgmode_t
---| "extend" # Fill window with the current image and blur it
---| "mirror" # Fill window with the mirrored current image and blur it
---| "auto"   # Fill the window background in `extend` or `mirror` mode depending on the image aspect ratio

---Direction for selecting next file in gallery mode
---@alias gdir_t
---| "first"  # Select first thumbnail in image list
---| "last"   # Select last thumbnail in image list
---| "up"     # Select the thumbnail above the current one
---| "down"   # Select the thumbnail below the current one
---| "left"   # Select the thumbnail to the left of the current one
---| "right"  # Select the thumbnail to the right of the current one
---| "pgup"   # Select the thumbnail on the previous page
---| "pgdown" # Select the thumbnail on the next page

---Aspect ratio used for thumbnails in gallery mode
---@alias aspect_t
---| "fit"  # Fit image into a square thumbnail
---| "fill" # Fill square thumbnail with the image
---| "keep" # Adjust thumbnail size to the aspect ratio of the image

---Position of text block
---@alias block_position_t
---| "topleft"      # Top left corner of the window
---| "topright"     # Top right corner of the window
---| "bottomleft"   # Bottom left corner of the window
---| "bottomright"  # Bottom right corner of the window

---Template for text overlay line.
---The template includes text and fields surrounded by curly braces.
---The following fields are supported:
---* `{name}`: File name of the currently viewed/selected image
---* `{dir}`: Parent directory name of the currently viewed/selected image
---* `{path}`: Absolute path to the currently viewed/selected image
---* `{size}`: File size in bytes
---* `{sizehr}`: File size in human-readable format
---* `{time}`: File modification time
---* `{format}`: Brief image format descriptio
---* `{scale}`: Current image scale in percent
---* `{list.index}`: Current index of image in the image list
---* `{list.total}`: Total number of files in the image list
---* `{frame.index}`: Current frame index
---* `{frame.total}`: Total number of frames
---* `{frame.width}`: Current frame width in pixels
---* `{frame.height}`: Current frame height in pixels
---* `{meta.*}`: Image meta info: EXIF, tags etc. List of available tags can be
---  found at [Exiv2 website](https://exiv2.org/tags.html) or printed using
---  utility exiv2: `exiv2 -pa photo.jpg`
---
---Example: `Path to image: {path}`
---@alias text_template_t string

--------------------------------------------------------------------------------

---Read only file entry from the image list.
---@class swayimg.entry
---@field index integer Index in the image list
---@field path string Absolute path to the file
---@field size integer File size in bytes
---@field mtime string File modification time
---@field mark boolean Whether the image is marked

---Read only image description.
---@class swayimg.image : swayimg.entry
---@field format string Brief image format description
---@field frames integer Total number of frames in the image
---@field width integer Width of the currently displayed frame
---@field height integer Height of the currently displayed frame
---@field meta table<string, string> Image meta info: EXIF, tags, etc

--------------------------------------------------------------------------------

---General functionality
---@class swayimg
swayimg = {}

---Exit from application.
---@param code? integer Program exit code, `0` by default
function swayimg.exit(code) end

---Switch to specified mode.
---@param mode appmode_t Mode to activate
function swayimg.set_mode(mode) end

---Get current mode.
---@return appmode_t # Currently active mode
function swayimg.get_mode() end

---Set title for main application window.
---@param title string Window title text
function swayimg.set_title(title) end

---Get application window size.
---@return { width: integer, height: integer } # Window size in pixels
function swayimg.get_window_size() end

---Set application window size.
---@param width integer Width of the window in pixels
---@param height integer Height of the window in pixels
function swayimg.set_window_size(width, height) end

---Add a callback function called when main window is resized.
---@param fn function Window resize notification handler
function swayimg.on_window_resize(fn) end

---Get mouse pointer coordinates.
---@return { x :integer, y: integer } # Coordinates of the mouse pointer
function swayimg.get_mouse_pos() end

---Toggle full screen mode.
---@return boolean # True if full screen is enabled
function swayimg.toggle_fullscreen() end

---Add a callback function called when all subsystems have been initialized.
---@param fn function Initialization completion notification handler
function swayimg.on_initialized(fn) end

---Enable or disable antialiasing.
---@param enable boolean Enable/disable antialiasing
function swayimg.enable_antialiasing(enable) end

---Enable or disable changing orientation based on EXIF.
---@param enable boolean Enable/disable orientation change
function swayimg.enable_exif_orientation(enable) end

---Enable or disable window decoration (title, border, buttons).
---This function can only be called at program startup.
---Applicable only in Wayland, the corresponding protocol must be supported by
---the composer.
---By default disabled in Sway and enabled in other compositors.
---@param enable boolean Enable/disable window decoration
function swayimg.enable_decoration(enable) end

---Enable or disable window overlay mode.
---Create a floating window with the same coordinates and size as the currently
---focused window.
---This function can only be called at program startup.
---Applicable only in Sway and Hyprland compositors.
---By default enabled in Sway and disabled in other compositors.
---@param enable boolean Enable/disable overlay mode
function swayimg.enable_overlay(enable) end

---Set mouse button used for drag-and-drop image file to external apps.
---This function can only be called at program startup.
---@param button string Mouse button used for drag-n-drop, for example `MouseRight`
function swayimg.set_dnd_button(button) end

--------------------------------------------------------------------------------

---Image list
---@class swayimg.imagelist
swayimg.imagelist = {}

---Get number of entries in the image list.
---@return integer # Size of the image list
function swayimg.imagelist.size() end

---Get list of all entries in the image list.
---@return swayimg.entry[] # Array with all file entries
function swayimg.imagelist.get() end

---Add entry to the image list.
---@param path string Path to add
function swayimg.imagelist.add(path) end

---Remove entry from the image list.
---@param path string Path to remove
function swayimg.imagelist.remove(path) end

---Set sort order of the image list.
---@param order order_t List order
function swayimg.imagelist.set_order(order) end

---Enable or disable reverse order.
---@param enable boolean Enable/disable reverse order
function swayimg.imagelist.enable_reverse(enable) end

---Enable or disable recursive directory reading.
---@param enable boolean Enable/disable recursive mode
function swayimg.imagelist.enable_recursive(enable) end

---Enable or disable adding adjacent files from the same directory.
---This function can only be called at program startup.
---@param enable boolean Enable/disable adding adjacent files
function swayimg.imagelist.enable_adjacent(enable) end

---Enable or disable file system monitoring.
---@param enable boolean Enable/disable FS monitor
function swayimg.imagelist.enable_fsmon(enable) end

--------------------------------------------------------------------------------

---Text overlay layer
---@class swayimg.text
swayimg.text = {}

---Force show the text layer.
---This function restarts the timer.
---@see swayimg.text.set_timer
function swayimg.text.show() end

---Hide the text layer.
function swayimg.text.hide() end

---Check if text layer is visible.
---@return boolean # `true` if text layer is visible
function swayimg.text.visible() end

---Set font face.
---@param name string Font name
function swayimg.text.set_font(name) end

---Set font size.
---@param size integer Font size in pixels
function swayimg.text.set_size(size) end

---Set line spacing.
---@param size integer Line spacing in pixels, can be negative
function swayimg.text.set_spacing(size) end

---Set the padding from the window edges.
---@param size integer Padding size in pixels
function swayimg.text.set_padding(size) end

---Set foreground text color.
---@param color color_t Foreground text color
function swayimg.text.set_foreground(color) end

---Set background text color.
---@param color color_t Background text color
function swayimg.text.set_background(color) end

---Set shadow text color.
---Setting alpha channel to `0` disables shadows.
---@param color color_t Shadow text color
function swayimg.text.set_shadow(color) end

---Set a timeout after which the entire text layer will be hidden.
---Setting the timeout value to `0` disables the timer and causes the overlay
---to be displayed continuously.
---@param seconds number Timeout in seconds
function swayimg.text.set_timeout(seconds) end

---Set a timeout after which the status message will be hidden.
---Setting the timeout value to `0` disables the timer and causes the status
---message to be displayed continuously.
---@see swayimg.text.set_status
---@param seconds number Timeout in seconds
function swayimg.text.set_status_timeout(seconds) end

---Show status message.
---Multi-line text is separated by `\n`.
---@see swayimg.text.set_status_timer
---@param status string Status text to show
function swayimg.text.set_status(status) end

--------------------------------------------------------------------------------

---Base application mode
---@class swayimg_appmode
local swayimg_appmode = {}

---Set, clear or toggle mark for currently viewed/selected image.
---@param state? boolean Mark state to set, toggle if the state is not specified
function swayimg_appmode.mark_image(state) end

---Set mark icon color.
---@param color color_t Mark icon color
function swayimg_appmode.set_mark_color(color) end

---Remove all existing key/mouse/signal bindings.
function swayimg_appmode.bind_reset() end

---Bind the key press event to a handler.
---@param key string Key description, for example `Ctrl-a`
---@param fn function Key press handler
function swayimg_appmode.on_key(key, fn) end

---Bind the mouse button press event to a handler.
---@param button string Button description, for example `Ctrl-Alt-MouseRight`
---@param fn function Button press handler
function swayimg_appmode.on_mouse(button, fn) end

---Bind the signal event to a handler.
---@param signal string Signal name (`USR1` or `USR2`)
---@param fn function Signal handler
function swayimg_appmode.on_signal(signal, fn) end

---Add a callback function called when a new image is opened/selected.
---@param fn function Handler for notifications about changing the current image
function swayimg_appmode.on_image_change(fn) end

---Set text layer scheme.
---@param pos block_position_t Text block position
---@param scheme text_template_t[] Array of line templates with overlay scheme
function swayimg_appmode.set_text(pos, scheme) end

--------------------------------------------------------------------------------

---Viewer mode
---@class swayimg.viewer : swayimg_appmode
swayimg.viewer = {}

---Open the next file in the specified direction.
---@param dir vdir_t Next file direction
function swayimg.viewer.switch_image(dir) end

---Get information about currently displayed image.
---@return swayimg.image # Currently displayed image
function swayimg.viewer.get_image() end

---Reload current image.
function swayimg.viewer.reload() end

---Reset position and scale to default values.
---@see swayimg.viewer.set_default_scale
---@see swayimg.viewer.set_default_position
function swayimg.viewer.reset() end

---Get current image scale.
---@return number # Absolute scale value (1.0 = 100%)
function swayimg.viewer.get_scale() end

---Set absolute image scale.
---@param scale number Absolute value (1.0 = 100%)
---@param x? integer X coordinate of center point, empty for window center
---@param y? integer Y coordinate of center point, empty for window center
function swayimg.viewer.set_abs_scale(scale, x, y) end

---Set fixed scale for currently displayed image.
---@param scale fixed_scale_t Fixed scale name
function swayimg.viewer.set_fix_scale(scale) end

---Set default image scale for newly opened images.
---@param scale number|fixed_scale_t Absolute value (1.0 = 100%) or one the predefined names
function swayimg.viewer.set_default_scale(scale) end

---Get image position.
---@return { x :integer, y: integer } # Image coordinates on the window
function swayimg.viewer.get_position() end

---Set absolute image position.
---@param x integer Horizontal image position on the window
---@param y integer Vertical image position on the window
function swayimg.viewer.set_abs_position(x, y) end

---Set fixed image position.
---@param pos fixed_position_t Fixed image position
function swayimg.viewer.set_fix_position(pos) end

---Set default image position for newly opened images.
---@param pos fixed_position_t Fixed image position
function swayimg.viewer.set_default_position(pos) end

---Show next frame from multi-frame image (animation).
---This function stops the animation.
---@return integer # Index of the currently shown frame
function swayimg.viewer.next_frame() end

---Show previous frame from multi-frame image (animation).
---This function stops the animation.
---@return integer # Index of the currently shown frame
function swayimg.viewer.prev_frame() end

---Stop animation.
function swayimg.viewer.animation_stop() end

---Resume animation.
function swayimg.viewer.animation_resume() end

---Flip image vertically.
function swayimg.viewer.flip_vertical() end

---Flip image horizontally.
function swayimg.viewer.flip_horizontal() end

---Rotate image.
---@param angle rotation_t Rotation angle
function swayimg.viewer.rotate(angle) end

---Export currently displayed frame to PNG file.
---@param path string Path to the file
function swayimg.viewer.export(path) end

---Add/replace/remove meta info for currently displayed image.
---@param key string Meta key name
---@param value string Meta value, empty value to remove the record
function swayimg.viewer.set_meta(key, value) end

---Set the mouse button used to drag the image around the window.
---@param button string Mouse button name, for example `MouseLeft`
function swayimg.viewer.set_drag_button(button) end

---Set window background color and mode.
---@param bkg color_t|bkgmode_t Solid color or one of the predefined mode
function swayimg.viewer.set_window_background(bkg) end

---Set background color for transparent images.
---This disables chessboard drawing.
---@param color color_t Background color
function swayimg.viewer.set_image_background(color) end

---Set parameters for chessboard used as background for transparent images.
---This enables the chessboard if this feature was previously disabled.
---@param size integer Size of single grid cell in pixels
---@param color1 color_t First color
---@param color2 color_t Second color
function swayimg.viewer.set_image_chessboard(size, color1, color2) end

---Enable or disable automatic image centering.
---@param enable boolean Enable/disable automatic image centering
function swayimg.viewer.enable_centering(enable) end

---Enable or disable image list loop mode.
---@param enable boolean Enable/disable flag to set
function swayimg.viewer.enable_loop(enable) end

---Set max number of images to preload in background thread.
---@param size integer Number of images to preload
function swayimg.viewer.limit_preload(size) end

---Set max number of previously viewed images stored in the cache.
---@param size integer Number of images to store
function swayimg.viewer.limit_history(size) end

--------------------------------------------------------------------------------

---Slide show mode
---@class swayimg.slideshow : swayimg.viewer
swayimg.slideshow = {}

---Set a timeout after which next image should be opened.
---@param seconds number Timeout in seconds
function swayimg.slideshow.set_timeout(seconds) end

--------------------------------------------------------------------------------

---Gallery mode
---@class swayimg.gallery : swayimg_appmode
swayimg.gallery = {}

---Select the next thumbnail from the gallery.
---@param dir gdir_t Next thumbnail direction
function swayimg.gallery.switch_image(dir) end

---Get information about currently selected image entry.
---@return swayimg.entry # Currently selected image entry
function swayimg.gallery.get_image() end

---Set thumbnail aspect ratio.
---@param aspect aspect_t Thumbnail aspect ratio
function swayimg.gallery.set_aspect(aspect) end

---Get thumbnail size.
---@return integer # Thumbnail size in pixels
function swayimg.gallery.get_thumb_size() end

---Set thumbnail size.
---@param size integer Thumbnail size in pixels
function swayimg.gallery.set_thumb_size(size) end

---Set the padding size between thumbnails.
---@param size integer Padding size in pixels
function swayimg.gallery.set_padding_size(size) end

---Set the border size for currently selected thumbnail.
---@param size integer Border size in pixels
function swayimg.gallery.set_border_size(size) end

---Set border color for currently selected thumbnail.
---@param color color_t Border color
function swayimg.gallery.set_border_color(color) end

---Set the scale factor for currently selected thumbnail.
---@param scale number Scale factor, 1.0 = 100%
function swayimg.gallery.set_selected_scale(scale) end

---Set background color for currently selected thumbnail.
---@param color color_t Background color
function swayimg.gallery.set_selected_color(color) end

---Set background color for unselected thumbnails.
---@param color color_t Background color
function swayimg.gallery.set_unselected_color(color) end

---Set window background color.
---@param color color_t Background color
function swayimg.gallery.set_window_color(color) end

---Set max number of thumbnails stored in memory cache.
---@param size integer Cache size
function swayimg.gallery.limit_cache(size) end

---Enable or disable preloading invisible thumbnails.
---@param enable boolean Enable/disable preloading invisible thumbnails
function swayimg.gallery.enable_preload(enable) end

---Enable or disable persistent storage for thumbnails.
---@param enable boolean Enable/disable usage of persistent storage
function swayimg.gallery.enable_pstore(enable) end

---Set custom path for persistent storage for thumbnails.
---@param path string Path to the directory
function swayimg.gallery.set_pstore_path(path) end
