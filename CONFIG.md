# Swayimg configuration

The Swayimg configuration file is a Lua script.

Please refer to the official Lua documentation for information about the file
format.

The source file [swayimg.lua](extra/swayimg.lua) contains a description of Lua
bindings and can be used for the LSP server, it is located in `/usr/share/swayimg/swayimg.lua`
after installing the program.

The program searches for the config file in the following locations:
1. `$XDG_CONFIG_HOME/swayimg/init.lua`
2. `$HOME/.config/swayimg/init.lua`
3. `$XDG_CONFIG_DIRS/swayimg/init.lua`
4. `/etc/xdg/swayimg/init.lua`

Config example:
```lua
swayimg.text.set_size(32)
swayimg.text.set_foreground(0xffff0000)

swayimg.viewer.set_default_scale("fill")

swayimg.gallery.on_key("Delete", function()
  local image = swayimg.gallery.get_image()
  os.remove(image.path)
end)
```

A more detailed example can be found on the [project website](extra/example.lua)
or in the file `/usr/share/swayimg/example.lua` after installing the program.

## List of available functions

* General functionality
  * [swayimg.exit](#swayimgexit): Exit from application
  * [swayimg.set_mode](#swayimgset_mode): Switch to specified mode
  * [swayimg.get_mode](#swayimgget_mode): Get current mode
  * [swayimg.set_title](#swayimgset_title): Set title for main application window
  * [swayimg.get_window_size](#swayimgget_window_size): Get application window size
  * [swayimg.set_window_size](#swayimgset_window_size): Set application window size
  * [swayimg.on_window_resize](#swayimgon_window_resize): Add a callback function called when main window is resized
  * [swayimg.get_mouse_pos](#swayimgget_mouse_pos): Get mouse pointer coordinates
  * [swayimg.toggle_fullscreen](#swayimgtoggle_fullscreen): Toggle full screen mode
  * [swayimg.on_initialized](#swayimgon_initialized): Add a callback function called when all subsystems have been initialized
  * [swayimg.enable_antialiasing](#swayimgenable_antialiasing): Enable or disable antialiasing
  * [swayimg.enable_decoration](#swayimgenable_decoration): Enable or disable window decoration (title, border, buttons)
  * [swayimg.enable_overlay](#swayimgenable_overlay): Enable or disable window overlay mode
  * [swayimg.set_dnd_button](#swayimgset_dnd_button): Set mouse button used for drag-and-drop image file to external apps
* Image list
  * [swayimg.imagelist.size](#swayimgimagelistsize): Get number of entries in the image list
  * [swayimg.imagelist.get](#swayimgimagelistget): Get list of all entries in the image list
  * [swayimg.imagelist.add](#swayimgimagelistadd): Add entry to the image list
  * [swayimg.imagelist.remove](#swayimgimagelistremove): Remove entry from the image list
  * [swayimg.imagelist.set_order](#swayimgimagelistset_order): Set sort order of the image list
  * [swayimg.imagelist.enable_reverse](#swayimgimagelistenable_reverse): Enable or disable reverse order
  * [swayimg.imagelist.enable_recursive](#swayimgimagelistenable_recursive): Enable or disable recursive directory reading
  * [swayimg.imagelist.enable_adjacent](#swayimgimagelistenable_adjacent): Enable or disable adding adjacent files from the same directory
  * [swayimg.imagelist.enable_fsmon](#swayimgimagelistenable_fsmon): Enable or disable file system monitoring
* Text layer
  * [swayimg.text.show](#swayimgtextshow): Force show the text layer
  * [swayimg.text.hide](#swayimgtexthide): Hide the text layer
  * [swayimg.text.visible](#swayimgtextvisible): Check if text layer is visible
  * [swayimg.text.set_font](#swayimgtextset_font): Set font face
  * [swayimg.text.set_size](#swayimgtextset_size): Set font size
  * [swayimg.text.set_spacing](#swayimgtextset_spacing): Set line spacing
  * [swayimg.text.set_padding](#swayimgtextset_padding): Set the padding from the window edges
  * [swayimg.text.set_foreground](#swayimgtextset_foreground): Set foreground text color
  * [swayimg.text.set_background](#swayimgtextset_background): Set background text color
  * [swayimg.text.set_shadow](#swayimgtextset_shadow): Set shadow text color
  * [swayimg.text.set_timeout](#swayimgtextset_timeout): Set a timeout after which the entire text layer will be hidden
  * [swayimg.text.set_status_timeout](#swayimgtextset_status_timeout): Set a timeout after which the status message will be hidden
  * [swayimg.text.set_status](#swayimgtextset_status): Show status message
* Viewer mode
  * [swayimg.viewer.switch_image](#swayimgviewerswitch_image): Open the next file in the specified direction
  * [swayimg.viewer.get_image](#swayimgviewerget_image): Get information about currently displayed image
  * [swayimg.viewer.reset](#swayimgviewerreset): Reset position and scale to default values
  * [swayimg.viewer.get_scale](#swayimgviewerget_scale): Get current image scale
  * [swayimg.viewer.set_abs_scale](#swayimgviewerset_abs_scale): Set absolute image scale
  * [swayimg.viewer.set_fix_scale](#swayimgviewerset_fix_scale): Set fixed scale for currently displayed image
  * [swayimg.viewer.set_default_scale](#swayimgviewerset_default_scale): Set default image scale for newly opened images
  * [swayimg.viewer.get_position](#swayimgviewerget_position): Get image position
  * [swayimg.viewer.set_abs_position](#swayimgviewerset_abs_position): Set absolute image position
  * [swayimg.viewer.set_fix_position](#swayimgviewerset_fix_position): Set fixed image position
  * [swayimg.viewer.set_default_position](#swayimgviewerset_default_position): Set default image position for newly opened images
  * [swayimg.viewer.next_frame](#swayimgviewernext_frame): Show next frame from multi-frame image (animation)
  * [swayimg.viewer.prev_frame](#swayimgviewerprev_frame): Show previous frame from multi-frame image (animation)
  * [swayimg.viewer.animation_stop](#swayimgvieweranimation_stop): Stop animation
  * [swayimg.viewer.animation_resume](#swayimgvieweranimation_resume): Resume animation
  * [swayimg.viewer.flip_vertical](#swayimgviewerflip_vertical): Flip image vertically
  * [swayimg.viewer.flip_horizontal](#swayimgviewerflip_horizontal): Flip image horizontally
  * [swayimg.viewer.rotate](#swayimgviewerrotate): Rotate image
  * [swayimg.viewer.export](#swayimgviewerexport): Export currently displayed frame to PNG file
  * [swayimg.viewer.set_meta](#swayimgviewerset_meta): Add/replace/remove meta info for currently displayed image
  * [swayimg.viewer.set_drag_button](#swayimgviewerset_drag_button): Set the mouse button used to drag the image around the window
  * [swayimg.viewer.set_window_background](#swayimgviewerset_window_background): Set window background color and mode
  * [swayimg.viewer.set_image_background](#swayimgviewerset_image_background): Set background color for transparent images
  * [swayimg.viewer.set_image_chessboard](#swayimgviewerset_image_chessboard): Set parameters for chessboard used as background for transparent images
  * [swayimg.viewer.enable_centering](#swayimgviewerenable_centering): Enable or disable automatic image centering
  * [swayimg.viewer.enable_loop](#swayimgviewerenable_loop): Enable or disable image list loop mode
  * [swayimg.viewer.limit_preload](#swayimgviewerlimit_preload): Set max number of images to preload in background thread
  * [swayimg.viewer.limit_history](#swayimgviewerlimit_history): Set max number of previously viewed images stored in the cache
  * [swayimg.viewer.mark_image](#swayimgviewermark_image): Set, clear or toggle mark for currently viewed/selected image
  * [swayimg.viewer.set_mark_color](#swayimgviewerset_mark_color): Set mark icon color
  * [swayimg.viewer.bind_reset](#swayimgviewerbind_reset): Remove all existing key/mouse/signal bindings
  * [swayimg.viewer.on_key](#swayimgvieweron_key): Bind the key press event to a handler
  * [swayimg.viewer.on_mouse](#swayimgvieweron_mouse): Bind the mouse button press event to a handler
  * [swayimg.viewer.on_signal](#swayimgvieweron_signal): Bind the signal event to a handler
  * [swayimg.viewer.on_image_change](#swayimgvieweron_image_change): Add a callback function called when a new image is opened/selected
  * [swayimg.viewer.set_text](#swayimgviewerset_text): Set text layer scheme
* Slide show mode
  * [swayimg.slideshow.set_timeout](#swayimgslideshowset_timeout): Set a timeout after which next image should be opened
  * [swayimg.slideshow.switch_image](#swayimgslideshowswitch_image): Open the next file in the specified direction
  * [swayimg.slideshow.get_image](#swayimgslideshowget_image): Get information about currently displayed image
  * [swayimg.slideshow.reset](#swayimgslideshowreset): Reset position and scale to default values
  * [swayimg.slideshow.get_scale](#swayimgslideshowget_scale): Get current image scale
  * [swayimg.slideshow.set_abs_scale](#swayimgslideshowset_abs_scale): Set absolute image scale
  * [swayimg.slideshow.set_fix_scale](#swayimgslideshowset_fix_scale): Set fixed scale for currently displayed image
  * [swayimg.slideshow.set_default_scale](#swayimgslideshowset_default_scale): Set default image scale for newly opened images
  * [swayimg.slideshow.get_position](#swayimgslideshowget_position): Get image position
  * [swayimg.slideshow.set_abs_position](#swayimgslideshowset_abs_position): Set absolute image position
  * [swayimg.slideshow.set_fix_position](#swayimgslideshowset_fix_position): Set fixed image position
  * [swayimg.slideshow.set_default_position](#swayimgslideshowset_default_position): Set default image position for newly opened images
  * [swayimg.slideshow.next_frame](#swayimgslideshownext_frame): Show next frame from multi-frame image (animation)
  * [swayimg.slideshow.prev_frame](#swayimgslideshowprev_frame): Show previous frame from multi-frame image (animation)
  * [swayimg.slideshow.animation_stop](#swayimgslideshowanimation_stop): Stop animation
  * [swayimg.slideshow.animation_resume](#swayimgslideshowanimation_resume): Resume animation
  * [swayimg.slideshow.flip_vertical](#swayimgslideshowflip_vertical): Flip image vertically
  * [swayimg.slideshow.flip_horizontal](#swayimgslideshowflip_horizontal): Flip image horizontally
  * [swayimg.slideshow.rotate](#swayimgslideshowrotate): Rotate image
  * [swayimg.slideshow.export](#swayimgslideshowexport): Export currently displayed frame to PNG file
  * [swayimg.slideshow.set_meta](#swayimgslideshowset_meta): Add/replace/remove meta info for currently displayed image
  * [swayimg.slideshow.set_drag_button](#swayimgslideshowset_drag_button): Set the mouse button used to drag the image around the window
  * [swayimg.slideshow.set_window_background](#swayimgslideshowset_window_background): Set window background color and mode
  * [swayimg.slideshow.set_image_background](#swayimgslideshowset_image_background): Set background color for transparent images
  * [swayimg.slideshow.set_image_chessboard](#swayimgslideshowset_image_chessboard): Set parameters for chessboard used as background for transparent images
  * [swayimg.slideshow.enable_centering](#swayimgslideshowenable_centering): Enable or disable automatic image centering
  * [swayimg.slideshow.enable_loop](#swayimgslideshowenable_loop): Enable or disable image list loop mode
  * [swayimg.slideshow.limit_preload](#swayimgslideshowlimit_preload): Set max number of images to preload in background thread
  * [swayimg.slideshow.limit_history](#swayimgslideshowlimit_history): Set max number of previously viewed images stored in the cache
  * [swayimg.slideshow.mark_image](#swayimgslideshowmark_image): Set, clear or toggle mark for currently viewed/selected image
  * [swayimg.slideshow.set_mark_color](#swayimgslideshowset_mark_color): Set mark icon color
  * [swayimg.slideshow.bind_reset](#swayimgslideshowbind_reset): Remove all existing key/mouse/signal bindings
  * [swayimg.slideshow.on_key](#swayimgslideshowon_key): Bind the key press event to a handler
  * [swayimg.slideshow.on_mouse](#swayimgslideshowon_mouse): Bind the mouse button press event to a handler
  * [swayimg.slideshow.on_signal](#swayimgslideshowon_signal): Bind the signal event to a handler
  * [swayimg.slideshow.on_image_change](#swayimgslideshowon_image_change): Add a callback function called when a new image is opened/selected
  * [swayimg.slideshow.set_text](#swayimgslideshowset_text): Set text layer scheme
* Gallery mode
  * [swayimg.gallery.switch_image](#swayimggalleryswitch_image): Select the next thumbnail from the gallery
  * [swayimg.gallery.get_image](#swayimggalleryget_image): Get information about currently selected image entry
  * [swayimg.gallery.set_aspect](#swayimggalleryset_aspect): Set thumbnail aspect ratio
  * [swayimg.gallery.get_thumb_size](#swayimggalleryget_thumb_size): Get thumbnail size
  * [swayimg.gallery.set_thumb_size](#swayimggalleryset_thumb_size): Set thumbnail size
  * [swayimg.gallery.set_padding_size](#swayimggalleryset_padding_size): Set the padding size between thumbnails
  * [swayimg.gallery.set_border_size](#swayimggalleryset_border_size): Set the border size for currently selected thumbnail
  * [swayimg.gallery.set_border_color](#swayimggalleryset_border_color): Set border color for currently selected thumbnail
  * [swayimg.gallery.set_selected_scale](#swayimggalleryset_selected_scale): Set the scale factor for currently selected thumbnail
  * [swayimg.gallery.set_selected_color](#swayimggalleryset_selected_color): Set background color for currently selected thumbnail
  * [swayimg.gallery.set_unselected_color](#swayimggalleryset_unselected_color): Set background color for unselected thumbnails
  * [swayimg.gallery.set_window_color](#swayimggalleryset_window_color): Set window background color
  * [swayimg.gallery.limit_cache](#swayimggallerylimit_cache): Set max number of thumbnails stored in memory cache
  * [swayimg.gallery.enable_preload](#swayimggalleryenable_preload): Enable or disable preloading invisible thumbnails
  * [swayimg.gallery.enable_pstore](#swayimggalleryenable_pstore): Enable or disable persistent storage for thumbnails
  * [swayimg.gallery.set_pstore_path](#swayimggalleryset_pstore_path): Set custom path for persistent storage for thumbnails
  * [swayimg.gallery.mark_image](#swayimggallerymark_image): Set, clear or toggle mark for currently viewed/selected image
  * [swayimg.gallery.set_mark_color](#swayimggalleryset_mark_color): Set mark icon color
  * [swayimg.gallery.bind_reset](#swayimggallerybind_reset): Remove all existing key/mouse/signal bindings
  * [swayimg.gallery.on_key](#swayimggalleryon_key): Bind the key press event to a handler
  * [swayimg.gallery.on_mouse](#swayimggalleryon_mouse): Bind the mouse button press event to a handler
  * [swayimg.gallery.on_signal](#swayimggalleryon_signal): Bind the signal event to a handler
  * [swayimg.gallery.on_image_change](#swayimggalleryon_image_change): Add a callback function called when a new image is opened/selected
  * [swayimg.gallery.set_text](#swayimggalleryset_text): Set text layer scheme

## General functionality

### swayimg.exit

```lua
swayimg.exit(code?: integer)
```

Exit from application.

@_param_ `code` - Program exit code, `0` by default

### swayimg.set_mode

```lua
swayimg.set_mode(mode: appmode_t)
```

Switch to specified mode.

@_param_ `mode` - Mode to activate

`appmode_t`, Application mode:
* `"viewer"`: Image viewer mode
* `"slideshow"`: Slide show mode
* `"gallery"`: Gallery mode

### swayimg.get_mode

```lua
swayimg.get_mode() -> appmode_t
```

Get current mode.

@_return_ - Currently active mode

`appmode_t`, Application mode:
* `"viewer"`: Image viewer mode
* `"slideshow"`: Slide show mode
* `"gallery"`: Gallery mode

### swayimg.set_title

```lua
swayimg.set_title(title: string)
```

Set title for main application window.

@_param_ `title` - Window title text

### swayimg.get_window_size

```lua
swayimg.get_window_size() -> { width: integer, height: integer }
```

Get application window size.

@_return_ - Window size in pixels

### swayimg.set_window_size

```lua
swayimg.set_window_size(width: integer, height: integer)
```

Set application window size.

@_param_ `width` - Width of the window in pixels

@_param_ `height` - Height of the window in pixels

### swayimg.on_window_resize

```lua
swayimg.on_window_resize(fn: function)
```

Add a callback function called when main window is resized.

@_param_ `fn` - Window resize notification handler

### swayimg.get_mouse_pos

```lua
swayimg.get_mouse_pos() -> { x :integer, y: integer }
```

Get mouse pointer coordinates.

@_return_ - Coordinates of the mouse pointer

### swayimg.toggle_fullscreen

```lua
swayimg.toggle_fullscreen() -> boolean
```

Toggle full screen mode.

@_return_ - True if full screen is enabled

### swayimg.on_initialized

```lua
swayimg.on_initialized(fn: function)
```

Add a callback function called when all subsystems have been initialized.

@_param_ `fn` - Initialization completion notification handler

### swayimg.enable_antialiasing

```lua
swayimg.enable_antialiasing(enable: boolean)
```

Enable or disable antialiasing.

@_param_ `enable` - Enable/disable antialiasing

### swayimg.enable_decoration

```lua
swayimg.enable_decoration(enable: boolean)
```

Enable or disable window decoration (title, border, buttons).

This function can only be called at program startup.
Applicable only in Wayland, the corresponding protocol must be supported by
the composer.
By default disabled in Sway and enabled in other compositors.

@_param_ `enable` - Enable/disable window decoration

### swayimg.enable_overlay

```lua
swayimg.enable_overlay(enable: boolean)
```

Enable or disable window overlay mode.

Create a floating window with the same coordinates and size as the currently
focused window.
This function can only be called at program startup.
Applicable only in Sway and Hyprland compositors.
By default enabled in Sway and disabled in other compositors.

@_param_ `enable` - Enable/disable overlay mode

### swayimg.set_dnd_button

```lua
swayimg.set_dnd_button(button: string)
```

Set mouse button used for drag-and-drop image file to external apps.

This function can only be called at program startup.

@_param_ `button` - Mouse button used for drag-n-drop, for example `MouseRight`

## Image list

### swayimg.imagelist.size

```lua
swayimg.imagelist.size() -> integer
```

Get number of entries in the image list.

@_return_ - Size of the image list

### swayimg.imagelist.get

```lua
swayimg.imagelist.get() -> swayimg.entry[]
```

Get list of all entries in the image list.

@_return_ - Array with all file entries

### swayimg.imagelist.add

```lua
swayimg.imagelist.add(path: string)
```

Add entry to the image list.

@_param_ `path` - Path to add

### swayimg.imagelist.remove

```lua
swayimg.imagelist.remove(path: string)
```

Remove entry from the image list.

@_param_ `path` - Path to remove

### swayimg.imagelist.set_order

```lua
swayimg.imagelist.set_order(order: order_t)
```

Set sort order of the image list.

@_param_ `order` - List order

`order_t`, Image list order:
* `"none"`: Unsorted (system-dependent)
* `"alpha"`: Lexicographic sort: 1,10,2,20,a,b,c
* `"numeric"`: Numeric sort: 1,2,3,10,100,a,b,c
* `"mtime"`: Modification time sort
* `"size"`: Size sort
* `"random"`: Random order

### swayimg.imagelist.enable_reverse

```lua
swayimg.imagelist.enable_reverse(enable: boolean)
```

Enable or disable reverse order.

@_param_ `enable` - Enable/disable reverse order

### swayimg.imagelist.enable_recursive

```lua
swayimg.imagelist.enable_recursive(enable: boolean)
```

Enable or disable recursive directory reading.

@_param_ `enable` - Enable/disable recursive mode

### swayimg.imagelist.enable_adjacent

```lua
swayimg.imagelist.enable_adjacent(enable: boolean)
```

Enable or disable adding adjacent files from the same directory.

This function can only be called at program startup.

@_param_ `enable` - Enable/disable adding adjacent files

### swayimg.imagelist.enable_fsmon

```lua
swayimg.imagelist.enable_fsmon(enable: boolean)
```

Enable or disable file system monitoring.

@_param_ `enable` - Enable/disable FS monitor

## Text layer

### swayimg.text.show

```lua
swayimg.text.show()
```

Force show the text layer.

This function restarts the timer.

See [swayimg.text.set_timer](swayimgtextset_timer)

### swayimg.text.hide

```lua
swayimg.text.hide()
```

Hide the text layer.

### swayimg.text.visible

```lua
swayimg.text.visible() -> boolean
```

Check if text layer is visible.

@_return_ - `true` if text layer is visible

### swayimg.text.set_font

```lua
swayimg.text.set_font(name: string)
```

Set font face.

@_param_ `name` - Font name

### swayimg.text.set_size

```lua
swayimg.text.set_size(size: integer)
```

Set font size.

@_param_ `size` - Font size in pixels

### swayimg.text.set_spacing

```lua
swayimg.text.set_spacing(size: integer)
```

Set line spacing.

@_param_ `size` - Line spacing in pixels, can be negative

### swayimg.text.set_padding

```lua
swayimg.text.set_padding(size: integer)
```

Set the padding from the window edges.

@_param_ `size` - Padding size in pixels

### swayimg.text.set_foreground

```lua
swayimg.text.set_foreground(color: color_t)
```

Set foreground text color.

@_param_ `color` - Foreground text color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.text.set_background

```lua
swayimg.text.set_background(color: color_t)
```

Set background text color.

@_param_ `color` - Background text color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.text.set_shadow

```lua
swayimg.text.set_shadow(color: color_t)
```

Set shadow text color.

Setting alpha channel to `0` disables shadows.

@_param_ `color` - Shadow text color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.text.set_timeout

```lua
swayimg.text.set_timeout(seconds: number)
```

Set a timeout after which the entire text layer will be hidden.

Setting the timeout value to `0` disables the timer and causes the overlay
to be displayed continuously.

@_param_ `seconds` - Timeout in seconds

### swayimg.text.set_status_timeout

```lua
swayimg.text.set_status_timeout(seconds: number)
```

Set a timeout after which the status message will be hidden.

Setting the timeout value to `0` disables the timer and causes the status
message to be displayed continuously.

See [swayimg.text.set_status](swayimgtextset_status)

@_param_ `seconds` - Timeout in seconds

### swayimg.text.set_status

```lua
swayimg.text.set_status(status: string)
```

Show status message.

Multi-line text is separated by `\n`.

See [swayimg.text.set_status_timer](swayimgtextset_status_timer)

@_param_ `status` - Status text to show

## Viewer mode

### swayimg.viewer.switch_image

```lua
swayimg.viewer.switch_image(dir: vdir_t)
```

Open the next file in the specified direction.

@_param_ `dir` - Next file direction

`vdir_t`, Direction for opening next file in viewer and slideshow modes:
* `"first"`: First file in image list
* `"last"`: Last file in image list
* `"next"`: Next file
* `"prev"`: Previous file
* `"next_dir"`: First file in next directory
* `"prev_dir"`: Last file in previous directory
* `"random"`: Random file in image list

### swayimg.viewer.get_image

```lua
swayimg.viewer.get_image() -> swayimg.image
```

Get information about currently displayed image.

@_return_ - Currently displayed image

### swayimg.viewer.reset

```lua
swayimg.viewer.reset()
```

Reset position and scale to default values.

See [swayimg.viewer.set_default_scale](swayimgviewerset_default_scale)

See [swayimg.viewer.set_default_position](swayimgviewerset_default_position)

### swayimg.viewer.get_scale

```lua
swayimg.viewer.get_scale() -> number
```

Get current image scale.

@_return_ - Absolute scale value (1.0 = 100%)

### swayimg.viewer.set_abs_scale

```lua
swayimg.viewer.set_abs_scale(scale: number, x?: integer, y?: integer)
```

Set absolute image scale.

@_param_ `scale` - Absolute value (1.0 = 100%)

@_param_ `x` - X coordinate of center point, empty for window center

@_param_ `y` - Y coordinate of center point, empty for window center

### swayimg.viewer.set_fix_scale

```lua
swayimg.viewer.set_fix_scale(scale: fixed_scale_t)
```

Set fixed scale for currently displayed image.

@_param_ `scale` - Fixed scale name

`fixed_scale_t`, Fixed scale for images in viewer and slideshow modes:
* `"optimal"`: 100% or less to fit to window
* `"width"`: Fit image width to window width
* `"height"`: Fit image height to window height
* `"fit"`: Fit to window
* `"fill"`: Crop image to fill the window
* `"real"`: Real size (100%)
* `"keep"`: Keep the same scale as for previously viewed image

### swayimg.viewer.set_default_scale

```lua
swayimg.viewer.set_default_scale(scale: number|fixed_scale_t)
```

Set default image scale for newly opened images.

@_param_ `scale` - Absolute value (1.0 = 100%) or one the predefined names

`fixed_scale_t`, Fixed scale for images in viewer and slideshow modes:
* `"optimal"`: 100% or less to fit to window
* `"width"`: Fit image width to window width
* `"height"`: Fit image height to window height
* `"fit"`: Fit to window
* `"fill"`: Crop image to fill the window
* `"real"`: Real size (100%)
* `"keep"`: Keep the same scale as for previously viewed image

### swayimg.viewer.get_position

```lua
swayimg.viewer.get_position() -> { x :integer, y: integer }
```

Get image position.

@_return_ - Image coordinates on the window

### swayimg.viewer.set_abs_position

```lua
swayimg.viewer.set_abs_position(x: integer, y: integer)
```

Set absolute image position.

@_param_ `x` - Horizontal image position on the window

@_param_ `y` - Vertical image position on the window

### swayimg.viewer.set_fix_position

```lua
swayimg.viewer.set_fix_position(pos: fixed_position_t)
```

Set fixed image position.

@_param_ `pos` - Fixed image position

`fixed_position_t`, Fixed position for images in viewer and slideshow modes:
* `"center"`: Vertical and horizontal center of the window
* `"topcenter"`: Top (vertical) and center (horizontal) of the window
* `"bottomcenter"`: Bottom (vertical) and center (horizontal) of the window
* `"leftcenter"`: Left (horizontal) and center (vertical) of the window
* `"rightcenter"`: Right (horizontal) and center (vertical) of the window
* `"topleft"`: Top left corner of the window
* `"topright"`: Top right corner of the window
* `"bottomleft"`: Bottom left corner of the window
* `"bottomright"`: Bottom right corner of the window

### swayimg.viewer.set_default_position

```lua
swayimg.viewer.set_default_position(pos: fixed_position_t)
```

Set default image position for newly opened images.

@_param_ `pos` - Fixed image position

`fixed_position_t`, Fixed position for images in viewer and slideshow modes:
* `"center"`: Vertical and horizontal center of the window
* `"topcenter"`: Top (vertical) and center (horizontal) of the window
* `"bottomcenter"`: Bottom (vertical) and center (horizontal) of the window
* `"leftcenter"`: Left (horizontal) and center (vertical) of the window
* `"rightcenter"`: Right (horizontal) and center (vertical) of the window
* `"topleft"`: Top left corner of the window
* `"topright"`: Top right corner of the window
* `"bottomleft"`: Bottom left corner of the window
* `"bottomright"`: Bottom right corner of the window

### swayimg.viewer.next_frame

```lua
swayimg.viewer.next_frame() -> integer
```

Show next frame from multi-frame image (animation).

This function stops the animation.

@_return_ - Index of the currently shown frame

### swayimg.viewer.prev_frame

```lua
swayimg.viewer.prev_frame() -> integer
```

Show previous frame from multi-frame image (animation).

This function stops the animation.

@_return_ - Index of the currently shown frame

### swayimg.viewer.animation_stop

```lua
swayimg.viewer.animation_stop()
```

Stop animation.

### swayimg.viewer.animation_resume

```lua
swayimg.viewer.animation_resume()
```

Resume animation.

### swayimg.viewer.flip_vertical

```lua
swayimg.viewer.flip_vertical()
```

Flip image vertically.

### swayimg.viewer.flip_horizontal

```lua
swayimg.viewer.flip_horizontal()
```

Flip image horizontally.

### swayimg.viewer.rotate

```lua
swayimg.viewer.rotate(angle: rotation_t)
```

Rotate image.

@_param_ `angle` - Rotation angle

`rotation_t`, Fixed rotation angles for images in viewer and slideshow modes:
* `90`: 90 degrees
* `180`: 180 degrees
* `270`: 270 degrees

### swayimg.viewer.export

```lua
swayimg.viewer.export(path: string)
```

Export currently displayed frame to PNG file.

@_param_ `path` - Path to the file

### swayimg.viewer.set_meta

```lua
swayimg.viewer.set_meta(key: string, value: string)
```

Add/replace/remove meta info for currently displayed image.

@_param_ `key` - Meta key name

@_param_ `value` - Meta value, empty value to remove the record

### swayimg.viewer.set_drag_button

```lua
swayimg.viewer.set_drag_button(button: string)
```

Set the mouse button used to drag the image around the window.

@_param_ `button` - Mouse button name, for example `MouseLeft`

### swayimg.viewer.set_window_background

```lua
swayimg.viewer.set_window_background(bkg: color_t|bkgmode_t)
```

Set window background color and mode.

@_param_ `bkg` - Solid color or one of the predefined mode

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

`bkgmode_t`, Fixed rotation angles for images in viewer and slideshow modes:
* `"extend"`: Fill window with the current image and blur it
* `"mirror"`: Fill window with the mirrored current image and blur it
* `"auto"`: Fill the window background in `extend` or `mirror` mode depending on the image aspect ratio

### swayimg.viewer.set_image_background

```lua
swayimg.viewer.set_image_background(color: color_t)
```

Set background color for transparent images.

This disables chessboard drawing.

@_param_ `color` - Background color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.viewer.set_image_chessboard

```lua
swayimg.viewer.set_image_chessboard(size: integer, color1: color_t, color2: color_t)
```

Set parameters for chessboard used as background for transparent images.

This enables the chessboard if this feature was previously disabled.

@_param_ `size` - Size of single grid cell in pixels

@_param_ `color1` - First color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

@_param_ `color2` - Second color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.viewer.enable_centering

```lua
swayimg.viewer.enable_centering(enable: boolean)
```

Enable or disable automatic image centering.

@_param_ `enable` - Enable/disable automatic image centering

### swayimg.viewer.enable_loop

```lua
swayimg.viewer.enable_loop(enable: boolean)
```

Enable or disable image list loop mode.

@_param_ `enable` - Enable/disable flag to set

### swayimg.viewer.limit_preload

```lua
swayimg.viewer.limit_preload(size: integer)
```

Set max number of images to preload in background thread.

@_param_ `size` - Number of images to preload

### swayimg.viewer.limit_history

```lua
swayimg.viewer.limit_history(size: integer)
```

Set max number of previously viewed images stored in the cache.

@_param_ `size` - Number of images to store

### swayimg.viewer.mark_image

```lua
swayimg.viewer.mark_image(state?: boolean)
```

Set, clear or toggle mark for currently viewed/selected image.

@_param_ `state` - Mark state to set, toggle if the state is not specified

### swayimg.viewer.set_mark_color

```lua
swayimg.viewer.set_mark_color(color: color_t)
```

Set mark icon color.

@_param_ `color` - Mark icon color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.viewer.bind_reset

```lua
swayimg.viewer.bind_reset()
```

Remove all existing key/mouse/signal bindings.

### swayimg.viewer.on_key

```lua
swayimg.viewer.on_key(key: string, fn: function)
```

Bind the key press event to a handler.

@_param_ `key` - Key description, for example `Ctrl-a`

@_param_ `fn` - Key press handler

### swayimg.viewer.on_mouse

```lua
swayimg.viewer.on_mouse(button: string, fn: function)
```

Bind the mouse button press event to a handler.

@_param_ `button` - Button description, for example `Ctrl-Alt-MouseRight`

@_param_ `fn` - Button press handler

### swayimg.viewer.on_signal

```lua
swayimg.viewer.on_signal(signal: string, fn: function)
```

Bind the signal event to a handler.

@_param_ `signal` - Signal name (`USR1` or `USR2`)

@_param_ `fn` - Signal handler

### swayimg.viewer.on_image_change

```lua
swayimg.viewer.on_image_change(fn: function)
```

Add a callback function called when a new image is opened/selected.

@_param_ `fn` - Handler for notifications about changing the current image

### swayimg.viewer.set_text

```lua
swayimg.viewer.set_text(pos: block_position_t, scheme: text_template_t[])
```

Set text layer scheme.

@_param_ `pos` - Text block position

`block_position_t`, Position of text block:
* `"topleft"`: Top left corner of the window
* `"topright"`: Top right corner of the window
* `"bottomleft"`: Bottom left corner of the window
* `"bottomright"`: Bottom right corner of the window

@_param_ `scheme` - Array of line templates with overlay scheme

`text_template_t`:
Template for text overlay line.
The template includes text and fields surrounded by curly braces.
The following fields are supported:
* `{name}`: File name of the currently viewed/selected image
* `{dir}`: Parent directory name of the currently viewed/selected image
* `{path}`: Absolute path to the currently viewed/selected image
* `{size}`: File size in bytes
* `{sizehr}`: File size in human-readable format
* `{time}`: File modification time
* `{format}`: Brief image format descriptio
* `{scale}`: Current image scale in percent
* `{list.index}`: Current index of image in the image list
* `{list.total}`: Total number of files in the image list
* `{frame.index}`: Current frame index
* `{frame.total}`: Total number of frames
* `{frame.width}`: Current frame width in pixels
* `{frame.height}`: Current frame height in pixels
* `{meta.*}`: Image meta info: EXIF, tags etc. List of available tags can be
  found at [Exiv2 website](https://exiv2.org/tags.html) or printed using
  utility exiv2: `exiv2 -pa photo.jpg`

Example: `Path to image: {path}`

## Slide show mode

### swayimg.slideshow.set_timeout

```lua
swayimg.slideshow.set_timeout(seconds: number)
```

Set a timeout after which next image should be opened.

@_param_ `seconds` - Timeout in seconds

### swayimg.slideshow.switch_image

```lua
swayimg.slideshow.switch_image(dir: vdir_t)
```

Open the next file in the specified direction.

@_param_ `dir` - Next file direction

`vdir_t`, Direction for opening next file in viewer and slideshow modes:
* `"first"`: First file in image list
* `"last"`: Last file in image list
* `"next"`: Next file
* `"prev"`: Previous file
* `"next_dir"`: First file in next directory
* `"prev_dir"`: Last file in previous directory
* `"random"`: Random file in image list

### swayimg.slideshow.get_image

```lua
swayimg.slideshow.get_image() -> swayimg.image
```

Get information about currently displayed image.

@_return_ - Currently displayed image

### swayimg.slideshow.reset

```lua
swayimg.slideshow.reset()
```

Reset position and scale to default values.

See [swayimg.viewer.set_default_scale](swayimgviewerset_default_scale)

See [swayimg.viewer.set_default_position](swayimgviewerset_default_position)

### swayimg.slideshow.get_scale

```lua
swayimg.slideshow.get_scale() -> number
```

Get current image scale.

@_return_ - Absolute scale value (1.0 = 100%)

### swayimg.slideshow.set_abs_scale

```lua
swayimg.slideshow.set_abs_scale(scale: number, x?: integer, y?: integer)
```

Set absolute image scale.

@_param_ `scale` - Absolute value (1.0 = 100%)

@_param_ `x` - X coordinate of center point, empty for window center

@_param_ `y` - Y coordinate of center point, empty for window center

### swayimg.slideshow.set_fix_scale

```lua
swayimg.slideshow.set_fix_scale(scale: fixed_scale_t)
```

Set fixed scale for currently displayed image.

@_param_ `scale` - Fixed scale name

`fixed_scale_t`, Fixed scale for images in viewer and slideshow modes:
* `"optimal"`: 100% or less to fit to window
* `"width"`: Fit image width to window width
* `"height"`: Fit image height to window height
* `"fit"`: Fit to window
* `"fill"`: Crop image to fill the window
* `"real"`: Real size (100%)
* `"keep"`: Keep the same scale as for previously viewed image

### swayimg.slideshow.set_default_scale

```lua
swayimg.slideshow.set_default_scale(scale: number|fixed_scale_t)
```

Set default image scale for newly opened images.

@_param_ `scale` - Absolute value (1.0 = 100%) or one the predefined names

`fixed_scale_t`, Fixed scale for images in viewer and slideshow modes:
* `"optimal"`: 100% or less to fit to window
* `"width"`: Fit image width to window width
* `"height"`: Fit image height to window height
* `"fit"`: Fit to window
* `"fill"`: Crop image to fill the window
* `"real"`: Real size (100%)
* `"keep"`: Keep the same scale as for previously viewed image

### swayimg.slideshow.get_position

```lua
swayimg.slideshow.get_position() -> { x :integer, y: integer }
```

Get image position.

@_return_ - Image coordinates on the window

### swayimg.slideshow.set_abs_position

```lua
swayimg.slideshow.set_abs_position(x: integer, y: integer)
```

Set absolute image position.

@_param_ `x` - Horizontal image position on the window

@_param_ `y` - Vertical image position on the window

### swayimg.slideshow.set_fix_position

```lua
swayimg.slideshow.set_fix_position(pos: fixed_position_t)
```

Set fixed image position.

@_param_ `pos` - Fixed image position

`fixed_position_t`, Fixed position for images in viewer and slideshow modes:
* `"center"`: Vertical and horizontal center of the window
* `"topcenter"`: Top (vertical) and center (horizontal) of the window
* `"bottomcenter"`: Bottom (vertical) and center (horizontal) of the window
* `"leftcenter"`: Left (horizontal) and center (vertical) of the window
* `"rightcenter"`: Right (horizontal) and center (vertical) of the window
* `"topleft"`: Top left corner of the window
* `"topright"`: Top right corner of the window
* `"bottomleft"`: Bottom left corner of the window
* `"bottomright"`: Bottom right corner of the window

### swayimg.slideshow.set_default_position

```lua
swayimg.slideshow.set_default_position(pos: fixed_position_t)
```

Set default image position for newly opened images.

@_param_ `pos` - Fixed image position

`fixed_position_t`, Fixed position for images in viewer and slideshow modes:
* `"center"`: Vertical and horizontal center of the window
* `"topcenter"`: Top (vertical) and center (horizontal) of the window
* `"bottomcenter"`: Bottom (vertical) and center (horizontal) of the window
* `"leftcenter"`: Left (horizontal) and center (vertical) of the window
* `"rightcenter"`: Right (horizontal) and center (vertical) of the window
* `"topleft"`: Top left corner of the window
* `"topright"`: Top right corner of the window
* `"bottomleft"`: Bottom left corner of the window
* `"bottomright"`: Bottom right corner of the window

### swayimg.slideshow.next_frame

```lua
swayimg.slideshow.next_frame() -> integer
```

Show next frame from multi-frame image (animation).

This function stops the animation.

@_return_ - Index of the currently shown frame

### swayimg.slideshow.prev_frame

```lua
swayimg.slideshow.prev_frame() -> integer
```

Show previous frame from multi-frame image (animation).

This function stops the animation.

@_return_ - Index of the currently shown frame

### swayimg.slideshow.animation_stop

```lua
swayimg.slideshow.animation_stop()
```

Stop animation.

### swayimg.slideshow.animation_resume

```lua
swayimg.slideshow.animation_resume()
```

Resume animation.

### swayimg.slideshow.flip_vertical

```lua
swayimg.slideshow.flip_vertical()
```

Flip image vertically.

### swayimg.slideshow.flip_horizontal

```lua
swayimg.slideshow.flip_horizontal()
```

Flip image horizontally.

### swayimg.slideshow.rotate

```lua
swayimg.slideshow.rotate(angle: rotation_t)
```

Rotate image.

@_param_ `angle` - Rotation angle

`rotation_t`, Fixed rotation angles for images in viewer and slideshow modes:
* `90`: 90 degrees
* `180`: 180 degrees
* `270`: 270 degrees

### swayimg.slideshow.export

```lua
swayimg.slideshow.export(path: string)
```

Export currently displayed frame to PNG file.

@_param_ `path` - Path to the file

### swayimg.slideshow.set_meta

```lua
swayimg.slideshow.set_meta(key: string, value: string)
```

Add/replace/remove meta info for currently displayed image.

@_param_ `key` - Meta key name

@_param_ `value` - Meta value, empty value to remove the record

### swayimg.slideshow.set_drag_button

```lua
swayimg.slideshow.set_drag_button(button: string)
```

Set the mouse button used to drag the image around the window.

@_param_ `button` - Mouse button name, for example `MouseLeft`

### swayimg.slideshow.set_window_background

```lua
swayimg.slideshow.set_window_background(bkg: color_t|bkgmode_t)
```

Set window background color and mode.

@_param_ `bkg` - Solid color or one of the predefined mode

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

`bkgmode_t`, Fixed rotation angles for images in viewer and slideshow modes:
* `"extend"`: Fill window with the current image and blur it
* `"mirror"`: Fill window with the mirrored current image and blur it
* `"auto"`: Fill the window background in `extend` or `mirror` mode depending on the image aspect ratio

### swayimg.slideshow.set_image_background

```lua
swayimg.slideshow.set_image_background(color: color_t)
```

Set background color for transparent images.

This disables chessboard drawing.

@_param_ `color` - Background color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.slideshow.set_image_chessboard

```lua
swayimg.slideshow.set_image_chessboard(size: integer, color1: color_t, color2: color_t)
```

Set parameters for chessboard used as background for transparent images.

This enables the chessboard if this feature was previously disabled.

@_param_ `size` - Size of single grid cell in pixels

@_param_ `color1` - First color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

@_param_ `color2` - Second color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.slideshow.enable_centering

```lua
swayimg.slideshow.enable_centering(enable: boolean)
```

Enable or disable automatic image centering.

@_param_ `enable` - Enable/disable automatic image centering

### swayimg.slideshow.enable_loop

```lua
swayimg.slideshow.enable_loop(enable: boolean)
```

Enable or disable image list loop mode.

@_param_ `enable` - Enable/disable flag to set

### swayimg.slideshow.limit_preload

```lua
swayimg.slideshow.limit_preload(size: integer)
```

Set max number of images to preload in background thread.

@_param_ `size` - Number of images to preload

### swayimg.slideshow.limit_history

```lua
swayimg.slideshow.limit_history(size: integer)
```

Set max number of previously viewed images stored in the cache.

@_param_ `size` - Number of images to store

### swayimg.slideshow.mark_image

```lua
swayimg.slideshow.mark_image(state?: boolean)
```

Set, clear or toggle mark for currently viewed/selected image.

@_param_ `state` - Mark state to set, toggle if the state is not specified

### swayimg.slideshow.set_mark_color

```lua
swayimg.slideshow.set_mark_color(color: color_t)
```

Set mark icon color.

@_param_ `color` - Mark icon color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.slideshow.bind_reset

```lua
swayimg.slideshow.bind_reset()
```

Remove all existing key/mouse/signal bindings.

### swayimg.slideshow.on_key

```lua
swayimg.slideshow.on_key(key: string, fn: function)
```

Bind the key press event to a handler.

@_param_ `key` - Key description, for example `Ctrl-a`

@_param_ `fn` - Key press handler

### swayimg.slideshow.on_mouse

```lua
swayimg.slideshow.on_mouse(button: string, fn: function)
```

Bind the mouse button press event to a handler.

@_param_ `button` - Button description, for example `Ctrl-Alt-MouseRight`

@_param_ `fn` - Button press handler

### swayimg.slideshow.on_signal

```lua
swayimg.slideshow.on_signal(signal: string, fn: function)
```

Bind the signal event to a handler.

@_param_ `signal` - Signal name (`USR1` or `USR2`)

@_param_ `fn` - Signal handler

### swayimg.slideshow.on_image_change

```lua
swayimg.slideshow.on_image_change(fn: function)
```

Add a callback function called when a new image is opened/selected.

@_param_ `fn` - Handler for notifications about changing the current image

### swayimg.slideshow.set_text

```lua
swayimg.slideshow.set_text(pos: block_position_t, scheme: text_template_t[])
```

Set text layer scheme.

@_param_ `pos` - Text block position

`block_position_t`, Position of text block:
* `"topleft"`: Top left corner of the window
* `"topright"`: Top right corner of the window
* `"bottomleft"`: Bottom left corner of the window
* `"bottomright"`: Bottom right corner of the window

@_param_ `scheme` - Array of line templates with overlay scheme

`text_template_t`:
Template for text overlay line.
The template includes text and fields surrounded by curly braces.
The following fields are supported:
* `{name}`: File name of the currently viewed/selected image
* `{dir}`: Parent directory name of the currently viewed/selected image
* `{path}`: Absolute path to the currently viewed/selected image
* `{size}`: File size in bytes
* `{sizehr}`: File size in human-readable format
* `{time}`: File modification time
* `{format}`: Brief image format descriptio
* `{scale}`: Current image scale in percent
* `{list.index}`: Current index of image in the image list
* `{list.total}`: Total number of files in the image list
* `{frame.index}`: Current frame index
* `{frame.total}`: Total number of frames
* `{frame.width}`: Current frame width in pixels
* `{frame.height}`: Current frame height in pixels
* `{meta.*}`: Image meta info: EXIF, tags etc. List of available tags can be
  found at [Exiv2 website](https://exiv2.org/tags.html) or printed using
  utility exiv2: `exiv2 -pa photo.jpg`

Example: `Path to image: {path}`

## Gallery mode

### swayimg.gallery.switch_image

```lua
swayimg.gallery.switch_image(dir: gdir_t)
```

Select the next thumbnail from the gallery.

@_param_ `dir` - Next thumbnail direction

`gdir_t`, Direction for selecting next file in gallery mode:
* `"first"`: Select first thumbnail in image list
* `"last"`: Select last thumbnail in image list
* `"up"`: Select the thumbnail above the current one
* `"down"`: Select the thumbnail below the current one
* `"left"`: Select the thumbnail to the left of the current one
* `"right"`: Select the thumbnail to the right of the current one
* `"pgup"`: Select the thumbnail on the previous page
* `"pgdown"`: Select the thumbnail on the next page

### swayimg.gallery.get_image

```lua
swayimg.gallery.get_image() -> swayimg.entry
```

Get information about currently selected image entry.

@_return_ - Currently selected image entry

### swayimg.gallery.set_aspect

```lua
swayimg.gallery.set_aspect(aspect: aspect_t)
```

Set thumbnail aspect ratio.

@_param_ `aspect` - Thumbnail aspect ratio

`aspect_t`, Aspect ratio used for thumbnails in gallery mode:
* `"fit"`: Fit image into a square thumbnail
* `"fill"`: Fill square thumbnail with the image
* `"keep"`: Adjust thumbnail size to the aspect ratio of the image

### swayimg.gallery.get_thumb_size

```lua
swayimg.gallery.get_thumb_size() -> integer
```

Get thumbnail size.

@_return_ - Thumbnail size in pixels

### swayimg.gallery.set_thumb_size

```lua
swayimg.gallery.set_thumb_size(size: integer)
```

Set thumbnail size.

@_param_ `size` - Thumbnail size in pixels

### swayimg.gallery.set_padding_size

```lua
swayimg.gallery.set_padding_size(size: integer)
```

Set the padding size between thumbnails.

@_param_ `size` - Padding size in pixels

### swayimg.gallery.set_border_size

```lua
swayimg.gallery.set_border_size(size: integer)
```

Set the border size for currently selected thumbnail.

@_param_ `size` - Border size in pixels

### swayimg.gallery.set_border_color

```lua
swayimg.gallery.set_border_color(color: color_t)
```

Set border color for currently selected thumbnail.

@_param_ `color` - Border color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.gallery.set_selected_scale

```lua
swayimg.gallery.set_selected_scale(scale: number)
```

Set the scale factor for currently selected thumbnail.

@_param_ `scale` - Scale factor, 1.0 = 100%

### swayimg.gallery.set_selected_color

```lua
swayimg.gallery.set_selected_color(color: color_t)
```

Set background color for currently selected thumbnail.

@_param_ `color` - Background color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.gallery.set_unselected_color

```lua
swayimg.gallery.set_unselected_color(color: color_t)
```

Set background color for unselected thumbnails.

@_param_ `color` - Background color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.gallery.set_window_color

```lua
swayimg.gallery.set_window_color(color: color_t)
```

Set window background color.

@_param_ `color` - Background color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.gallery.limit_cache

```lua
swayimg.gallery.limit_cache(size: integer)
```

Set max number of thumbnails stored in memory cache.

@_param_ `size` - Cache size

### swayimg.gallery.enable_preload

```lua
swayimg.gallery.enable_preload(enable: boolean)
```

Enable or disable preloading invisible thumbnails.

@_param_ `enable` - Enable/disable preloading invisible thumbnails

### swayimg.gallery.enable_pstore

```lua
swayimg.gallery.enable_pstore(enable: boolean)
```

Enable or disable persistent storage for thumbnails.

@_param_ `enable` - Enable/disable usage of persistent storage

### swayimg.gallery.set_pstore_path

```lua
swayimg.gallery.set_pstore_path(path: string)
```

Set custom path for persistent storage for thumbnails.

@_param_ `path` - Path to the directory

### swayimg.gallery.mark_image

```lua
swayimg.gallery.mark_image(state?: boolean)
```

Set, clear or toggle mark for currently viewed/selected image.

@_param_ `state` - Mark state to set, toggle if the state is not specified

### swayimg.gallery.set_mark_color

```lua
swayimg.gallery.set_mark_color(color: color_t)
```

Set mark icon color.

@_param_ `color` - Mark icon color

`color_t`:
ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.gallery.bind_reset

```lua
swayimg.gallery.bind_reset()
```

Remove all existing key/mouse/signal bindings.

### swayimg.gallery.on_key

```lua
swayimg.gallery.on_key(key: string, fn: function)
```

Bind the key press event to a handler.

@_param_ `key` - Key description, for example `Ctrl-a`

@_param_ `fn` - Key press handler

### swayimg.gallery.on_mouse

```lua
swayimg.gallery.on_mouse(button: string, fn: function)
```

Bind the mouse button press event to a handler.

@_param_ `button` - Button description, for example `Ctrl-Alt-MouseRight`

@_param_ `fn` - Button press handler

### swayimg.gallery.on_signal

```lua
swayimg.gallery.on_signal(signal: string, fn: function)
```

Bind the signal event to a handler.

@_param_ `signal` - Signal name (`USR1` or `USR2`)

@_param_ `fn` - Signal handler

### swayimg.gallery.on_image_change

```lua
swayimg.gallery.on_image_change(fn: function)
```

Add a callback function called when a new image is opened/selected.

@_param_ `fn` - Handler for notifications about changing the current image

### swayimg.gallery.set_text

```lua
swayimg.gallery.set_text(pos: block_position_t, scheme: text_template_t[])
```

Set text layer scheme.

@_param_ `pos` - Text block position

`block_position_t`, Position of text block:
* `"topleft"`: Top left corner of the window
* `"topright"`: Top right corner of the window
* `"bottomleft"`: Bottom left corner of the window
* `"bottomright"`: Bottom right corner of the window

@_param_ `scheme` - Array of line templates with overlay scheme

`text_template_t`:
Template for text overlay line.
The template includes text and fields surrounded by curly braces.
The following fields are supported:
* `{name}`: File name of the currently viewed/selected image
* `{dir}`: Parent directory name of the currently viewed/selected image
* `{path}`: Absolute path to the currently viewed/selected image
* `{size}`: File size in bytes
* `{sizehr}`: File size in human-readable format
* `{time}`: File modification time
* `{format}`: Brief image format descriptio
* `{scale}`: Current image scale in percent
* `{list.index}`: Current index of image in the image list
* `{list.total}`: Total number of files in the image list
* `{frame.index}`: Current frame index
* `{frame.total}`: Total number of frames
* `{frame.width}`: Current frame width in pixels
* `{frame.height}`: Current frame height in pixels
* `{meta.*}`: Image meta info: EXIF, tags etc. List of available tags can be
  found at [Exiv2 website](https://exiv2.org/tags.html) or printed using
  utility exiv2: `exiv2 -pa photo.jpg`

Example: `Path to image: {path}`

