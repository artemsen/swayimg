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
swayimg.text.size = 32
swayimg.text.color = 0xffff0000

swayimg.viewer.default_scale = "fill"

swayimg.gallery.on_key("Delete", function()
  local image = swayimg.gallery.get_image()
  os.remove(image.path)
end)
```

A more detailed example can be found on the [project website](extra/example.lua)
or in the file `/usr/share/swayimg/example.lua` after installing the program.

## List of available functions

* General functionality
  * [swayimg.appid](#swayimgappid): Application Id
  * [swayimg.mode](#swayimgmode): Application mode
  * [swayimg.fullscreen](#swayimgfullscreen): Full screen mode
  * [swayimg.dnd_button](#swayimgdnd_button): Mouse button used for drag-and-drop image file to external apps
  * [swayimg.overlay](#swayimgoverlay): Window overlay mode
  * [swayimg.decoration](#swayimgdecoration): Window decoration (title, border, buttons)
  * [swayimg.antialiasing](#swayimgantialiasing): Anti-aliasing mode
  * [swayimg.exif_orientation](#swayimgexif_orientation): Automatic orientation based on EXIF
  * [swayimg.title](#swayimgtitle): Window title
  * [swayimg.exit()](#swayimgexit): Exit from application
  * [swayimg.get_window_size()](#swayimgget_window_size): Get application window size
  * [swayimg.set_window_size()](#swayimgset_window_size): Set application window size
  * [swayimg.on_window_resize()](#swayimgon_window_resize): Set a callback function called when main window is resized
  * [swayimg.get_mouse_pos()](#swayimgget_mouse_pos): Get mouse pointer coordinates
  * [swayimg.on_initialized()](#swayimgon_initialized): Set a callback function called when all subsystems have been initialized
  * [swayimg.on_redrawn()](#swayimgon_redrawn): Set a callback function called after the window is drawn
  * [swayimg.defer()](#swayimgdefer): Execute deferred procedure
  * [swayimg.set_format_params()](#swayimgset_format_params): Setting format parameters
* Image list
  * [swayimg.imagelist.order](#swayimgimagelistorder): Sort order of the image list
  * [swayimg.imagelist.reverse](#swayimgimagelistreverse): Reverse the image list order
  * [swayimg.imagelist.recursive](#swayimgimagelistrecursive): Recursive directory reading
  * [swayimg.imagelist.adjacent](#swayimgimagelistadjacent): Adding adjacent files from the same directory
  * [swayimg.imagelist.fsmon](#swayimgimagelistfsmon): File system monitoring
  * [swayimg.imagelist.size](#swayimgimagelistsize): Total number of entries in the image list
  * [swayimg.imagelist.add()](#swayimgimagelistadd): Add entries to the image list
  * [swayimg.imagelist.remove()](#swayimgimagelistremove): Remove specified entries from the image list
  * [swayimg.imagelist.clear()](#swayimgimagelistclear): Clear the image list
  * [swayimg.imagelist.get()](#swayimgimagelistget): Get list of all entries in the image list
* Text overlay layer
  * [swayimg.text.visible](#swayimgtextvisible): Text overlay state
  * [swayimg.text.timeout](#swayimgtexttimeout): Timeout in seconds after which the entire text layer will be hidden
  * [swayimg.text.status_timeout](#swayimgtextstatus_timeout): Timeout in seconds after which the status message will be hidden
  * [swayimg.text.font](#swayimgtextfont): Font name
  * [swayimg.text.size](#swayimgtextsize): Font size in pixels
  * [swayimg.text.spacing](#swayimgtextspacing): Line spacing in pixels
  * [swayimg.text.padding](#swayimgtextpadding): Padding from the window edges in pixels
  * [swayimg.text.color](#swayimgtextcolor): Foreground text color
  * [swayimg.text.background](#swayimgtextbackground): Background text color
  * [swayimg.text.shadow](#swayimgtextshadow): Shadow text color
  * [swayimg.text.status](#swayimgtextstatus): Status message
* Viewer mode
  * [swayimg.viewer.autocenter](#swayimgviewerautocenter): Automatic image centering
  * [swayimg.viewer.loop](#swayimgviewerloop): Image list loop mode
  * [swayimg.viewer.default_scale](#swayimgviewerdefault_scale): |fixed_scale_t
  * [swayimg.viewer.default_position](#swayimgviewerdefault_position): Default image position for newly opened images
  * [swayimg.viewer.scale](#swayimgviewerscale): Absolute scale value (1.0 = 100%)
  * [swayimg.viewer.animation](#swayimgvieweranimation): Stop/resume and get animation status
  * [swayimg.viewer.frame](#swayimgviewerframe): Currently displayed frame number
  * [swayimg.viewer.drag_button](#swayimgviewerdrag_button): Mouse button used for drag image around the window
  * [swayimg.viewer.preload](#swayimgviewerpreload): Max number of images to preload in background thread
  * [swayimg.viewer.history](#swayimgviewerhistory): Max number of previously viewed images stored in the cache
  * [swayimg.viewer.mark_color](#swayimgviewermark_color): Mark icon color
  * [swayimg.viewer.pinch_factor](#swayimgviewerpinch_factor): Pinch gesture factor
  * [swayimg.viewer.switch_image()](#swayimgviewerswitch_image): Open the next file in the specified direction
  * [swayimg.viewer.open()](#swayimgvieweropen): Open the next file in the specified direction
  * [swayimg.viewer.open_path()](#swayimgvieweropen_path): Open the file at the specified path
  * [swayimg.viewer.get_image()](#swayimgviewerget_image): Get information about currently displayed image
  * [swayimg.viewer.reload()](#swayimgviewerreload): Reload current image
  * [swayimg.viewer.set_abs_scale()](#swayimgviewerset_abs_scale): Set absolute image scale
  * [swayimg.viewer.set_fix_scale()](#swayimgviewerset_fix_scale): Set fixed scale for currently displayed image
  * [swayimg.viewer.reset()](#swayimgviewerreset): Reset position and scale to default values
  * [swayimg.viewer.get_position()](#swayimgviewerget_position): Get image position
  * [swayimg.viewer.set_abs_position()](#swayimgviewerset_abs_position): Set absolute image position
  * [swayimg.viewer.set_fix_position()](#swayimgviewerset_fix_position): Set fixed image position
  * [swayimg.viewer.flip_vertical()](#swayimgviewerflip_vertical): Flip image vertically
  * [swayimg.viewer.flip_horizontal()](#swayimgviewerflip_horizontal): Flip image horizontally
  * [swayimg.viewer.rotate()](#swayimgviewerrotate): Rotate image
  * [swayimg.viewer.export()](#swayimgviewerexport): Export currently displayed frame to PNG file
  * [swayimg.viewer.set_meta()](#swayimgviewerset_meta): Add/replace/remove meta info for currently displayed image
  * [swayimg.viewer.set_window_background()](#swayimgviewerset_window_background): Set window background color or extension mode
  * [swayimg.viewer.set_image_background()](#swayimgviewerset_image_background): Set background color for transparent images
  * [swayimg.viewer.set_image_chessboard()](#swayimgviewerset_image_chessboard): Set parameters for chessboard used as background for transparent images
  * [swayimg.viewer.mark_image()](#swayimgviewermark_image): Set, clear or toggle mark for currently viewed/selected image
  * [swayimg.viewer.bind_reset()](#swayimgviewerbind_reset): Remove all existing key/mouse/signal bindings
  * [swayimg.viewer.on_key()](#swayimgvieweron_key): Bind the key press event to a handler
  * [swayimg.viewer.on_mouse()](#swayimgvieweron_mouse): Bind the mouse button press event to a handler
  * [swayimg.viewer.on_signal()](#swayimgvieweron_signal): Bind the signal event to a handler
  * [swayimg.viewer.on_image_change()](#swayimgvieweron_image_change): Set a callback function called when a new image is opened/selected
  * [swayimg.viewer.set_text()](#swayimgviewerset_text): Set text layer scheme
* Slide show mode
  * [swayimg.slideshow.timeout](#swayimgslideshowtimeout): Timeout in seconds after which next image should be opened
  * [swayimg.slideshow.autocenter](#swayimgslideshowautocenter): Automatic image centering
  * [swayimg.slideshow.loop](#swayimgslideshowloop): Image list loop mode
  * [swayimg.slideshow.default_scale](#swayimgslideshowdefault_scale): |fixed_scale_t
  * [swayimg.slideshow.default_position](#swayimgslideshowdefault_position): Default image position for newly opened images
  * [swayimg.slideshow.scale](#swayimgslideshowscale): Absolute scale value (1.0 = 100%)
  * [swayimg.slideshow.animation](#swayimgslideshowanimation): Stop/resume and get animation status
  * [swayimg.slideshow.frame](#swayimgslideshowframe): Currently displayed frame number
  * [swayimg.slideshow.drag_button](#swayimgslideshowdrag_button): Mouse button used for drag image around the window
  * [swayimg.slideshow.preload](#swayimgslideshowpreload): Max number of images to preload in background thread
  * [swayimg.slideshow.history](#swayimgslideshowhistory): Max number of previously viewed images stored in the cache
  * [swayimg.slideshow.mark_color](#swayimgslideshowmark_color): Mark icon color
  * [swayimg.slideshow.pinch_factor](#swayimgslideshowpinch_factor): Pinch gesture factor
  * [swayimg.slideshow.switch_image()](#swayimgslideshowswitch_image): Open the next file in the specified direction
  * [swayimg.slideshow.open()](#swayimgslideshowopen): Open the next file in the specified direction
  * [swayimg.slideshow.open_path()](#swayimgslideshowopen_path): Open the file at the specified path
  * [swayimg.slideshow.get_image()](#swayimgslideshowget_image): Get information about currently displayed image
  * [swayimg.slideshow.reload()](#swayimgslideshowreload): Reload current image
  * [swayimg.slideshow.set_abs_scale()](#swayimgslideshowset_abs_scale): Set absolute image scale
  * [swayimg.slideshow.set_fix_scale()](#swayimgslideshowset_fix_scale): Set fixed scale for currently displayed image
  * [swayimg.slideshow.reset()](#swayimgslideshowreset): Reset position and scale to default values
  * [swayimg.slideshow.get_position()](#swayimgslideshowget_position): Get image position
  * [swayimg.slideshow.set_abs_position()](#swayimgslideshowset_abs_position): Set absolute image position
  * [swayimg.slideshow.set_fix_position()](#swayimgslideshowset_fix_position): Set fixed image position
  * [swayimg.slideshow.flip_vertical()](#swayimgslideshowflip_vertical): Flip image vertically
  * [swayimg.slideshow.flip_horizontal()](#swayimgslideshowflip_horizontal): Flip image horizontally
  * [swayimg.slideshow.rotate()](#swayimgslideshowrotate): Rotate image
  * [swayimg.slideshow.export()](#swayimgslideshowexport): Export currently displayed frame to PNG file
  * [swayimg.slideshow.set_meta()](#swayimgslideshowset_meta): Add/replace/remove meta info for currently displayed image
  * [swayimg.slideshow.set_window_background()](#swayimgslideshowset_window_background): Set window background color or extension mode
  * [swayimg.slideshow.set_image_background()](#swayimgslideshowset_image_background): Set background color for transparent images
  * [swayimg.slideshow.set_image_chessboard()](#swayimgslideshowset_image_chessboard): Set parameters for chessboard used as background for transparent images
  * [swayimg.slideshow.mark_image()](#swayimgslideshowmark_image): Set, clear or toggle mark for currently viewed/selected image
  * [swayimg.slideshow.bind_reset()](#swayimgslideshowbind_reset): Remove all existing key/mouse/signal bindings
  * [swayimg.slideshow.on_key()](#swayimgslideshowon_key): Bind the key press event to a handler
  * [swayimg.slideshow.on_mouse()](#swayimgslideshowon_mouse): Bind the mouse button press event to a handler
  * [swayimg.slideshow.on_signal()](#swayimgslideshowon_signal): Bind the signal event to a handler
  * [swayimg.slideshow.on_image_change()](#swayimgslideshowon_image_change): Set a callback function called when a new image is opened/selected
  * [swayimg.slideshow.set_text()](#swayimgslideshowset_text): Set text layer scheme
* Gallery mode
  * [swayimg.gallery.aspect](#swayimggalleryaspect): Thumbnail aspect ratio
  * [swayimg.gallery.thumb_size](#swayimggallerythumb_size): Thumbnail size in pixels
  * [swayimg.gallery.padding_size](#swayimggallerypadding_size): Padding size in pixels between thumbnails
  * [swayimg.gallery.border_size](#swayimggalleryborder_size): Border size in pixels for currently selected thumbnail
  * [swayimg.gallery.selected_scale](#swayimggalleryselected_scale): Scale factor for currently selected thumbnail
  * [swayimg.gallery.window_color](#swayimggallerywindow_color): Background color
  * [swayimg.gallery.unselected_color](#swayimggalleryunselected_color): Background color for unselected thumbnails
  * [swayimg.gallery.selected_color](#swayimggalleryselected_color): Background color for currently selected thumbnail
  * [swayimg.gallery.border_color](#swayimggalleryborder_color): Border color for currently selected thumbnail
  * [swayimg.gallery.hover](#swayimggalleryhover): Change current thumbnail on mouse hover
  * [swayimg.gallery.pstore](#swayimggallerypstore): Use persistent storage for thumbnails
  * [swayimg.gallery.pstore_path](#swayimggallerypstore_path): Path for thumbnails persistent storage
  * [swayimg.gallery.preload](#swayimggallerypreload): Preload invisible thumbnails
  * [swayimg.gallery.cache](#swayimggallerycache): Max number of invisible thumbnails stored in memory cache
  * [swayimg.gallery.embedded_thumb](#swayimggalleryembedded_thumb): Use embedded thumbnails
  * [swayimg.gallery.mark_color](#swayimggallerymark_color): Mark icon color
  * [swayimg.gallery.pinch_factor](#swayimggallerypinch_factor): Pinch gesture factor
  * [swayimg.gallery.switch_image()](#swayimggalleryswitch_image): Select the next thumbnail from the gallery
  * [swayimg.gallery.select()](#swayimggalleryselect): Select the next thumbnail from the gallery
  * [swayimg.gallery.select_at()](#swayimggalleryselect_at): Select the thumbnail at specified position
  * [swayimg.gallery.select_path()](#swayimggalleryselect_path): Select the thumbnail by image path
  * [swayimg.gallery.reload()](#swayimggalleryreload): Reload thumbnails
  * [swayimg.gallery.get_image()](#swayimggalleryget_image): Get information about currently selected image entry
  * [swayimg.gallery.mark_image()](#swayimggallerymark_image): Set, clear or toggle mark for currently viewed/selected image
  * [swayimg.gallery.bind_reset()](#swayimggallerybind_reset): Remove all existing key/mouse/signal bindings
  * [swayimg.gallery.on_key()](#swayimggalleryon_key): Bind the key press event to a handler
  * [swayimg.gallery.on_mouse()](#swayimggalleryon_mouse): Bind the mouse button press event to a handler
  * [swayimg.gallery.on_signal()](#swayimggalleryon_signal): Bind the signal event to a handler
  * [swayimg.gallery.on_image_change()](#swayimggalleryon_image_change): Set a callback function called when a new image is opened/selected
  * [swayimg.gallery.set_text()](#swayimggalleryset_text): Set text layer scheme

## General functionality

### swayimg.appid

```lua
swayimg.appid: string
```

Application Id.

Since 5.5.

This field can be set only at program startup.

### swayimg.mode

```lua
swayimg.mode: appmode_t
```

Application mode.

Since 5.5.

Setting this field changes the current mode (viewer/slideshow/gallery).

`appmode_t` - Application mode:
* `"viewer"`: Image viewer mode
* `"slideshow"`: Slide show mode
* `"gallery"`: Gallery mode

### swayimg.fullscreen

```lua
swayimg.fullscreen: boolean
```

Full screen mode.

Since 5.5.

### swayimg.dnd_button

```lua
swayimg.dnd_button: mbutton_t
```

Mouse button used for drag-and-drop image file to external apps.

Since 5.5.

Write-only field which can be set at startup.

`mbutton_t` - Mouse buttons:
* `"MouseLeft"`: Left mouse button
* `"MouseRight"`: Right mouse button
* `"MouseMiddle"`: Middle mouse button
* `"MouseSide"`: Side mouse button
* `"MouseExtra"`: Extra mouse button
* `"ScrollUp"`: Scroll up
* `"ScrollDown"`: Scroll down
* `"ScrollLeft"`: Scroll left
* `"ScrollRight"`: Scroll right

### swayimg.overlay

```lua
swayimg.overlay: boolean
```

Window overlay mode.

Since 5.5.

Write-only field which can be set at startup.

Create a floating window with the same coordinates and size as the currently focused window.

Applicable only in Wayland, the corresponding protocol must be supported by the composer.

By default enabled in Sway and disabled in other compositors.

### swayimg.decoration

```lua
swayimg.decoration: boolean
```

Window decoration (title, border, buttons).

Since 5.5.

Write-only field which can be set at startup.

Applicable only in Wayland, the corresponding protocol must be supported by the composer.

By default disabled in Sway and enabled in other compositors.

### swayimg.antialiasing

```lua
swayimg.antialiasing: boolean
```

Anti-aliasing mode.

Since 5.5.

### swayimg.exif_orientation

```lua
swayimg.exif_orientation: boolean
```

Automatic orientation based on EXIF.

Since 5.5.

Write-only field.

### swayimg.title

```lua
swayimg.title: string
```

Window title.

Since 5.5.

Write-only field.

### swayimg.exit

```lua
swayimg.exit(code?: integer)
```

Exit from application.

Since 5.0.

@_param_ `code` - Program exit code, `0` by default

### swayimg.get_window_size

```lua
swayimg.get_window_size() -> { width: integer, height: integer }
```

Get application window size.

Since 5.0.

@_return_ - Window size in pixels

### swayimg.set_window_size

```lua
swayimg.set_window_size(width: integer, height: integer)
```

Set application window size.

Since 5.0.

@_param_ `width` - Width of the window in pixels

@_param_ `height` - Height of the window in pixels

### swayimg.on_window_resize

```lua
swayimg.on_window_resize(fn: function|nil)
```

Set a callback function called when main window is resized.

Since 5.0.

@_param_ `fn` - Window resize notification handler

### swayimg.get_mouse_pos

```lua
swayimg.get_mouse_pos() -> { x :integer, y: integer }
```

Get mouse pointer coordinates.

Since 5.0.

@_return_ - Coordinates of the mouse pointer

### swayimg.on_initialized

```lua
swayimg.on_initialized(fn: function)
```

Set a callback function called when all subsystems have been initialized.

Since 5.0.

@_param_ `fn` - Initialization completion notification handler

### swayimg.on_redrawn

```lua
swayimg.on_redrawn(fn: function|nil)
```

Set a callback function called after the window is drawn.

Since 5.5.

@_param_ `fn` - Function to execute

### swayimg.defer

```lua
swayimg.defer(seconds: number, fn: function)
```

Execute deferred procedure.

Since 5.5.

@_param_ `seconds` - Delay in seconds (can be fractional)

@_param_ `fn` - Function to execute

### swayimg.set_format_params

```lua
swayimg.set_format_params(name: string, params: table)
```

Setting format parameters.

Since 5.3.

Supported parameters:
* `raw`:
  * `camera_wb`: Fix colors using white balance from camera

@_param_ `name` - Format name (e.g. `raw`)

@_param_ `params` - Table of parameters (e.g. `{ camera_wb = true }`)

## Image list

### swayimg.imagelist.order

```lua
swayimg.imagelist.order: order_t
```

Sort order of the image list.

Since 5.5.

`order_t` - Image list order:
* `"none"`: Unsorted (system-dependent)
* `"alpha"`: Lexicographic sort: 1,10,2,20,a,b,c
* `"numeric"`: Numeric sort: 1,2,3,10,100,a,b,c
* `"mtime"`: Modification time sort
* `"size"`: Size sort
* `"random"`: Random order

### swayimg.imagelist.reverse

```lua
swayimg.imagelist.reverse: boolean
```

Reverse the image list order.

Since 5.5.

### swayimg.imagelist.recursive

```lua
swayimg.imagelist.recursive: boolean
```

Recursive directory reading.

Since 5.5.

### swayimg.imagelist.adjacent

```lua
swayimg.imagelist.adjacent: boolean
```

Adding adjacent files from the same directory.

Since 5.5.

### swayimg.imagelist.fsmon

```lua
swayimg.imagelist.fsmon: boolean
```

File system monitoring.

Since 5.5.

### swayimg.imagelist.size

```lua
swayimg.imagelist.size: integer
```

Total number of entries in the image list.

Since 5.5.

Read-only field.

### swayimg.imagelist.add

```lua
swayimg.imagelist.add(paths: string|string[])
```

Add entries to the image list.

Since 5.0.

@_param_ `paths` - Paths to add

### swayimg.imagelist.remove

```lua
swayimg.imagelist.remove(paths: string|string[])
```

Remove specified entries from the image list.

Since 5.0.

@_param_ `paths` - Paths to remove

### swayimg.imagelist.clear

```lua
swayimg.imagelist.clear()
```

Clear the image list.

Since 5.3.

### swayimg.imagelist.get

```lua
swayimg.imagelist.get() -> swayimg.entry[]
```

Get list of all entries in the image list.

Since 5.0.

@_return_ - Array with all file entries

## Text overlay layer

### swayimg.text.visible

```lua
swayimg.text.visible: boolean
```

Text overlay state.

Since 5.5.

### swayimg.text.timeout

```lua
swayimg.text.timeout: number
```

Timeout in seconds after which the entire text layer will be hidden.

Since 5.5.

Write-only field.

### swayimg.text.status_timeout

```lua
swayimg.text.status_timeout: number
```

Timeout in seconds after which the status message will be hidden.

Since 5.5.

Write-only field.

### swayimg.text.font

```lua
swayimg.text.font: string
```

Font name.

Since 5.5.

Write-only field.

### swayimg.text.size

```lua
swayimg.text.size: integer
```

Font size in pixels.

Since 5.5.

Write-only field.

### swayimg.text.spacing

```lua
swayimg.text.spacing: integer
```

Line spacing in pixels.

Since 5.5.

Write-only field.

### swayimg.text.padding

```lua
swayimg.text.padding: integer
```

Padding from the window edges in pixels.

Since 5.5.

This is write-only field.

### swayimg.text.color

```lua
swayimg.text.color: color_t
```

Foreground text color.

Since 5.5.

Write-only field.

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.text.background

```lua
swayimg.text.background: color_t
```

Background text color.

Since 5.5.

Write-only field.

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.text.shadow

```lua
swayimg.text.shadow: color_t
```

Shadow text color.

Since 5.5.

Write-only field.

Setting alpha channel to `0` disables shadows.

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.text.status

```lua
swayimg.text.status: string
```

Status message.

Since 5.5.

Write-only field.

Multi-line text should be separated by new line character `\n`.

## Viewer mode

### swayimg.viewer.autocenter

```lua
swayimg.viewer.autocenter: boolean
```

Automatic image centering.

Since 5.5.

Write-only field.

### swayimg.viewer.loop

```lua
swayimg.viewer.loop: boolean
```

Image list loop mode.

Since 5.5.

Write-only field.

### swayimg.viewer.default_scale

```lua
swayimg.viewer.default_scale: number
```

|fixed_scale_t.

Default image scale for newly opened images.

Since 5.5.

Write-only field.

### swayimg.viewer.default_position

```lua
swayimg.viewer.default_position: fixed_position_t
```

Default image position for newly opened images.

Since 5.5.

Write-only field.

`fixed_position_t` - Fixed position for images in viewer and slideshow modes:
* `"center"`: Vertical and horizontal center of the window
* `"topcenter"`: Top (vertical) and center (horizontal) of the window
* `"bottomcenter"`: Bottom (vertical) and center (horizontal) of the window
* `"leftcenter"`: Left (horizontal) and center (vertical) of the window
* `"rightcenter"`: Right (horizontal) and center (vertical) of the window
* `"topleft"`: Top left corner of the window
* `"topright"`: Top right corner of the window
* `"bottomleft"`: Bottom left corner of the window
* `"bottomright"`: Bottom right corner of the window

### swayimg.viewer.scale

```lua
swayimg.viewer.scale: number
```

Absolute scale value (1.0 = 100%).

Since 5.5.

### swayimg.viewer.animation

```lua
swayimg.viewer.animation: boolean
```

Stop/resume and get animation status.

Since 5.5.

### swayimg.viewer.frame

```lua
swayimg.viewer.frame: integer
```

Currently displayed frame number.

Since 5.5.

Setting this field stops animation.

### swayimg.viewer.drag_button

```lua
swayimg.viewer.drag_button: mbutton_t
```

Mouse button used for drag image around the window.

Since 5.5.

Write-only field.

`mbutton_t` - Mouse buttons:
* `"MouseLeft"`: Left mouse button
* `"MouseRight"`: Right mouse button
* `"MouseMiddle"`: Middle mouse button
* `"MouseSide"`: Side mouse button
* `"MouseExtra"`: Extra mouse button
* `"ScrollUp"`: Scroll up
* `"ScrollDown"`: Scroll down
* `"ScrollLeft"`: Scroll left
* `"ScrollRight"`: Scroll right

### swayimg.viewer.preload

```lua
swayimg.viewer.preload: integer
```

Max number of images to preload in background thread.

Since 5.5.

Write-only field.

### swayimg.viewer.history

```lua
swayimg.viewer.history: integer
```

Max number of previously viewed images stored in the cache.

Since 5.5.

Write-only field.

### swayimg.viewer.mark_color

```lua
swayimg.viewer.mark_color: color_t
```

Mark icon color.

Since 5.5.

Write-only field.

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.viewer.pinch_factor

```lua
swayimg.viewer.pinch_factor: number
```

Pinch gesture factor.

Since 5.5.

Write-only field.

### swayimg.viewer.switch_image

```lua
swayimg.viewer.switch_image(dir: vdir_t)
```

Open the next file in the specified direction.

Since 5.0.

WARNING: This function is deprecated, use `swayimg.viewer.open` instead.

See [swayimg.viewer.open](#swayimgvieweropen).

@_param_ `dir` - Next file direction

`vdir_t` - Direction for opening next file in viewer and slideshow modes:
* `"first"`: First file in image list
* `"last"`: Last file in image list
* `"next"`: Next file
* `"prev"`: Previous file
* `"next_dir"`: First file in next directory
* `"prev_dir"`: Last file in previous directory
* `"random"`: Random file in image list

### swayimg.viewer.open

```lua
swayimg.viewer.open(dir: vdir_t) -> boolean
```

Open the next file in the specified direction.

Since 5.5.

@_param_ `dir` - Next file direction

`vdir_t` - Direction for opening next file in viewer and slideshow modes:
* `"first"`: First file in image list
* `"last"`: Last file in image list
* `"next"`: Next file
* `"prev"`: Previous file
* `"next_dir"`: First file in next directory
* `"prev_dir"`: Last file in previous directory
* `"random"`: Random file in image list

@_return_ - True if next file was opened

### swayimg.viewer.open_path

```lua
swayimg.viewer.open_path(path: string) -> boolean
```

Open the file at the specified path.

Since 5.5.

This function adds a file to the image list and then opens it in the viewer.

@_param_ `path` - Path to the file

@_return_ - True if file was opened

### swayimg.viewer.get_image

```lua
swayimg.viewer.get_image() -> swayimg.image|nil
```

Get information about currently displayed image.

Since 5.0.

@_return_ - Currently displayed image

### swayimg.viewer.reload

```lua
swayimg.viewer.reload()
```

Reload current image.

Since 5.1.

### swayimg.viewer.set_abs_scale

```lua
swayimg.viewer.set_abs_scale(scale: number, x?: integer, y?: integer)
```

Set absolute image scale.

Since 5.0.

@_param_ `scale` - Absolute value (1.0 = 100%)

@_param_ `x` - X coordinate of center point, empty for window center

@_param_ `y` - Y coordinate of center point, empty for window center

### swayimg.viewer.set_fix_scale

```lua
swayimg.viewer.set_fix_scale(scale: fixed_scale_t)
```

Set fixed scale for currently displayed image.

Since 5.0.

@_param_ `scale` - Fixed scale name

`fixed_scale_t` - Fixed scale for images in viewer and slideshow modes:
* `"optimal"`: 100% or less to fit to window
* `"width"`: Fit image width to window width
* `"height"`: Fit image height to window height
* `"fit"`: Fit to window
* `"fill"`: Crop image to fill the window
* `"real"`: Real size (100%)
* `"keep"`: Keep the same scale as for previously viewed image

### swayimg.viewer.reset

```lua
swayimg.viewer.reset()
```

Reset position and scale to default values.

Since 5.0.

See [swayimg.viewer.set_default_scale](#swayimgviewerset_default_scale).

See [swayimg.viewer.set_default_position](#swayimgviewerset_default_position).

### swayimg.viewer.get_position

```lua
swayimg.viewer.get_position() -> { x :integer, y: integer }
```

Get image position.

Since 5.0.

@_return_ - Image coordinates on the window

### swayimg.viewer.set_abs_position

```lua
swayimg.viewer.set_abs_position(x: integer, y: integer)
```

Set absolute image position.

Since 5.0.

@_param_ `x` - Horizontal image position on the window

@_param_ `y` - Vertical image position on the window

### swayimg.viewer.set_fix_position

```lua
swayimg.viewer.set_fix_position(pos: fixed_position_t)
```

Set fixed image position.

Since 5.0.

@_param_ `pos` - Fixed image position

`fixed_position_t` - Fixed position for images in viewer and slideshow modes:
* `"center"`: Vertical and horizontal center of the window
* `"topcenter"`: Top (vertical) and center (horizontal) of the window
* `"bottomcenter"`: Bottom (vertical) and center (horizontal) of the window
* `"leftcenter"`: Left (horizontal) and center (vertical) of the window
* `"rightcenter"`: Right (horizontal) and center (vertical) of the window
* `"topleft"`: Top left corner of the window
* `"topright"`: Top right corner of the window
* `"bottomleft"`: Bottom left corner of the window
* `"bottomright"`: Bottom right corner of the window

### swayimg.viewer.flip_vertical

```lua
swayimg.viewer.flip_vertical()
```

Flip image vertically.

Since 5.0.

### swayimg.viewer.flip_horizontal

```lua
swayimg.viewer.flip_horizontal()
```

Flip image horizontally.

Since 5.0.

### swayimg.viewer.rotate

```lua
swayimg.viewer.rotate(angle: rotation_t)
```

Rotate image.

Since 5.0.

@_param_ `angle` - Rotation angle

`rotation_t` - Fixed rotation angles for images in viewer and slideshow modes:
* `90`: 90 degrees
* `180`: 180 degrees
* `270`: 270 degrees

### swayimg.viewer.export

```lua
swayimg.viewer.export(path: string)
```

Export currently displayed frame to PNG file.

Since 5.0.

@_param_ `path` - Path to the file

### swayimg.viewer.set_meta

```lua
swayimg.viewer.set_meta(key: string, value: string)
```

Add/replace/remove meta info for currently displayed image.

Since 5.0.

@_param_ `key` - Meta key name

@_param_ `value` - Meta value, empty value to remove the record

### swayimg.viewer.set_window_background

```lua
swayimg.viewer.set_window_background(bkg: color_t|bkgmode_t)
```

Set window background color or extension mode.

Since 5.0.

@_param_ `bkg` - Solid color or one of the predefined mode

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`
`bkgmode_t` - Fixed rotation angles for images in viewer and slideshow modes:
* `"extend"`: Fill window with the current image and blur it
* `"mirror"`: Fill window with the mirrored current image and blur it
* `"auto"`: Fill the window background in `extend` or `mirror` mode depending on the image aspect ratio

### swayimg.viewer.set_image_background

```lua
swayimg.viewer.set_image_background(color: color_t)
```

Set background color for transparent images.

Since 5.0.

This disables chessboard drawing.

@_param_ `color` - Background color

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.viewer.set_image_chessboard

```lua
swayimg.viewer.set_image_chessboard(size: integer, color1: color_t, color2: color_t)
```

Set parameters for chessboard used as background for transparent images.

Since 5.0.

This enables the chessboard if this feature was previously disabled.

@_param_ `size` - Size of single grid cell in pixels

@_param_ `color1` - First color

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

@_param_ `color2` - Second color

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.viewer.mark_image

```lua
swayimg.viewer.mark_image(state?: boolean)
```

Set, clear or toggle mark for currently viewed/selected image.

Since 5.0.

@_param_ `state` - Mark state to set, toggle if the state is not specified

### swayimg.viewer.bind_reset

```lua
swayimg.viewer.bind_reset()
```

Remove all existing key/mouse/signal bindings.

Since 5.0.

### swayimg.viewer.on_key

```lua
swayimg.viewer.on_key(key: string, fn: function)
```

Bind the key press event to a handler.

Since 5.0.

@_param_ `key` - Key description, for example `Ctrl-a`

@_param_ `fn` - Key press handler

### swayimg.viewer.on_mouse

```lua
swayimg.viewer.on_mouse(button: mbutton_t, fn: function)
```

Bind the mouse button press event to a handler.

Since 5.0.

@_param_ `button` - Button description, for example `Ctrl-Alt-MouseRight`

`mbutton_t` - Mouse buttons:
* `"MouseLeft"`: Left mouse button
* `"MouseRight"`: Right mouse button
* `"MouseMiddle"`: Middle mouse button
* `"MouseSide"`: Side mouse button
* `"MouseExtra"`: Extra mouse button
* `"ScrollUp"`: Scroll up
* `"ScrollDown"`: Scroll down
* `"ScrollLeft"`: Scroll left
* `"ScrollRight"`: Scroll right

@_param_ `fn` - Button press handler

### swayimg.viewer.on_signal

```lua
swayimg.viewer.on_signal(signal: string, fn: function)
```

Bind the signal event to a handler.

Since 5.0.

@_param_ `signal` - Signal name (`USR1` or `USR2`)

@_param_ `fn` - Signal handler

### swayimg.viewer.on_image_change

```lua
swayimg.viewer.on_image_change(fn: function|nil)
```

Set a callback function called when a new image is opened/selected.

Since 5.0.

@_param_ `fn` - Handler for notifications about changing the current image

### swayimg.viewer.set_text

```lua
swayimg.viewer.set_text(pos: block_position_t, scheme: text_template_t[])
```

Set text layer scheme.

Since 5.0.

@_param_ `pos` - Text block position

`block_position_t` - Position of text block:
* `"topleft"`: Top left corner of the window
* `"topright"`: Top right corner of the window
* `"bottomleft"`: Bottom left corner of the window
* `"bottomright"`: Bottom right corner of the window

@_param_ `scheme` - Array of line templates with overlay scheme

`text_template_t` - Template for text overlay line:

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
* `{meta.*}`: Image meta info: EXIF, tags etc. List of available EXIF tags
  can be found at [Exiv2 website](https://exiv2.org/tags.html) or printed
  using utility exiv2: `exiv2 -pa photo.jpg`

To print `{` character escape it with `{{`.

The template string may contain a tab character to separate key/value pairs.
In this case, the text block will be aligned with the longest key.
If the value cannot be output (for example, the specified EXIF tag is
missing), then the entire string including the key is ignored upon printing.

Example: `Path to image:\t{path}`

## Slide show mode

### swayimg.slideshow.timeout

```lua
swayimg.slideshow.timeout: number
```

Timeout in seconds after which next image should be opened.

Since 5.5.

### swayimg.slideshow.autocenter

```lua
swayimg.slideshow.autocenter: boolean
```

Automatic image centering.

Since 5.5.

Write-only field.

### swayimg.slideshow.loop

```lua
swayimg.slideshow.loop: boolean
```

Image list loop mode.

Since 5.5.

Write-only field.

### swayimg.slideshow.default_scale

```lua
swayimg.slideshow.default_scale: number
```

|fixed_scale_t.

Default image scale for newly opened images.

Since 5.5.

Write-only field.

### swayimg.slideshow.default_position

```lua
swayimg.slideshow.default_position: fixed_position_t
```

Default image position for newly opened images.

Since 5.5.

Write-only field.

`fixed_position_t` - Fixed position for images in viewer and slideshow modes:
* `"center"`: Vertical and horizontal center of the window
* `"topcenter"`: Top (vertical) and center (horizontal) of the window
* `"bottomcenter"`: Bottom (vertical) and center (horizontal) of the window
* `"leftcenter"`: Left (horizontal) and center (vertical) of the window
* `"rightcenter"`: Right (horizontal) and center (vertical) of the window
* `"topleft"`: Top left corner of the window
* `"topright"`: Top right corner of the window
* `"bottomleft"`: Bottom left corner of the window
* `"bottomright"`: Bottom right corner of the window

### swayimg.slideshow.scale

```lua
swayimg.slideshow.scale: number
```

Absolute scale value (1.0 = 100%).

Since 5.5.

### swayimg.slideshow.animation

```lua
swayimg.slideshow.animation: boolean
```

Stop/resume and get animation status.

Since 5.5.

### swayimg.slideshow.frame

```lua
swayimg.slideshow.frame: integer
```

Currently displayed frame number.

Since 5.5.

Setting this field stops animation.

### swayimg.slideshow.drag_button

```lua
swayimg.slideshow.drag_button: mbutton_t
```

Mouse button used for drag image around the window.

Since 5.5.

Write-only field.

`mbutton_t` - Mouse buttons:
* `"MouseLeft"`: Left mouse button
* `"MouseRight"`: Right mouse button
* `"MouseMiddle"`: Middle mouse button
* `"MouseSide"`: Side mouse button
* `"MouseExtra"`: Extra mouse button
* `"ScrollUp"`: Scroll up
* `"ScrollDown"`: Scroll down
* `"ScrollLeft"`: Scroll left
* `"ScrollRight"`: Scroll right

### swayimg.slideshow.preload

```lua
swayimg.slideshow.preload: integer
```

Max number of images to preload in background thread.

Since 5.5.

Write-only field.

### swayimg.slideshow.history

```lua
swayimg.slideshow.history: integer
```

Max number of previously viewed images stored in the cache.

Since 5.5.

Write-only field.

### swayimg.slideshow.mark_color

```lua
swayimg.slideshow.mark_color: color_t
```

Mark icon color.

Since 5.5.

Write-only field.

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.slideshow.pinch_factor

```lua
swayimg.slideshow.pinch_factor: number
```

Pinch gesture factor.

Since 5.5.

Write-only field.

### swayimg.slideshow.switch_image

```lua
swayimg.slideshow.switch_image(dir: vdir_t)
```

Open the next file in the specified direction.

Since 5.0.

WARNING: This function is deprecated, use `swayimg.viewer.open` instead.

See [swayimg.viewer.open](#swayimgvieweropen).

@_param_ `dir` - Next file direction

`vdir_t` - Direction for opening next file in viewer and slideshow modes:
* `"first"`: First file in image list
* `"last"`: Last file in image list
* `"next"`: Next file
* `"prev"`: Previous file
* `"next_dir"`: First file in next directory
* `"prev_dir"`: Last file in previous directory
* `"random"`: Random file in image list

### swayimg.slideshow.open

```lua
swayimg.slideshow.open(dir: vdir_t) -> boolean
```

Open the next file in the specified direction.

Since 5.5.

@_param_ `dir` - Next file direction

`vdir_t` - Direction for opening next file in viewer and slideshow modes:
* `"first"`: First file in image list
* `"last"`: Last file in image list
* `"next"`: Next file
* `"prev"`: Previous file
* `"next_dir"`: First file in next directory
* `"prev_dir"`: Last file in previous directory
* `"random"`: Random file in image list

@_return_ - True if next file was opened

### swayimg.slideshow.open_path

```lua
swayimg.slideshow.open_path(path: string) -> boolean
```

Open the file at the specified path.

Since 5.5.

This function adds a file to the image list and then opens it in the viewer.

@_param_ `path` - Path to the file

@_return_ - True if file was opened

### swayimg.slideshow.get_image

```lua
swayimg.slideshow.get_image() -> swayimg.image|nil
```

Get information about currently displayed image.

Since 5.0.

@_return_ - Currently displayed image

### swayimg.slideshow.reload

```lua
swayimg.slideshow.reload()
```

Reload current image.

Since 5.1.

### swayimg.slideshow.set_abs_scale

```lua
swayimg.slideshow.set_abs_scale(scale: number, x?: integer, y?: integer)
```

Set absolute image scale.

Since 5.0.

@_param_ `scale` - Absolute value (1.0 = 100%)

@_param_ `x` - X coordinate of center point, empty for window center

@_param_ `y` - Y coordinate of center point, empty for window center

### swayimg.slideshow.set_fix_scale

```lua
swayimg.slideshow.set_fix_scale(scale: fixed_scale_t)
```

Set fixed scale for currently displayed image.

Since 5.0.

@_param_ `scale` - Fixed scale name

`fixed_scale_t` - Fixed scale for images in viewer and slideshow modes:
* `"optimal"`: 100% or less to fit to window
* `"width"`: Fit image width to window width
* `"height"`: Fit image height to window height
* `"fit"`: Fit to window
* `"fill"`: Crop image to fill the window
* `"real"`: Real size (100%)
* `"keep"`: Keep the same scale as for previously viewed image

### swayimg.slideshow.reset

```lua
swayimg.slideshow.reset()
```

Reset position and scale to default values.

Since 5.0.

See [swayimg.viewer.set_default_scale](#swayimgviewerset_default_scale).

See [swayimg.viewer.set_default_position](#swayimgviewerset_default_position).

### swayimg.slideshow.get_position

```lua
swayimg.slideshow.get_position() -> { x :integer, y: integer }
```

Get image position.

Since 5.0.

@_return_ - Image coordinates on the window

### swayimg.slideshow.set_abs_position

```lua
swayimg.slideshow.set_abs_position(x: integer, y: integer)
```

Set absolute image position.

Since 5.0.

@_param_ `x` - Horizontal image position on the window

@_param_ `y` - Vertical image position on the window

### swayimg.slideshow.set_fix_position

```lua
swayimg.slideshow.set_fix_position(pos: fixed_position_t)
```

Set fixed image position.

Since 5.0.

@_param_ `pos` - Fixed image position

`fixed_position_t` - Fixed position for images in viewer and slideshow modes:
* `"center"`: Vertical and horizontal center of the window
* `"topcenter"`: Top (vertical) and center (horizontal) of the window
* `"bottomcenter"`: Bottom (vertical) and center (horizontal) of the window
* `"leftcenter"`: Left (horizontal) and center (vertical) of the window
* `"rightcenter"`: Right (horizontal) and center (vertical) of the window
* `"topleft"`: Top left corner of the window
* `"topright"`: Top right corner of the window
* `"bottomleft"`: Bottom left corner of the window
* `"bottomright"`: Bottom right corner of the window

### swayimg.slideshow.flip_vertical

```lua
swayimg.slideshow.flip_vertical()
```

Flip image vertically.

Since 5.0.

### swayimg.slideshow.flip_horizontal

```lua
swayimg.slideshow.flip_horizontal()
```

Flip image horizontally.

Since 5.0.

### swayimg.slideshow.rotate

```lua
swayimg.slideshow.rotate(angle: rotation_t)
```

Rotate image.

Since 5.0.

@_param_ `angle` - Rotation angle

`rotation_t` - Fixed rotation angles for images in viewer and slideshow modes:
* `90`: 90 degrees
* `180`: 180 degrees
* `270`: 270 degrees

### swayimg.slideshow.export

```lua
swayimg.slideshow.export(path: string)
```

Export currently displayed frame to PNG file.

Since 5.0.

@_param_ `path` - Path to the file

### swayimg.slideshow.set_meta

```lua
swayimg.slideshow.set_meta(key: string, value: string)
```

Add/replace/remove meta info for currently displayed image.

Since 5.0.

@_param_ `key` - Meta key name

@_param_ `value` - Meta value, empty value to remove the record

### swayimg.slideshow.set_window_background

```lua
swayimg.slideshow.set_window_background(bkg: color_t|bkgmode_t)
```

Set window background color or extension mode.

Since 5.0.

@_param_ `bkg` - Solid color or one of the predefined mode

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`
`bkgmode_t` - Fixed rotation angles for images in viewer and slideshow modes:
* `"extend"`: Fill window with the current image and blur it
* `"mirror"`: Fill window with the mirrored current image and blur it
* `"auto"`: Fill the window background in `extend` or `mirror` mode depending on the image aspect ratio

### swayimg.slideshow.set_image_background

```lua
swayimg.slideshow.set_image_background(color: color_t)
```

Set background color for transparent images.

Since 5.0.

This disables chessboard drawing.

@_param_ `color` - Background color

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.slideshow.set_image_chessboard

```lua
swayimg.slideshow.set_image_chessboard(size: integer, color1: color_t, color2: color_t)
```

Set parameters for chessboard used as background for transparent images.

Since 5.0.

This enables the chessboard if this feature was previously disabled.

@_param_ `size` - Size of single grid cell in pixels

@_param_ `color1` - First color

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

@_param_ `color2` - Second color

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.slideshow.mark_image

```lua
swayimg.slideshow.mark_image(state?: boolean)
```

Set, clear or toggle mark for currently viewed/selected image.

Since 5.0.

@_param_ `state` - Mark state to set, toggle if the state is not specified

### swayimg.slideshow.bind_reset

```lua
swayimg.slideshow.bind_reset()
```

Remove all existing key/mouse/signal bindings.

Since 5.0.

### swayimg.slideshow.on_key

```lua
swayimg.slideshow.on_key(key: string, fn: function)
```

Bind the key press event to a handler.

Since 5.0.

@_param_ `key` - Key description, for example `Ctrl-a`

@_param_ `fn` - Key press handler

### swayimg.slideshow.on_mouse

```lua
swayimg.slideshow.on_mouse(button: mbutton_t, fn: function)
```

Bind the mouse button press event to a handler.

Since 5.0.

@_param_ `button` - Button description, for example `Ctrl-Alt-MouseRight`

`mbutton_t` - Mouse buttons:
* `"MouseLeft"`: Left mouse button
* `"MouseRight"`: Right mouse button
* `"MouseMiddle"`: Middle mouse button
* `"MouseSide"`: Side mouse button
* `"MouseExtra"`: Extra mouse button
* `"ScrollUp"`: Scroll up
* `"ScrollDown"`: Scroll down
* `"ScrollLeft"`: Scroll left
* `"ScrollRight"`: Scroll right

@_param_ `fn` - Button press handler

### swayimg.slideshow.on_signal

```lua
swayimg.slideshow.on_signal(signal: string, fn: function)
```

Bind the signal event to a handler.

Since 5.0.

@_param_ `signal` - Signal name (`USR1` or `USR2`)

@_param_ `fn` - Signal handler

### swayimg.slideshow.on_image_change

```lua
swayimg.slideshow.on_image_change(fn: function|nil)
```

Set a callback function called when a new image is opened/selected.

Since 5.0.

@_param_ `fn` - Handler for notifications about changing the current image

### swayimg.slideshow.set_text

```lua
swayimg.slideshow.set_text(pos: block_position_t, scheme: text_template_t[])
```

Set text layer scheme.

Since 5.0.

@_param_ `pos` - Text block position

`block_position_t` - Position of text block:
* `"topleft"`: Top left corner of the window
* `"topright"`: Top right corner of the window
* `"bottomleft"`: Bottom left corner of the window
* `"bottomright"`: Bottom right corner of the window

@_param_ `scheme` - Array of line templates with overlay scheme

`text_template_t` - Template for text overlay line:

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
* `{meta.*}`: Image meta info: EXIF, tags etc. List of available EXIF tags
  can be found at [Exiv2 website](https://exiv2.org/tags.html) or printed
  using utility exiv2: `exiv2 -pa photo.jpg`

To print `{` character escape it with `{{`.

The template string may contain a tab character to separate key/value pairs.
In this case, the text block will be aligned with the longest key.
If the value cannot be output (for example, the specified EXIF tag is
missing), then the entire string including the key is ignored upon printing.

Example: `Path to image:\t{path}`

## Gallery mode

### swayimg.gallery.aspect

```lua
swayimg.gallery.aspect: aspect_t
```

Thumbnail aspect ratio.

Since 5.5.

Write-only field.

`aspect_t` - Aspect ratio used for thumbnails in gallery mode:
* `"fit"`: Fit image into a square thumbnail
* `"fill"`: Fill square thumbnail with the image
* `"keep"`: Adjust thumbnail size to the aspect ratio of the image

### swayimg.gallery.thumb_size

```lua
swayimg.gallery.thumb_size: integer
```

Thumbnail size in pixels.

Since 5.5.

### swayimg.gallery.padding_size

```lua
swayimg.gallery.padding_size: integer
```

Padding size in pixels between thumbnails.

Since 5.5.

Write-only field.

### swayimg.gallery.border_size

```lua
swayimg.gallery.border_size: integer
```

Border size in pixels for currently selected thumbnail.

Since 5.5.

Write-only field.

### swayimg.gallery.selected_scale

```lua
swayimg.gallery.selected_scale: number
```

Scale factor for currently selected thumbnail.

Since 5.5.

Write-only field.

### swayimg.gallery.window_color

```lua
swayimg.gallery.window_color: color_t
```

Background color.

Set window background color.

Since 5.5.

Write-only field.

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.gallery.unselected_color

```lua
swayimg.gallery.unselected_color: color_t
```

Background color for unselected thumbnails.

Since 5.5.

Write-only field.

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.gallery.selected_color

```lua
swayimg.gallery.selected_color: color_t
```

Background color for currently selected thumbnail.

Since 5.5.

Write-only field.

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.gallery.border_color

```lua
swayimg.gallery.border_color: color_t
```

Border color for currently selected thumbnail.

Since 5.5.

Write-only field.

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.gallery.hover

```lua
swayimg.gallery.hover: boolean
```

Change current thumbnail on mouse hover.

Since 5.5.

Write-only field.

### swayimg.gallery.pstore

```lua
swayimg.gallery.pstore: boolean
```

Use persistent storage for thumbnails.

Since 5.5.

Write-only field.

### swayimg.gallery.pstore_path

```lua
swayimg.gallery.pstore_path: string
```

Path for thumbnails persistent storage.

Since 5.5.

Write-only field.

### swayimg.gallery.preload

```lua
swayimg.gallery.preload: boolean
```

Preload invisible thumbnails.

Since 5.5.

Write-only field.

The program preloads thumbnails into the cache up to the amount specified in the `cache` field.

### swayimg.gallery.cache

```lua
swayimg.gallery.cache: integer
```

Max number of invisible thumbnails stored in memory cache.

Since 5.5.

Write-only field.

### swayimg.gallery.embedded_thumb

```lua
swayimg.gallery.embedded_thumb: boolean
```

Use embedded thumbnails.

Since 5.5.

Currently only applicable to RAW images.

### swayimg.gallery.mark_color

```lua
swayimg.gallery.mark_color: color_t
```

Mark icon color.

Since 5.5.

Write-only field.

`color_t` - ARGB color in hex format: AARRGGBB, for example `0xff00aa99`

### swayimg.gallery.pinch_factor

```lua
swayimg.gallery.pinch_factor: number
```

Pinch gesture factor.

Since 5.5.

Write-only field.

### swayimg.gallery.switch_image

```lua
swayimg.gallery.switch_image(dir: gdir_t)
```

Select the next thumbnail from the gallery.

Since 5.0.

WARNING: This function is deprecated, use `swayimg.gallery.select` instead.

See [swayimg.gallery.select_next](#swayimggalleryselect_next).

@_param_ `dir` - Next thumbnail direction

`gdir_t` - Direction for selecting next file in gallery mode:
* `"first"`: Select first thumbnail in image list
* `"last"`: Select last thumbnail in image list
* `"up"`: Select the thumbnail above the current one
* `"down"`: Select the thumbnail below the current one
* `"left"`: Select the thumbnail to the left of the current one
* `"right"`: Select the thumbnail to the right of the current one
* `"pgup"`: Select the thumbnail on the previous page
* `"pgdown"`: Select the thumbnail on the next page

### swayimg.gallery.select

```lua
swayimg.gallery.select(dir: gdir_t) -> boolean
```

Select the next thumbnail from the gallery.

Since 5.5.

@_param_ `dir` - Next thumbnail direction

`gdir_t` - Direction for selecting next file in gallery mode:
* `"first"`: Select first thumbnail in image list
* `"last"`: Select last thumbnail in image list
* `"up"`: Select the thumbnail above the current one
* `"down"`: Select the thumbnail below the current one
* `"left"`: Select the thumbnail to the left of the current one
* `"right"`: Select the thumbnail to the right of the current one
* `"pgup"`: Select the thumbnail on the previous page
* `"pgdown"`: Select the thumbnail on the next page

@_return_ - True if selection was changed

### swayimg.gallery.select_at

```lua
swayimg.gallery.select_at(x: integer, y: integer) -> boolean
```

Select the thumbnail at specified position.

Since 5.5.

@_param_ `x` - X coordinate of the thumbnail

@_param_ `y` - Y coordinate of the thumbnail

@_return_ - True if selection was changed

### swayimg.gallery.select_path

```lua
swayimg.gallery.select_path(path: string) -> boolean
```

Select the thumbnail by image path.

Since 5.5.

@_param_ `path` - Path to the image

@_return_ - True if selection was changed

### swayimg.gallery.reload

```lua
swayimg.gallery.reload()
```

Reload thumbnails.

Since 5.3.

### swayimg.gallery.get_image

```lua
swayimg.gallery.get_image() -> swayimg.entry|nil
```

Get information about currently selected image entry.

Since 5.0.

@_return_ - Currently selected image entry

### swayimg.gallery.mark_image

```lua
swayimg.gallery.mark_image(state?: boolean)
```

Set, clear or toggle mark for currently viewed/selected image.

Since 5.0.

@_param_ `state` - Mark state to set, toggle if the state is not specified

### swayimg.gallery.bind_reset

```lua
swayimg.gallery.bind_reset()
```

Remove all existing key/mouse/signal bindings.

Since 5.0.

### swayimg.gallery.on_key

```lua
swayimg.gallery.on_key(key: string, fn: function)
```

Bind the key press event to a handler.

Since 5.0.

@_param_ `key` - Key description, for example `Ctrl-a`

@_param_ `fn` - Key press handler

### swayimg.gallery.on_mouse

```lua
swayimg.gallery.on_mouse(button: mbutton_t, fn: function)
```

Bind the mouse button press event to a handler.

Since 5.0.

@_param_ `button` - Button description, for example `Ctrl-Alt-MouseRight`

`mbutton_t` - Mouse buttons:
* `"MouseLeft"`: Left mouse button
* `"MouseRight"`: Right mouse button
* `"MouseMiddle"`: Middle mouse button
* `"MouseSide"`: Side mouse button
* `"MouseExtra"`: Extra mouse button
* `"ScrollUp"`: Scroll up
* `"ScrollDown"`: Scroll down
* `"ScrollLeft"`: Scroll left
* `"ScrollRight"`: Scroll right

@_param_ `fn` - Button press handler

### swayimg.gallery.on_signal

```lua
swayimg.gallery.on_signal(signal: string, fn: function)
```

Bind the signal event to a handler.

Since 5.0.

@_param_ `signal` - Signal name (`USR1` or `USR2`)

@_param_ `fn` - Signal handler

### swayimg.gallery.on_image_change

```lua
swayimg.gallery.on_image_change(fn: function|nil)
```

Set a callback function called when a new image is opened/selected.

Since 5.0.

@_param_ `fn` - Handler for notifications about changing the current image

### swayimg.gallery.set_text

```lua
swayimg.gallery.set_text(pos: block_position_t, scheme: text_template_t[])
```

Set text layer scheme.

Since 5.0.

@_param_ `pos` - Text block position

`block_position_t` - Position of text block:
* `"topleft"`: Top left corner of the window
* `"topright"`: Top right corner of the window
* `"bottomleft"`: Bottom left corner of the window
* `"bottomright"`: Bottom right corner of the window

@_param_ `scheme` - Array of line templates with overlay scheme

`text_template_t` - Template for text overlay line:

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
* `{meta.*}`: Image meta info: EXIF, tags etc. List of available EXIF tags
  can be found at [Exiv2 website](https://exiv2.org/tags.html) or printed
  using utility exiv2: `exiv2 -pa photo.jpg`

To print `{` character escape it with `{{`.

The template string may contain a tab character to separate key/value pairs.
In this case, the text block will be aligned with the longest key.
If the value cannot be output (for example, the specified EXIF tag is
missing), then the entire string including the key is ignored upon printing.

Example: `Path to image:\t{path}`
