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
  local image = swayimg.gallery.current_image()
  os.remove(image["path"])
end)
```

A more detailed example can be found on the [project website](extra/example.lua)
or in the file `/usr/share/swayimg/example.lua` after installing the program.

## List of available functions

* General functionality
  * [swayimg.exit](#swayimgexit): Exit from application
  * [swayimg.set_title](#swayimgset_title): Set window title
  * [swayimg.set_status](#swayimgset_status): Show status message
  * [swayimg.set_mode](#swayimgset_mode): Switch to specified mode
  * [swayimg.get_mode](#swayimgget_mode): Get current mode
  * [swayimg.get_window_size](#swayimgget_window_size): Get application window size
  * [swayimg.get_mouse_pos](#swayimgget_mouse_pos): Get mouse pinter coordinates
  * [swayimg.enable_antialiasing](#swayimgenable_antialiasing): Enable or disable antialiasing
  * [swayimg.enable_decoration](#swayimgenable_decoration): Enable or disable window decoration (title, border, buttons)
  * [swayimg.enable_overlay](#swayimgenable_overlay): Enable or disable window overlay mode
* Image list
  * [swayimg.imagelist.size](#swayimgimagelistsize): Get size of image list
  * [swayimg.imagelist.get](#swayimgimagelistget): Get list of all entries in the image list
  * [swayimg.imagelist.add](#swayimgimagelistadd): Add entry to the image list
  * [swayimg.imagelist.remove](#swayimgimagelistremove): Remove entry from the image list
  * [swayimg.imagelist.mark](#swayimgimagelistmark): Set, clear or toggle mark for currently viewed/selected image
  * [swayimg.imagelist.set_order](#swayimgimagelistset_order): Set image list order
  * [swayimg.imagelist.enable_reverse](#swayimgimagelistenable_reverse): Enable or disable reverse order
  * [swayimg.imagelist.enable_recursive](#swayimgimagelistenable_recursive): Enable or disable recursive directory reading
  * [swayimg.imagelist.enable_adjacent](#swayimgimagelistenable_adjacent): Enable or disable opening adjacent files from the same directory
* Text layer
  * [swayimg.text.set_font](#swayimgtextset_font): Set font face
  * [swayimg.text.set_size](#swayimgtextset_size): Set font size
  * [swayimg.text.set_padding](#swayimgtextset_padding): Set the padding from window edges
  * [swayimg.text.set_foreground](#swayimgtextset_foreground): Set foreground text color
  * [swayimg.text.set_background](#swayimgtextset_background): Set background text color
  * [swayimg.text.set_shadow](#swayimgtextset_shadow): Set shadow text color
  * [swayimg.text.set_overall_timer](#swayimgtextset_overall_timer): Set a timeout after which the entire text layer will be hidden
  * [swayimg.text.set_status_timer](#swayimgtextset_status_timer): Set a timeout after which the status message will be hidden
  * [swayimg.text.show](#swayimgtextshow): Show text layer and stop the time
  * [swayimg.text.hide](#swayimgtexthide): Hide text layer and stop the timer
* Viewer mode functions
  * [swayimg.viewer.open](#swayimgvieweropen): Open next file at specified direction
  * [swayimg.viewer.current_image](#swayimgviewercurrent_image): Get information about currently viewed image
  * [swayimg.viewer.get_scale](#swayimgviewerget_scale): Get current image scale
  * [swayimg.viewer.set_abs_scale](#swayimgviewerset_abs_scale): Set absolute image scale
  * [swayimg.viewer.set_fix_scale](#swayimgviewerset_fix_scale): Set fixed image scale
  * [swayimg.viewer.reset_scale](#swayimgviewerreset_scale): Reset scale to default value
  * [swayimg.viewer.set_default_scale](#swayimgviewerset_default_scale): Set default image scale for newly opened images
  * [swayimg.viewer.get_position](#swayimgviewerget_position): Get image position
  * [swayimg.viewer.set_abs_position](#swayimgviewerset_abs_position): Set absolute image position
  * [swayimg.viewer.set_fix_position](#swayimgviewerset_fix_position): Set fixed image position
  * [swayimg.viewer.set_default_position](#swayimgviewerset_default_position): Set default image position for newly opened images
  * [swayimg.viewer.next_frame](#swayimgviewernext_frame): Show next frame
  * [swayimg.viewer.prev_frame](#swayimgviewerprev_frame): Show previous frame
  * [swayimg.viewer.flip_vertical](#swayimgviewerflip_vertical): Flip image vertically
  * [swayimg.viewer.flip_horizontal](#swayimgviewerflip_horizontal): Flip image horizontally
  * [swayimg.viewer.rotate](#swayimgviewerrotate): Rotate image
  * [swayimg.viewer.animation_stop](#swayimgvieweranimation_stop): Stop animation
  * [swayimg.viewer.animation_resume](#swayimgvieweranimation_resume): Resume animation
  * [swayimg.viewer.set_window_background](#swayimgviewerset_window_background): Set window background color and mode
  * [swayimg.viewer.set_image_background](#swayimgviewerset_image_background): Set background color for transparent images
  * [swayimg.viewer.set_image_grid](#swayimgviewerset_image_grid): Set parameters for grid used as background for transparent images
  * [swayimg.viewer.set_mark_color](#swayimgviewerset_mark_color): Set mark icon color
  * [swayimg.viewer.set_meta](#swayimgviewerset_meta): Add/replace/remove meta info for current image
  * [swayimg.viewer.export](#swayimgviewerexport): Export currently viewed frame to PNG file
  * [swayimg.viewer.on_change_image](#swayimgvieweron_change_image): Add a callback function called when a new image is opened
  * [swayimg.viewer.on_key](#swayimgvieweron_key): Bind the key press event to a handler
  * [swayimg.viewer.on_mouse](#swayimgvieweron_mouse): Bind the mouse button press event to a handler
  * [swayimg.viewer.bind_drag](#swayimgviewerbind_drag): Bind the mouse state to image drag operation
  * [swayimg.viewer.on_signal](#swayimgvieweron_signal): Bind the signal event to a handler
  * [swayimg.viewer.bind_reset](#swayimgviewerbind_reset): Remove all existing key/mouse/signal bindings
  * [swayimg.viewer.enable_freemove](#swayimgviewerenable_freemove): Enable or disable free move mode
  * [swayimg.viewer.enable_loop](#swayimgviewerenable_loop): Enable or disable image list loop mode
  * [swayimg.viewer.set_preload_limit](#swayimgviewerset_preload_limit): Set number of images to preload in a separate thread
  * [swayimg.viewer.set_history_limit](#swayimgviewerset_history_limit): Set number of previously viewed images to store in cache
  * [swayimg.viewer.set_text_tl](#swayimgviewerset_text_tl): Set text layer scheme for top left corner of the window
  * [swayimg.viewer.set_text_tr](#swayimgviewerset_text_tr): Set text layer scheme for top right corner of the window
  * [swayimg.viewer.set_text_bl](#swayimgviewerset_text_bl): Set text layer scheme for bottom left corner of the window
  * [swayimg.viewer.set_text_br](#swayimgviewerset_text_br): Set text layer scheme for bottom right corner of the window
* Slide show mode
  * [swayimg.slideshow.set_time](#swayimgslideshowset_time): Set a timeout after which next image should be opened
  * [swayimg.slideshow.open](#swayimgslideshowopen): Open next file at specified direction
  * [swayimg.slideshow.current_image](#swayimgslideshowcurrent_image): Get information about currently viewed image
  * [swayimg.slideshow.get_scale](#swayimgslideshowget_scale): Get current image scale
  * [swayimg.slideshow.set_abs_scale](#swayimgslideshowset_abs_scale): Set absolute image scale
  * [swayimg.slideshow.set_fix_scale](#swayimgslideshowset_fix_scale): Set fixed image scale
  * [swayimg.slideshow.reset_scale](#swayimgslideshowreset_scale): Reset scale to default value
  * [swayimg.slideshow.set_default_scale](#swayimgslideshowset_default_scale): Set default image scale for newly opened images
  * [swayimg.slideshow.get_position](#swayimgslideshowget_position): Get image position
  * [swayimg.slideshow.set_abs_position](#swayimgslideshowset_abs_position): Set absolute image position
  * [swayimg.slideshow.set_fix_position](#swayimgslideshowset_fix_position): Set fixed image position
  * [swayimg.slideshow.set_default_position](#swayimgslideshowset_default_position): Set default image position for newly opened images
  * [swayimg.slideshow.next_frame](#swayimgslideshownext_frame): Show next frame
  * [swayimg.slideshow.prev_frame](#swayimgslideshowprev_frame): Show previous frame
  * [swayimg.slideshow.flip_vertical](#swayimgslideshowflip_vertical): Flip image vertically
  * [swayimg.slideshow.flip_horizontal](#swayimgslideshowflip_horizontal): Flip image horizontally
  * [swayimg.slideshow.rotate](#swayimgslideshowrotate): Rotate image
  * [swayimg.slideshow.animation_stop](#swayimgslideshowanimation_stop): Stop animation
  * [swayimg.slideshow.animation_resume](#swayimgslideshowanimation_resume): Resume animation
  * [swayimg.slideshow.set_window_background](#swayimgslideshowset_window_background): Set window background color and mode
  * [swayimg.slideshow.set_image_background](#swayimgslideshowset_image_background): Set background color for transparent images
  * [swayimg.slideshow.set_image_grid](#swayimgslideshowset_image_grid): Set parameters for grid used as background for transparent images
  * [swayimg.slideshow.set_mark_color](#swayimgslideshowset_mark_color): Set mark icon color
  * [swayimg.slideshow.set_meta](#swayimgslideshowset_meta): Add/replace/remove meta info for current image
  * [swayimg.slideshow.export](#swayimgslideshowexport): Export currently viewed frame to PNG file
  * [swayimg.slideshow.on_change_image](#swayimgslideshowon_change_image): Add a callback function called when a new image is opened
  * [swayimg.slideshow.on_key](#swayimgslideshowon_key): Bind the key press event to a handler
  * [swayimg.slideshow.on_mouse](#swayimgslideshowon_mouse): Bind the mouse button press event to a handler
  * [swayimg.slideshow.bind_drag](#swayimgslideshowbind_drag): Bind the mouse state to image drag operation
  * [swayimg.slideshow.on_signal](#swayimgslideshowon_signal): Bind the signal event to a handler
  * [swayimg.slideshow.bind_reset](#swayimgslideshowbind_reset): Remove all existing key/mouse/signal bindings
  * [swayimg.slideshow.enable_freemove](#swayimgslideshowenable_freemove): Enable or disable free move mode
  * [swayimg.slideshow.enable_loop](#swayimgslideshowenable_loop): Enable or disable image list loop mode
  * [swayimg.slideshow.set_preload_limit](#swayimgslideshowset_preload_limit): Set number of images to preload in a separate thread
  * [swayimg.slideshow.set_history_limit](#swayimgslideshowset_history_limit): Set number of previously viewed images to store in cache
  * [swayimg.slideshow.set_text_tl](#swayimgslideshowset_text_tl): Set text layer scheme for top left corner of the window
  * [swayimg.slideshow.set_text_tr](#swayimgslideshowset_text_tr): Set text layer scheme for top right corner of the window
  * [swayimg.slideshow.set_text_bl](#swayimgslideshowset_text_bl): Set text layer scheme for bottom left corner of the window
  * [swayimg.slideshow.set_text_br](#swayimgslideshowset_text_br): Set text layer scheme for bottom right corner of the window
* Gallery mode
  * [swayimg.gallery.select](#swayimggalleryselect): Select the next thumbnail from the gallery
  * [swayimg.gallery.current_image](#swayimggallerycurrent_image): Get information about currently selected image
  * [swayimg.gallery.set_aspect](#swayimggalleryset_aspect): Set thumbnail aspect ratio
  * [swayimg.gallery.get_thumb_size](#swayimggalleryget_thumb_size): Get thumbnail size
  * [swayimg.gallery.set_thumb_size](#swayimggalleryset_thumb_size): Set thumbnail size
  * [swayimg.gallery.set_padding_size](#swayimggalleryset_padding_size): Set the padding size between thumbnails
  * [swayimg.gallery.set_border_size](#swayimggalleryset_border_size): Set the border size for currently selected thumbnail
  * [swayimg.gallery.set_border_color](#swayimggalleryset_border_color): Set border color for currently selected thumbnail
  * [swayimg.gallery.set_selected_scale](#swayimggalleryset_selected_scale): Set the scale factor for currently selected thumbnail
  * [swayimg.gallery.set_selected_color](#swayimggalleryset_selected_color): Set background color for currently selected thumbnail
  * [swayimg.gallery.set_background_color](#swayimggalleryset_background_color): Set background color for unselected thumbnails
  * [swayimg.gallery.set_window_color](#swayimggalleryset_window_color): Set window background color
  * [swayimg.gallery.set_mark_color](#swayimggalleryset_mark_color): Set mark icon color
  * [swayimg.gallery.on_change_image](#swayimggalleryon_change_image): Add a callback function called when a new image is selected
  * [swayimg.gallery.on_key](#swayimggalleryon_key): Bind the key press event to a handler
  * [swayimg.gallery.on_mouse](#swayimggalleryon_mouse): Bind the mouse button press event to a handler
  * [swayimg.gallery.on_signal](#swayimggalleryon_signal): Bind the signal event to a handler
  * [swayimg.gallery.bind_reset](#swayimggallerybind_reset): Remove all existing key/mouse/signal bindings
  * [swayimg.gallery.set_cache_size](#swayimggalleryset_cache_size): Set max number of thumbnails stored in memory cache
  * [swayimg.gallery.enable_preload](#swayimggalleryenable_preload): Enable or disable preloading invisible thumbnails
  * [swayimg.gallery.enable_pstore](#swayimggalleryenable_pstore): Enable or disable persistent storage for thumbnails
  * [swayimg.gallery.set_pstore_path](#swayimggalleryset_pstore_path): Set custom path for persistent storage for thumbnails

## General functionality

### swayimg.exit

```lua
swayimg.exit(code?: number)
```

Exit from application.

@*param* `code` — Program exit code, `0` by default

### swayimg.set_title

```lua
swayimg.set_title(title: string)
```

Set window title.

@*param* `title` — Window title text

### swayimg.set_status

```lua
swayimg.set_status(status: string)
```

Show status message.

@*param* `status` — Status text to show

### swayimg.set_mode

```lua
swayimg.set_mode(mode: "gallery"|"slideshow"|"viewer")
```

Switch to specified mode.

@*param* `mode` — Mode to activate

```lua
mode:
    | "viewer" -- Image viewer mode
    | "slideshow" -- Slide show mode
    | "gallery" -- Gallery mode
```

### swayimg.get_mode

```lua
swayimg.get_mode() -> "gallery"|"slideshow"|"viewer"
```

Get current mode.

@*return* — Currently active mode

```lua
return #1:
    | "viewer" -- Image viewer mode
    | "slideshow" -- Slide show mode
    | "gallery" -- Gallery mode
```

### swayimg.get_window_size

```lua
swayimg.get_window_size() -> table
```

Get application window size.

@*return* — (width, height) tuple with window size in pixels

### swayimg.get_mouse_pos

```lua
swayimg.get_mouse_pos() -> table
```

Get mouse pinter coordinates.

@*return* — (x, y) tuple with mouse pointer coordinates

### swayimg.enable_antialiasing

```lua
swayimg.enable_antialiasing(enable: boolean)
```

Enable or disable antialiasing.

@*param* `enable` — Enable/disable flag to set

### swayimg.enable_decoration

```lua
swayimg.enable_decoration(enable: boolean)
```

Enable or disable window decoration (title, border, buttons).
This function available only in Wayland, the corresponding protocol must be
supported by the composer.
By default disabled in Sway and enabled in other compositors.

@*param* `enable` — Enable/disable flag to set

### swayimg.enable_overlay

```lua
swayimg.enable_overlay(enable: boolean)
```

Enable or disable window overlay mode.
Sway and Hyprland compositors only.
Create a floating window with the same coordinates and size as the currently
focused window. This variable can be set only once.
By default enabled in Sway and disabled in other compositors.

@*param* `enable` — Enable/disable flag to set

## Image list

### swayimg.imagelist.size

```lua
swayimg.imagelist.size() -> number
```

Get size of image list.

@*return* — Number of entries in the image list

### swayimg.imagelist.get

```lua
swayimg.imagelist.get() -> table[]
```

Get list of all entries in the image list.

@*return* — Array with all entries

### swayimg.imagelist.add

```lua
swayimg.imagelist.add(path: string)
```

Add entry to the image list.

@*param* `path` — Path to add

### swayimg.imagelist.remove

```lua
swayimg.imagelist.remove(path: string)
```

Remove entry from the image list.

@*param* `path` — Path to remove

### swayimg.imagelist.mark

```lua
swayimg.imagelist.mark(state?: boolean)
```

Set, clear or toggle mark for currently viewed/selected image.

@*param* `state` — Mark state to set, toggle if the state is not specified

See:
  * [swayimg.viewer.set_mark_color](#swayimgviewerset_mark_color)
  * [swayimg.slideshow.set_mark_color](#swayimgslideshowset_mark_color)
  * [swayimg.gallery.set_mark_color](#swayimggalleryset_mark_color)

### swayimg.imagelist.set_order

```lua
swayimg.imagelist.set_order(order: "alpha"|"mtime"|"none"|"numeric"|"random"...(+1))
```

Set image list order.

@*param* `order` — List order

```lua
order:
    | "none" -- Unsorted (system depended)
    | "alpha" -- Lexicographic sort: 1,10,2,20,a,b,c
    | "numeric" -- Numeric sort: 1,2,3,10,100,a,b,c
    | "mtime" -- Modification time sort
    | "size" -- Size sort
    | "random" -- Random order
```

### swayimg.imagelist.enable_reverse

```lua
swayimg.imagelist.enable_reverse(enable: boolean)
```

Enable or disable reverse order.

@*param* `enable` — Enable/disable flag to set

### swayimg.imagelist.enable_recursive

```lua
swayimg.imagelist.enable_recursive(enable: boolean)
```

Enable or disable recursive directory reading.

@*param* `enable` — Enable/disable flag to set

### swayimg.imagelist.enable_adjacent

```lua
swayimg.imagelist.enable_adjacent(enable: boolean)
```

Enable or disable opening adjacent files from the same directory.

@*param* `enable` — Enable/disable flag to set

## Text layer

### swayimg.text.set_font

```lua
swayimg.text.set_font(name: string)
```

Set font face.

@*param* `name` — Font name

### swayimg.text.set_size

```lua
swayimg.text.set_size(size: number)
```

Set font size.

@*param* `size` — Font size in pixels

### swayimg.text.set_padding

```lua
swayimg.text.set_padding(size: number)
```

Set the padding from window edges.

@*param* `size` — Padding size in pixels

### swayimg.text.set_foreground

```lua
swayimg.text.set_foreground(color: number)
```

Set foreground text color.

@*param* `color` — Color in ARGB format, e.g. `0xff00aa99`

### swayimg.text.set_background

```lua
swayimg.text.set_background(color: number)
```

Set background text color.

@*param* `color` — Color in ARGB format, e.g. `0xff00aa99`

### swayimg.text.set_shadow

```lua
swayimg.text.set_shadow(color: number)
```

Set shadow text color.

@*param* `color` — Color in ARGB format, e.g. `0xff00aa99`

### swayimg.text.set_overall_timer

```lua
swayimg.text.set_overall_timer(seconds: number)
```

Set a timeout after which the entire text layer will be hidden.

@*param* `seconds` — Timeout in seconds

### swayimg.text.set_status_timer

```lua
swayimg.text.set_status_timer(seconds: number)
```

Set a timeout after which the status message will be hidden.

@*param* `seconds` — Timeout in seconds

### swayimg.text.show

```lua
swayimg.text.show()
```

Show text layer and stop the time.

### swayimg.text.hide

```lua
swayimg.text.hide()
```

Hide text layer and stop the timer.

## Viewer mode functions

### swayimg.viewer.open

```lua
swayimg.viewer.open(dir: "first"|"last"|"next"|"next_dir"|"prev"...(+2))
```

Open next file at specified direction.

@*param* `dir` — Next file direction

```lua
dir:
    | "first" -- First file in image list
    | "last" -- Last file in image list
    | "next" -- Next file
    | "prev" -- Previous file
    | "next_dir" -- First file in next directory
    | "prev_dir" -- Last file in previous directory
    | "random" -- Random file in image list
```

### swayimg.viewer.current_image

```lua
swayimg.viewer.current_image() -> table
```

Get information about currently viewed image.

@*return* — Dictionary with image properties: path, size, meta data, etc

### swayimg.viewer.get_scale

```lua
swayimg.viewer.get_scale() -> number
```

Get current image scale.

@*return* — Absolute scale value (1.0 = 100%)

### swayimg.viewer.set_abs_scale

```lua
swayimg.viewer.set_abs_scale(scale: number, x?: number, y?: number)
```

Set absolute image scale.

@*param* `scale` — Absolute value (1.0 = 100%)

@*param* `x` — X coordinate of center point, empty for window center

@*param* `y` — Y coordinate of center point, empty for window center

### swayimg.viewer.set_fix_scale

```lua
swayimg.viewer.set_fix_scale(scale: "fill"|"fit"|"height"|"keep"|"optimal"...(+2))
```

Set fixed image scale.

@*param* `scale` — Fixed scale name

```lua
scale:
    | "optimal" -- 100% or less to fit to window
    | "width" -- Fit image width to window width
    | "height" -- Fit image height to window height
    | "fit" -- Fit to window
    | "fill" -- Crop image to fill the window
    | "real" -- Real size (100%)
    | "keep" -- Keep the same scale as for previously viewed image
```

### swayimg.viewer.reset_scale

```lua
swayimg.viewer.reset_scale()
```

Reset scale to default value.
See: [swayimg.viewer.set_default_scale](#swayimgviewerset_default_scale)

### swayimg.viewer.set_default_scale

```lua
swayimg.viewer.set_default_scale(scale: number|"fill"|"fit"|"height"|"keep"...(+3))
```

Set default image scale for newly opened images.

@*param* `scale` — Absolute value (1.0 = 100%) or one the predefined names

```lua
scale:
    | "optimal" -- 100% or less to fit to window
    | "width" -- Fit image width to window width
    | "height" -- Fit image height to window height
    | "fit" -- Fit to window
    | "fill" -- Crop image to fill the window
    | "real" -- Real size (100%)
    | "keep" -- Keep the same scale as for previously viewed image
```

### swayimg.viewer.get_position

```lua
swayimg.viewer.get_position() -> table
```

Get image position.

@*return* — (x, y) tuple with image coordinates on the window

### swayimg.viewer.set_abs_position

```lua
swayimg.viewer.set_abs_position(x: number, y: number)
```

Set absolute image position.

@*param* `x` — Horizontal image position on the window

@*param* `y` — Vertical image position on the window

### swayimg.viewer.set_fix_position

```lua
swayimg.viewer.set_fix_position(pos: "bottomcenter"|"bottomleft"|"bottomright"|"center"|"leftcenter"...(+4))
```

Set fixed image position.

@*param* `pos` — Fixed image position

```lua
pos:
    | "center" -- Vertical and horizontal center of the window
    | "topcenter" -- Top (vertical) and center (horizontal) of the window
    | "bottomcenter" -- Bottom (vertical) and center (horizontal) of the window
    | "leftcenter" -- Left (horizontal) and center (vertical) of the window
    | "rightcenter" -- Right (horizontal) and center (vertical) of the window
    | "topleft" -- Top left corner of the window
    | "topright" -- Top right corner of the window
    | "bottomleft" -- Bottom left corner of the window
    | "bottomright" -- Bottom right corner of the window
```

### swayimg.viewer.set_default_position

```lua
swayimg.viewer.set_default_position(pos: "bottomcenter"|"bottomleft"|"bottomright"|"center"|"leftcenter"...(+4))
```

Set default image position for newly opened images.

@*param* `pos` — Fixed image position

```lua
pos:
    | "center" -- Vertical and horizontal center of the window
    | "topcenter" -- Top (vertical) and center (horizontal) of the window
    | "bottomcenter" -- Bottom (vertical) and center (horizontal) of the window
    | "leftcenter" -- Left (horizontal) and center (vertical) of the window
    | "rightcenter" -- Right (horizontal) and center (vertical) of the window
    | "topleft" -- Top left corner of the window
    | "topright" -- Top right corner of the window
    | "bottomleft" -- Bottom left corner of the window
    | "bottomright" -- Bottom right corner of the window
```

### swayimg.viewer.next_frame

```lua
swayimg.viewer.next_frame() -> number
```

Show next frame.
This function also stops the animation.

@*return* — Index of the currently shown frame

See:
  * [swayimg.viewer.animation_stop](#swayimgvieweranimation_stop)
  * [swayimg.viewer.animation_resume](#swayimgvieweranimation_resume)

### swayimg.viewer.prev_frame

```lua
swayimg.viewer.prev_frame() -> number
```

Show previous frame.
This function also stops the animation.

@*return* — Index of the currently shown frame

See:
  * [swayimg.viewer.animation_stop](#swayimgvieweranimation_stop)
  * [swayimg.viewer.animation_resume](#swayimgvieweranimation_resume)

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
swayimg.viewer.rotate(angle: 180|270|90)
```

Rotate image.

@*param* `angle` — Rotation angle

```lua
angle:
    | 90 -- 90 degrees
    | 180 -- 180 degrees
    | 270 -- 270 degrees
```

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

### swayimg.viewer.set_window_background

```lua
swayimg.viewer.set_window_background(bkg: number|"auto"|"extend"|"mirror")
```

Set window background color and mode.

@*param* `bkg` — Solid color or one of the predefined mode

```lua
bkg:
    | "extend" -- Fill window with the current image and blur it
    | "mirror" -- Fill window with the mirrored current image and blur it
    | "auto" -- Fill the window background in `extend` or `mirror` mode depending on the image aspect ratio
```

### swayimg.viewer.set_image_background

```lua
swayimg.viewer.set_image_background(color: number)
```

Set background color for transparent images.
This disables grid drawing.

@*param* `color` — Color in ARGB format, e.g. `0xff00aa99`

### swayimg.viewer.set_image_grid

```lua
swayimg.viewer.set_image_grid(size: number, color1: number, color2: number)
```

Set parameters for grid used as background for transparent images.

@*param* `size` — Size of single grid cell in pixels

@*param* `color1` — First color in ARGB format, e.g. `0xff00aa99`

@*param* `color2` — Second color in ARGB format, e.g. `0xff00aa99`

### swayimg.viewer.set_mark_color

```lua
swayimg.viewer.set_mark_color(color: number)
```

Set mark icon color.

@*param* `color` — Color in ARGB format, e.g. `0xff00aa99`

See: [swayimg.imagelist.mark](#swayimgimagelistmark)

### swayimg.viewer.set_meta

```lua
swayimg.viewer.set_meta(key: string, value: string)
```

Add/replace/remove meta info for current image.

@*param* `key` — Meta key name

@*param* `value` — Meta value, empty value to remove

### swayimg.viewer.export

```lua
swayimg.viewer.export(path: string)
```

Export currently viewed frame to PNG file.

@*param* `path` — Path to file

### swayimg.viewer.on_change_image

```lua
swayimg.viewer.on_change_image(fn: function)
```

Add a callback function called when a new image is opened.

@*param* `fn` — Callback handler

### swayimg.viewer.on_key

```lua
swayimg.viewer.on_key(key: string, fn: function)
```

Bind the key press event to a handler.

@*param* `key` — Key description, for example `Ctrl-a`

@*param* `fn` — Key press handler

### swayimg.viewer.on_mouse

```lua
swayimg.viewer.on_mouse(button: string, fn: function)
```

Bind the mouse button press event to a handler.

@*param* `button` — Button description, for example `Ctrl-Alt-MouseRight`

@*param* `fn` — Button press handler

### swayimg.viewer.bind_drag

```lua
swayimg.viewer.bind_drag(button: string)
```

Bind the mouse state to image drag operation.

@*param* `button` — Button description, for example `MouseLeft`

### swayimg.viewer.on_signal

```lua
swayimg.viewer.on_signal(signal: string, fn: function)
```

Bind the signal event to a handler.

@*param* `signal` — Signal name: `USR1` or `USR2`

@*param* `fn` — Signal handler

### swayimg.viewer.bind_reset

```lua
swayimg.viewer.bind_reset()
```

Remove all existing key/mouse/signal bindings.

### swayimg.viewer.enable_freemove

```lua
swayimg.viewer.enable_freemove(enable: boolean)
```

Enable or disable free move mode.

@*param* `enable` — Enable/disable flag to set

### swayimg.viewer.enable_loop

```lua
swayimg.viewer.enable_loop(enable: boolean)
```

Enable or disable image list loop mode.

@*param* `enable` — Enable/disable flag to set

### swayimg.viewer.set_preload_limit

```lua
swayimg.viewer.set_preload_limit(size: number)
```

Set number of images to preload in a separate thread.

@*param* `size` — Number of images to preload

### swayimg.viewer.set_history_limit

```lua
swayimg.viewer.set_history_limit(size: number)
```

Set number of previously viewed images to store in cache.

@*param* `size` — Number of images to store

### swayimg.viewer.set_text_tl

```lua
swayimg.viewer.set_text_tl(scheme: string[])
```

Set text layer scheme for top left corner of the window.

@*param* `scheme` — Array with text overlay scheme

Each string in array is a printable template. The template includes text and
fields surrounded by curly braces. The following fields are supported:
* `{name}`: File name of the currently viewed/selected image;
* `{dir}`: Parent directory name of the currently viewed/selected image;
* `{path}`: Absolute path to the currently viewed/selected image;
* `{size}`: File size in bytes;
* `{sizehr}`: File size in human-readable format;
* `{time}`: File modification time;
* `{format}`: Brief image format description;
* `{scale}`: Current image scale in percent;
* `{list.index}`: Current index of image in the image list;
* `{list.total}`: Total number of files in the image list;
* `{frame.width}`: Current frame width in pixels;
* `{frame.height}`: Current frame height in pixels;
* `{meta.*}`: Image meta info: EXIF, tags etc.

### swayimg.viewer.set_text_tr

```lua
swayimg.viewer.set_text_tr(scheme: string[])
```

Set text layer scheme for top right corner of the window.

@*param* `scheme` — Text overlay scheme

See: [swayimg.viewer.set_text_tl](#swayimgviewerset_text_tl) for scheme description

### swayimg.viewer.set_text_bl

```lua
swayimg.viewer.set_text_bl(scheme: string[])
```

Set text layer scheme for bottom left corner of the window.

@*param* `scheme` — Text overlay scheme

See: [swayimg.viewer.set_text_tl](#swayimgviewerset_text_tl) for scheme description

### swayimg.viewer.set_text_br

```lua
swayimg.viewer.set_text_br(scheme: string[])
```

Set text layer scheme for bottom right corner of the window.

@*param* `scheme` — Text overlay scheme

See: [swayimg.viewer.set_text_tl](#swayimgviewerset_text_tl) for scheme description

## Slide show mode

### swayimg.slideshow.set_time

```lua
swayimg.slideshow.set_time(seconds: number)
```

Set a timeout after which next image should be opened.

@*param* `seconds` — Timeout in seconds

### swayimg.slideshow.open

```lua
swayimg.slideshow.open(dir: "first"|"last"|"next"|"next_dir"|"prev"...(+2))
```

Open next file at specified direction.

@*param* `dir` — Next file direction

```lua
dir:
    | "first" -- First file in image list
    | "last" -- Last file in image list
    | "next" -- Next file
    | "prev" -- Previous file
    | "next_dir" -- First file in next directory
    | "prev_dir" -- Last file in previous directory
    | "random" -- Random file in image list
```

### swayimg.slideshow.current_image

```lua
swayimg.slideshow.current_image() -> table
```

Get information about currently viewed image.

@*return* — Dictionary with image properties: path, size, meta data, etc

### swayimg.slideshow.get_scale

```lua
swayimg.slideshow.get_scale() -> number
```

Get current image scale.

@*return* — Absolute scale value (1.0 = 100%)

### swayimg.slideshow.set_abs_scale

```lua
swayimg.slideshow.set_abs_scale(scale: number, x?: number, y?: number)
```

Set absolute image scale.

@*param* `scale` — Absolute value (1.0 = 100%)

@*param* `x` — X coordinate of center point, empty for window center

@*param* `y` — Y coordinate of center point, empty for window center

### swayimg.slideshow.set_fix_scale

```lua
swayimg.slideshow.set_fix_scale(scale: "fill"|"fit"|"height"|"keep"|"optimal"...(+2))
```

Set fixed image scale.

@*param* `scale` — Fixed scale name

```lua
scale:
    | "optimal" -- 100% or less to fit to window
    | "width" -- Fit image width to window width
    | "height" -- Fit image height to window height
    | "fit" -- Fit to window
    | "fill" -- Crop image to fill the window
    | "real" -- Real size (100%)
    | "keep" -- Keep the same scale as for previously viewed image
```

### swayimg.slideshow.reset_scale

```lua
swayimg.slideshow.reset_scale()
```

Reset scale to default value.
See: [swayimg.slideshow.set_default_scale](#swayimgslideshowset_default_scale)

### swayimg.slideshow.set_default_scale

```lua
swayimg.slideshow.set_default_scale(scale: number|"fill"|"fit"|"height"|"keep"...(+3))
```

Set default image scale for newly opened images.

@*param* `scale` — Absolute value (1.0 = 100%) or one the predefined names

```lua
scale:
    | "optimal" -- 100% or less to fit to window
    | "width" -- Fit image width to window width
    | "height" -- Fit image height to window height
    | "fit" -- Fit to window
    | "fill" -- Crop image to fill the window
    | "real" -- Real size (100%)
    | "keep" -- Keep the same scale as for previously viewed image
```

### swayimg.slideshow.get_position

```lua
swayimg.slideshow.get_position() -> table
```

Get image position.

@*return* — (x, y) tuple with image coordinates on the window

### swayimg.slideshow.set_abs_position

```lua
swayimg.slideshow.set_abs_position(x: number, y: number)
```

Set absolute image position.

@*param* `x` — Horizontal image position on the window

@*param* `y` — Vertical image position on the window

### swayimg.slideshow.set_fix_position

```lua
swayimg.slideshow.set_fix_position(pos: "bottomcenter"|"bottomleft"|"bottomright"|"center"|"leftcenter"...(+4))
```

Set fixed image position.

@*param* `pos` — Fixed image position

```lua
pos:
    | "center" -- Vertical and horizontal center of the window
    | "topcenter" -- Top (vertical) and center (horizontal) of the window
    | "bottomcenter" -- Bottom (vertical) and center (horizontal) of the window
    | "leftcenter" -- Left (horizontal) and center (vertical) of the window
    | "rightcenter" -- Right (horizontal) and center (vertical) of the window
    | "topleft" -- Top left corner of the window
    | "topright" -- Top right corner of the window
    | "bottomleft" -- Bottom left corner of the window
    | "bottomright" -- Bottom right corner of the window
```

### swayimg.slideshow.set_default_position

```lua
swayimg.slideshow.set_default_position(pos: "bottomcenter"|"bottomleft"|"bottomright"|"center"|"leftcenter"...(+4))
```

Set default image position for newly opened images.

@*param* `pos` — Fixed image position

```lua
pos:
    | "center" -- Vertical and horizontal center of the window
    | "topcenter" -- Top (vertical) and center (horizontal) of the window
    | "bottomcenter" -- Bottom (vertical) and center (horizontal) of the window
    | "leftcenter" -- Left (horizontal) and center (vertical) of the window
    | "rightcenter" -- Right (horizontal) and center (vertical) of the window
    | "topleft" -- Top left corner of the window
    | "topright" -- Top right corner of the window
    | "bottomleft" -- Bottom left corner of the window
    | "bottomright" -- Bottom right corner of the window
```

### swayimg.slideshow.next_frame

```lua
swayimg.slideshow.next_frame() -> number
```

Show next frame.

@*return* — Index of the currently shown frame

See:
  * [swayimg.slideshow.animation_stop](#swayimgslideshowanimation_stop)
  * [swayimg.slideshow.animation_resume](#swayimgslideshowanimation_resume)

### swayimg.slideshow.prev_frame

```lua
swayimg.slideshow.prev_frame() -> number
```

Show previous frame.

@*return* — Index of the currently shown frame

See:
  * [swayimg.slideshow.animation_stop](#swayimgslideshowanimation_stop)
  * [swayimg.slideshow.animation_resume](#swayimgslideshowanimation_resume)

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
swayimg.slideshow.rotate(angle: 180|270|90)
```

Rotate image.

@*param* `angle` — Rotation angle

```lua
angle:
    | 90 -- 90 degrees
    | 180 -- 180 degrees
    | 270 -- 270 degrees
```

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

### swayimg.slideshow.set_window_background

```lua
swayimg.slideshow.set_window_background(bkg: number|"auto"|"extend"|"mirror")
```

Set window background color and mode.

@*param* `bkg` — Solid color or one of the predefined mode

```lua
bkg:
    | "extend" -- Fill window with the current image and blur it
    | "mirror" -- Fill window with the mirrored current image and blur it
    | "auto" -- Fill the window background in `extend` or `mirror` mode depending on the image aspect ratio
```

### swayimg.slideshow.set_image_background

```lua
swayimg.slideshow.set_image_background(color: number)
```

Set background color for transparent images.
This disables grid drawing.

@*param* `color` — Color in ARGB format, e.g. `0xff00aa99`

### swayimg.slideshow.set_image_grid

```lua
swayimg.slideshow.set_image_grid(size: number, color1: number, color2: number)
```

Set parameters for grid used as background for transparent images.

@*param* `size` — Size of single grid cell in pixels

@*param* `color1` — First color in ARGB format, e.g. `0xff00aa99`

@*param* `color2` — Second color in ARGB format, e.g. `0xff00aa99`

### swayimg.slideshow.set_mark_color

```lua
swayimg.slideshow.set_mark_color(color: number)
```

Set mark icon color.

@*param* `color` — Color in ARGB format, e.g. `0xff00aa99`

See: [swayimg.imagelist.mark](#swayimgimagelistmark)

### swayimg.slideshow.set_meta

```lua
swayimg.slideshow.set_meta(key: string, value: string)
```

Add/replace/remove meta info for current image.

@*param* `key` — Meta key name

@*param* `value` — Meta value, empty value to remove

### swayimg.slideshow.export

```lua
swayimg.slideshow.export(path: string)
```

Export currently viewed frame to PNG file.

@*param* `path` — Path to file

### swayimg.slideshow.on_change_image

```lua
swayimg.slideshow.on_change_image(fn: function)
```

Add a callback function called when a new image is opened.

@*param* `fn` — Callback handler

### swayimg.slideshow.on_key

```lua
swayimg.slideshow.on_key(key: string, fn: function)
```

Bind the key press event to a handler.

@*param* `key` — Key description, for example `Ctrl-a`

@*param* `fn` — Key press handler

### swayimg.slideshow.on_mouse

```lua
swayimg.slideshow.on_mouse(button: string, fn: function)
```

Bind the mouse button press event to a handler.

@*param* `button` — Button description, for example `Ctrl-Alt-MouseRight`

@*param* `fn` — Button press handler

### swayimg.slideshow.bind_drag

```lua
swayimg.slideshow.bind_drag(button: string)
```

Bind the mouse state to image drag operation.

@*param* `button` — Button description, for example `MouseLeft`

### swayimg.slideshow.on_signal

```lua
swayimg.slideshow.on_signal(signal: string, fn: function)
```

Bind the signal event to a handler.

@*param* `signal` — Signal name: `USR1` or `USR2`

@*param* `fn` — Signal handler

### swayimg.slideshow.bind_reset

```lua
swayimg.slideshow.bind_reset()
```

Remove all existing key/mouse/signal bindings.

### swayimg.slideshow.enable_freemove

```lua
swayimg.slideshow.enable_freemove(enable: boolean)
```

Enable or disable free move mode.

@*param* `enable` — Enable/disable flag to set

### swayimg.slideshow.enable_loop

```lua
swayimg.slideshow.enable_loop(enable: boolean)
```

Enable or disable image list loop mode.

@*param* `enable` — Enable/disable flag to set

### swayimg.slideshow.set_preload_limit

```lua
swayimg.slideshow.set_preload_limit(size: number)
```

Set number of images to preload in a separate thread.

@*param* `size` — Number of images to preload

### swayimg.slideshow.set_history_limit

```lua
swayimg.slideshow.set_history_limit(size: number)
```

Set number of previously viewed images to store in cache.

@*param* `size` — Number of images to store

### swayimg.slideshow.set_text_tl

```lua
swayimg.slideshow.set_text_tl(scheme: string[])
```

Set text layer scheme for top left corner of the window.

@*param* `scheme` — Array with text overlay scheme

See: [swayimg.viewer.set_text_tl](#swayimgviewerset_text_tl) for scheme description

### swayimg.slideshow.set_text_tr

```lua
swayimg.slideshow.set_text_tr(scheme: string[])
```

Set text layer scheme for top right corner of the window.

@*param* `scheme` — Text overlay scheme

See: [swayimg.viewer.set_text_tl](#swayimgviewerset_text_tl) for scheme description

### swayimg.slideshow.set_text_bl

```lua
swayimg.slideshow.set_text_bl(scheme: string[])
```

Set text layer scheme for bottom left corner of the window.

@*param* `scheme` — Text overlay scheme

See: [swayimg.viewer.set_text_tl](#swayimgviewerset_text_tl) for scheme description

### swayimg.slideshow.set_text_br

```lua
swayimg.slideshow.set_text_br(scheme: string[])
```

Set text layer scheme for bottom right corner of the window.

@*param* `scheme` — Text overlay scheme

See: [swayimg.viewer.set_text_tl](#swayimgviewerset_text_tl) for scheme description

## Gallery mode

### swayimg.gallery.select

```lua
swayimg.gallery.select(dir: "down"|"first"|"last"|"left"|"pgdown"...(+3))
```

Select the next thumbnail from the gallery.

@*param* `dir` — Next thumbnail direction

```lua
dir:
    | "first" -- Select first thumbnail in image list
    | "last" -- Select last thumbnail in image list
    | "up" -- Select the thumbnail above the current one
    | "down" -- Select the thumbnail below the current one
    | "left" -- Select the thumbnail to the left of the current one
    | "right" -- Select the thumbnail to the right of the current one
    | "pgup" -- Select the thumbnail on the previous page
    | "pgdown" -- Select the thumbnail on the next page
```

### swayimg.gallery.current_image

```lua
swayimg.gallery.current_image() -> table
```

Get information about currently selected image.

@*return* — Dictionary with image properties: path, size, etc

### swayimg.gallery.set_aspect

```lua
swayimg.gallery.set_aspect(aspect: "fill"|"fit"|"keep")
```

Set thumbnail aspect ratio.

@*param* `aspect` — Thumbnail aspect ratio

```lua
aspect:
    | "fit" -- Fit image into a square thumbnail
    | "fill" -- Fill square thumbnail with the image
    | "keep" -- Adjust thumbnail size to the aspect ratio of the image
```

### swayimg.gallery.get_thumb_size

```lua
swayimg.gallery.get_thumb_size() -> number
```

Get thumbnail size.

@*return* — Thumbnail size in pixels

### swayimg.gallery.set_thumb_size

```lua
swayimg.gallery.set_thumb_size(size: number)
```

Set thumbnail size.

@*param* `size` — Thumbnail size in pixels

### swayimg.gallery.set_padding_size

```lua
swayimg.gallery.set_padding_size(size: number)
```

Set the padding size between thumbnails.

@*param* `size` — Padding size in pixels

### swayimg.gallery.set_border_size

```lua
swayimg.gallery.set_border_size(size: number)
```

Set the border size for currently selected thumbnail.

@*param* `size` — Border size in pixels

### swayimg.gallery.set_border_color

```lua
swayimg.gallery.set_border_color(color: number)
```

Set border color for currently selected thumbnail.

@*param* `color` — Color in ARGB format, e.g. `0xff00aa99`

### swayimg.gallery.set_selected_scale

```lua
swayimg.gallery.set_selected_scale(scale: number)
```

Set the scale factor for currently selected thumbnail.

@*param* `scale` — Scale factor, 1.0 = 100%

### swayimg.gallery.set_selected_color

```lua
swayimg.gallery.set_selected_color(color: number)
```

Set background color for currently selected thumbnail.

@*param* `color` — Color in ARGB format, e.g. `0xff00aa99`

### swayimg.gallery.set_background_color

```lua
swayimg.gallery.set_background_color(color: number)
```

Set background color for unselected thumbnails.

@*param* `color` — Color in ARGB format, e.g. `0xff00aa99`

### swayimg.gallery.set_window_color

```lua
swayimg.gallery.set_window_color(color: number)
```

Set window background color.

@*param* `color` — Color in ARGB format, e.g. `0xff00aa99`

### swayimg.gallery.set_mark_color

```lua
swayimg.gallery.set_mark_color(color: number)
```

Set mark icon color.

@*param* `color` — Color in ARGB format, e.g. `0xff00aa99`

See: [swayimg.imagelist.mark](#swayimgimagelistmark)

### swayimg.gallery.on_change_image

```lua
swayimg.gallery.on_change_image(fn: function)
```

Add a callback function called when a new image is selected.

@*param* `fn` — Callback handler

### swayimg.gallery.on_key

```lua
swayimg.gallery.on_key(key: string, fn: function)
```

Bind the key press event to a handler.

@*param* `key` — Key description, for example `Ctrl-a`

@*param* `fn` — Key press handler

### swayimg.gallery.on_mouse

```lua
swayimg.gallery.on_mouse(button: string, fn: function)
```

Bind the mouse button press event to a handler.

@*param* `button` — Button description, for example `MouseRight`

@*param* `fn` — Button press handler

### swayimg.gallery.on_signal

```lua
swayimg.gallery.on_signal(signal: string, fn: function)
```

Bind the signal event to a handler.

@*param* `signal` — Signal name: `USR1` or `USR2`

@*param* `fn` — Signal handler

### swayimg.gallery.bind_reset

```lua
swayimg.gallery.bind_reset()
```

Remove all existing key/mouse/signal bindings.

### swayimg.gallery.set_cache_size

```lua
swayimg.gallery.set_cache_size(size: number)
```

Set max number of thumbnails stored in memory cache.

@*param* `size` — Cache size

### swayimg.gallery.enable_preload

```lua
swayimg.gallery.enable_preload(enable: boolean)
```

Enable or disable preloading invisible thumbnails.

@*param* `enable` — Enable/disable flag to set

### swayimg.gallery.enable_pstore

```lua
swayimg.gallery.enable_pstore(enable: boolean)
```

Enable or disable persistent storage for thumbnails.

@*param* `enable` — Enable/disable flag to set

### swayimg.gallery.set_pstore_path

```lua
swayimg.gallery.set_pstore_path(path: string)
```

Set custom path for persistent storage for thumbnails.

@*param* `path` — Path to the directory

