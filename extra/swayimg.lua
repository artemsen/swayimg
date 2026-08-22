---@meta swayimg

---Application mode.
---@alias appmode_t
---| "viewer"    # Image viewer mode
---| "slideshow" # Slide show mode
---| "gallery"   # Gallery mode

---ARGB color in hex format: AARRGGBB, for example `0xff00aa99`.
---@alias color_t integer

---Image list order
---@alias order_t
---| "none"    # Unsorted (system-dependent)
---| "alpha"   # Lexicographic sort: 1,10,2,20,a,b,c
---| "numeric" # Numeric sort: 1,2,3,10,100,a,b,c
---| "mtime"   # Modification time sort
---| "size"    # Size sort
---| "random"  # Random order

---Direction for opening next file in viewer and slideshow modes.
---@alias vdir_t
---| "first"    # First file in image list
---| "last"     # Last file in image list
---| "next"     # Next file
---| "prev"     # Previous file
---| "next_dir" # First file in next directory
---| "prev_dir" # Last file in previous directory
---| "random"   # Random file in image list

---Fixed scale for images in viewer and slideshow modes.
---@alias fixed_scale_t
---| "optimal" # 100% or less to fit to window
---| "width"   # Fit image width to window width
---| "height"  # Fit image height to window height
---| "fit"     # Fit to window
---| "fill"    # Crop image to fill the window
---| "real"    # Real size (100%)
---| "keep"    # Keep the same scale as for previously viewed image

---Fixed position for images in viewer and slideshow modes.
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

---Fixed rotation angles for images in viewer and slideshow modes.
---@alias rotation_t
---| 90  # 90 degrees
---| 180 # 180 degrees
---| 270 # 270 degrees

---Fixed rotation angles for images in viewer and slideshow modes.
---@alias bkgmode_t
---| "extend" # Fill window with the current image and blur it
---| "mirror" # Fill window with the mirrored current image and blur it
---| "auto"   # Fill the window background in `extend` or `mirror` mode depending on the image aspect ratio

---Direction for selecting next file in gallery mode.
---@alias gdir_t
---| "first"  # Select first thumbnail in image list
---| "last"   # Select last thumbnail in image list
---| "up"     # Select the thumbnail above the current one
---| "down"   # Select the thumbnail below the current one
---| "left"   # Select the thumbnail to the left of the current one
---| "right"  # Select the thumbnail to the right of the current one
---| "pgup"   # Select the thumbnail on the previous page
---| "pgdown" # Select the thumbnail on the next page

---Aspect ratio used for thumbnails in gallery mode.
---@alias aspect_t
---| "fit"  # Fit image into a square thumbnail
---| "fill" # Fill square thumbnail with the image
---| "keep" # Adjust thumbnail size to the aspect ratio of the image

---Position of text block.
---@alias block_position_t
---| "topleft"      # Top left corner of the window
---| "topright"     # Top right corner of the window
---| "bottomleft"   # Bottom left corner of the window
---| "bottomright"  # Bottom right corner of the window

---Mouse buttons.
---@alias mbutton_t
---| "MouseLeft"    # Left mouse button
---| "MouseRight"   # Right mouse button
---| "MouseMiddle"  # Middle mouse button
---| "MouseSide"    # Side mouse button
---| "MouseExtra"   # Extra mouse button
---| "ScrollUp"     # Scroll up
---| "ScrollDown"   # Scroll down
---| "ScrollLeft"   # Scroll left
---| "ScrollRight"  # Scroll right

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
---* `{meta.*}`: Image meta info: EXIF, tags etc. List of available EXIF tags
---  can be found at [Exiv2 website](https://exiv2.org/tags.html) or printed
---  using utility exiv2: `exiv2 -pa photo.jpg`
---
---To print `{` character escape it with `{{`.
---
---The template string may contain a tab character to separate key/value pairs.
---In this case, the text block will be aligned with the longest key.
---If the value cannot be output (for example, the specified EXIF tag is
---missing), then the entire string including the key is ignored upon printing.
---
---Example: `Path to image:\t{path}`
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

---General functionality.
---@class swayimg
---
---Application Id.
---Since 5.5.
---This field can be set only at program startup.
---@field appid string
---
---Application mode.
---Since 5.5.
---Setting this field changes the current mode (viewer/slideshow/gallery).
---@field mode appmode_t
---
---Full screen mode.
---Since 5.5.
---@field fullscreen boolean
---
---Mouse button used for drag-and-drop image file to external apps.
---Since 5.5.
---Write-only field which can be set at startup.
---@field dnd_button mbutton_t
---
---Window overlay mode.
---Since 5.5.
---Write-only field which can be set at startup.
---Create a floating window with the same coordinates and size as the currently focused window.
---Applicable only in Wayland, the corresponding protocol must be supported by the composer.
---By default enabled in Sway and disabled in other compositors.
---@field overlay boolean
---
---Window decoration (title, border, buttons).
---Since 5.5.
---Write-only field which can be set at startup.
---Applicable only in Wayland, the corresponding protocol must be supported by the composer.
---By default disabled in Sway and enabled in other compositors.
---@field decoration boolean
---
---Anti-aliasing mode.
---Since 5.5.
---@field antialiasing boolean
---
---Automatic orientation based on EXIF.
---Since 5.5.
---Write-only field.
---@field exif_orientation boolean
---
---Custom image format parameters.
---Since 5.6.
---Write-only field.
---Supported parameters:
---* `raw`:
---  * `camera_wb`: Fix colors using white balance from camera
---* `ttf`:
---  * `text`: Text to render
---  * `color`: Text color
---  * `background`: Background color
---@field format_params table
---
---Window title.
---Since 5.5.
---Write-only field.
---@field title string
---
swayimg = {}

---Exit from application.
---Since 5.0.
---@param code? integer Program exit code, `0` by default
function swayimg.exit(code) end

---Get application window size.
---Since 5.0.
---@return { width: integer, height: integer } # Window size in pixels
function swayimg.get_window_size() end

---Set application window size.
---Since 5.0.
---@param width integer Width of the window in pixels
---@param height integer Height of the window in pixels
function swayimg.set_window_size(width, height) end

---Set a callback function called when main window is resized.
---Since 5.0.
---@param fn function|nil Window resize notification handler
function swayimg.on_window_resize(fn) end

---Get mouse pointer coordinates.
---Since 5.0.
---@return { x :integer, y: integer } # Coordinates of the mouse pointer
function swayimg.get_mouse_pos() end

---Set a callback function called when all subsystems have been initialized.
---Since 5.0.
---@param fn function Initialization completion notification handler
function swayimg.on_initialized(fn) end

---Set a callback function called after the window is drawn.
---Since 5.5.
---@param fn function|nil Function to execute
function swayimg.on_redrawn(fn) end

---Execute deferred procedure.
---Since 5.5.
---@param seconds number Delay in seconds (can be fractional)
---@param fn function Function to execute
function swayimg.defer(seconds, fn) end

--------------------------------------------------------------------------------

---Image list.
---@class swayimg.imagelist
---
---Sort order of the image list.
---Since 5.5.
---@field order order_t
---
---Reverse the image list order.
---Since 5.5.
---@field reverse boolean
---
---Recursive directory reading.
---Since 5.5.
---@field recursive boolean
---
---Adding adjacent files from the same directory.
---Since 5.5.
---@field adjacent boolean
---
---File system monitoring.
---Since 5.5.
---@field fsmon boolean
---
---Total number of entries in the image list.
---Since 5.5.
---Read-only field.
---@field size integer
---
swayimg.imagelist = {}

---Add entries to the image list.
---Since 5.0.
---@param paths string|string[] Paths to add
function swayimg.imagelist.add(paths) end

---Remove specified entries from the image list.
---Since 5.0.
---@param paths string|string[] Paths to remove
function swayimg.imagelist.remove(paths) end

---Clear the image list.
---Since 5.3.
function swayimg.imagelist.clear() end

---Get list of all entries in the image list.
---Since 5.0.
---@return swayimg.entry[] # Array with all file entries
function swayimg.imagelist.get() end

--------------------------------------------------------------------------------

---Text overlay layer.
---@class swayimg.text
---
---Text overlay state.
---Since 5.5.
---@field visible boolean
---
---Timeout in seconds after which the entire text layer will be hidden.
---Since 5.5.
---Write-only field.
---@field timeout number
---
---Timeout in seconds after which the status message will be hidden.
---Since 5.5.
---Write-only field.
---@field status_timeout number
---
---Font name.
---Since 5.5.
---Write-only field.
---@field font string
---
---Font size in pixels.
---Since 5.5.
---Write-only field.
---@field size integer
---
---Line spacing in pixels.
---Since 5.5.
---Write-only field.
---@field spacing integer
---
---Padding from the window edges in pixels.
---Since 5.5.
---This is write-only field.
---@field padding integer
---
---Foreground text color.
---Since 5.5.
---Write-only field.
---@field color color_t
---
---Background text color.
---Since 5.5.
---Write-only field.
---@field background color_t
---
---Shadow text color.
---Since 5.5.
---Write-only field.
---Setting alpha channel to `0` disables shadows.
---@field shadow color_t
---
---Status message.
---Since 5.5.
---Write-only field.
---Multi-line text should be separated by new line character `\n`.
---@field status string
---
swayimg.text = {}

--------------------------------------------------------------------------------

---Base application mode.
---@class swayimg_appmode
---
---Mark icon color.
---Since 5.5.
---Write-only field.
---@field mark_color color_t
---
---Pinch gesture factor.
---Since 5.5.
---Write-only field.
---@field pinch_factor number
---
local swayimg_appmode = {}

---Set, clear or toggle mark for currently viewed/selected image.
---Since 5.0.
---@param state? boolean Mark state to set, toggle if the state is not specified
function swayimg_appmode.mark_image(state) end

---Remove all existing key/mouse/signal bindings.
---Since 5.0.
function swayimg_appmode.bind_reset() end

---Bind the key press event to a handler.
---Since 5.0.
---@param key string|string[] Key description, for example `Ctrl-a`
---@param fn function Key press handler
function swayimg_appmode.on_key(key, fn) end

---Bind the mouse button press event to a handler.
---Since 5.0.
---@param button mbutton_t Button description, for example `Ctrl-Alt-MouseRight`
---@param fn function Button press handler
function swayimg_appmode.on_mouse(button, fn) end

---Bind the signal event to a handler.
---Since 5.0.
---@param signal string Signal name (`USR1` or `USR2`)
---@param fn function Signal handler
function swayimg_appmode.on_signal(signal, fn) end

---Set a callback function called when a new image is opened/selected.
---Since 5.0.
---@param fn function|nil Handler for notifications about changing the current image
function swayimg_appmode.on_image_change(fn) end

---Set text layer scheme.
---Since 5.0.
---@param pos block_position_t Text block position
---@param scheme text_template_t[] Array of line templates with overlay scheme
function swayimg_appmode.set_text(pos, scheme) end

--------------------------------------------------------------------------------

---Viewer mode.
---@class swayimg.viewer : swayimg_appmode
---
---Automatic image centering.
---Since 5.5.
---Write-only field.
---@field autocenter boolean
---
---Image list loop mode.
---Since 5.5.
---Write-only field.
---@field loop boolean
---
---Default image scale for newly opened images.
---Since 5.5.
---Write-only field.
---@field default_scale number|fixed_scale_t
---
---Default image position for newly opened images.
---Since 5.5.
---Write-only field.
---@field default_position fixed_position_t
---
---Absolute scale value (1.0 = 100%).
---Since 5.5.
---@field scale number
---
---Stop/resume and get animation status.
---Since 5.5.
---@field animation boolean
---
---Currently displayed frame number.
---Since 5.5.
---Setting this field stops animation.
---@field frame integer
---
---Mouse button used for drag image around the window.
---Since 5.5.
---Write-only field.
---@field drag_button mbutton_t
---
---Max number of images to preload in background thread.
---Since 5.5.
---Write-only field.
---@field preload integer
---
---Max number of previously viewed images stored in the cache.
---Since 5.5.
---Write-only field.
---@field history integer
---
swayimg.viewer = {}

---Open the next file in the specified direction.
---Since 5.0.
---
---WARNING: This function is deprecated, use `swayimg.viewer.open` instead.
---@see swayimg.viewer.open
---@deprecated
---@param dir vdir_t Next file direction
function swayimg.viewer.switch_image(dir) end

---Open the next file in the specified direction.
---Since 5.5.
---@param dir vdir_t Next file direction
---@return boolean # True if next file was opened
function swayimg.viewer.open(dir) end

---Open the file at the specified path.
---Since 5.5.
---
---This function adds a file to the image list and then opens it in the viewer.
---@param path string Path to the file
---@return boolean # True if file was opened
function swayimg.viewer.open_path(path) end

---Get information about currently displayed image.
---Since 5.0.
---@return swayimg.image|nil # Currently displayed image
function swayimg.viewer.get_image() end

---Reload current image.
---Since 5.1.
function swayimg.viewer.reload() end

---Set absolute image scale.
---Since 5.0.
---@param scale number Absolute value (1.0 = 100%)
---@param x? integer X coordinate of center point, empty for window center
---@param y? integer Y coordinate of center point, empty for window center
function swayimg.viewer.set_abs_scale(scale, x, y) end

---Set fixed scale for currently displayed image.
---Since 5.0.
---@param scale fixed_scale_t Fixed scale name
function swayimg.viewer.set_fix_scale(scale) end

---Reset position and scale to default values.
---Since 5.0.
---@see swayimg.viewer.set_default_scale
---@see swayimg.viewer.set_default_position
function swayimg.viewer.reset() end

---Get image position.
---Since 5.0.
---@return { x :integer, y: integer } # Image coordinates on the window
function swayimg.viewer.get_position() end

---Set absolute image position.
---Since 5.0.
---@param x integer Horizontal image position on the window
---@param y integer Vertical image position on the window
function swayimg.viewer.set_abs_position(x, y) end

---Set fixed image position.
---Since 5.0.
---@param pos fixed_position_t Fixed image position
function swayimg.viewer.set_fix_position(pos) end

---Flip image vertically.
---Since 5.0.
function swayimg.viewer.flip_vertical() end

---Flip image horizontally.
---Since 5.0.
function swayimg.viewer.flip_horizontal() end

---Rotate image.
---Since 5.0.
---@param angle rotation_t Rotation angle
function swayimg.viewer.rotate(angle) end

---Export currently displayed frame to PNG file.
---Since 5.0.
---@param path string Path to the file
function swayimg.viewer.export(path) end

---Add/replace/remove meta info for currently displayed image.
---Since 5.0.
---@param key string Meta key name
---@param value string Meta value, empty value to remove the record
function swayimg.viewer.set_meta(key, value) end

---Set window background color or extension mode.
---Since 5.0.
---@param bkg color_t|bkgmode_t Solid color or one of the predefined mode
function swayimg.viewer.set_window_background(bkg) end

---Set background color for transparent images.
---Since 5.0.
---
---This disables chessboard drawing.
---@param color color_t Background color
function swayimg.viewer.set_image_background(color) end

---Set parameters for chessboard used as background for transparent images.
---Since 5.0.
---
---This enables the chessboard if this feature was previously disabled.
---@param size integer Size of single grid cell in pixels
---@param color1 color_t First color
---@param color2 color_t Second color
function swayimg.viewer.set_image_chessboard(size, color1, color2) end

--------------------------------------------------------------------------------

---Slide show mode.
---@class swayimg.slideshow : swayimg.viewer
---
---Timeout in seconds after which next image should be opened.
---Since 5.5.
---@field timeout number
---
swayimg.slideshow = {}

--------------------------------------------------------------------------------

---Gallery mode.
---@class swayimg.gallery : swayimg_appmode
---
---Thumbnail aspect ratio.
---Since 5.5.
---Write-only field.
---@field aspect aspect_t
---
---Thumbnail size in pixels.
---Since 5.5.
---@field thumb_size integer
---
---Padding size in pixels between thumbnails.
---Since 5.5.
---Write-only field.
---@field padding_size integer
---
---Border size in pixels for currently selected thumbnail.
---Since 5.5.
---Write-only field.
---@field border_size integer
---
---Scale factor for currently selected thumbnail.
---Since 5.5.
---Write-only field.
---@field selected_scale number
---
---Set window background color.
---Since 5.5.
---Write-only field.
---@field window_color color_t Background color
---
---Background color for unselected thumbnails.
---Since 5.5.
---Write-only field.
---@field unselected_color color_t
---
---Background color for currently selected thumbnail.
---Since 5.5.
---Write-only field.
---@field selected_color color_t
---
---Border color for currently selected thumbnail.
---Since 5.5.
---Write-only field.
---@field border_color color_t
---
---Change current thumbnail on mouse hover.
---Since 5.5.
---Write-only field.
---@field hover boolean
---
---Use persistent storage for thumbnails.
---Since 5.5.
---Write-only field.
---@field pstore boolean
---
---Path for thumbnails persistent storage.
---Since 5.5.
---Write-only field.
---@field pstore_path string
---
---Preload invisible thumbnails.
---Since 5.5.
---Write-only field.
---The program preloads thumbnails into the cache up to the amount specified in the `cache` field.
---@field preload boolean
---
---Max number of invisible thumbnails stored in memory cache.
---Since 5.5.
---Write-only field.
---@field cache integer
---
---Use embedded thumbnails.
---Since 5.5.
---Currently only applicable to RAW images.
---@field embedded_thumb boolean
---
swayimg.gallery = {}

---Select the next thumbnail from the gallery.
---Since 5.0.
---
---WARNING: This function is deprecated, use `swayimg.gallery.select` instead.
---@see swayimg.gallery.select_next
---@deprecated
---@param dir gdir_t Next thumbnail direction
function swayimg.gallery.switch_image(dir) end

---Select the next thumbnail from the gallery.
---Since 5.5.
---@param dir gdir_t Next thumbnail direction
---@return boolean # True if selection was changed
function swayimg.gallery.select(dir) end

---Select the thumbnail at specified position.
---Since 5.5.
---@param x integer X coordinate of the thumbnail
---@param y integer Y coordinate of the thumbnail
---@return boolean # True if selection was changed
function swayimg.gallery.select_at(x, y) end

---Select the thumbnail by image path.
---Since 5.5.
---@param path string Path to the image
---@return boolean # True if selection was changed
function swayimg.gallery.select_path(path) end

---Reload thumbnails.
---Since 5.3.
function swayimg.gallery.reload() end

---Get information about currently selected image entry.
---Since 5.0.
---@return swayimg.entry|nil # Currently selected image entry
function swayimg.gallery.get_image() end
