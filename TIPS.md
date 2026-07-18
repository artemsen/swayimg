# Swayimg config: Tips and Tricks

* [Change scale on window resize](#Change-scale-on-window-resize)
* [Delete file form storage](#Delete-file-form-storage)
* [Change window title](#Change-window-title)
* [Process marked images](#Process-marked-images)
* [Handle double mouse click](#Handle-double-mouse-click)

## Change scale on window resize

Force set scale on window resize (useful for tiling compositors):
```lua
swayimg.on_window_resize(function()
  swayimg.viewer.set_fix_scale("optimal")
end)
```

## Delete file form storage

Bind the `Delete` key in slide show mode to delete the current file and display
a status message:
```lua
swayimg.slideshow.on_key("Delete", function()
  local image = swayimg.slideshow.get_image()
  if image then
    os.remove(image.path)
    swayimg.text.set_status("File "..image.path.." removed")
  end
end)
```

## Change window title

Set a custom window title in gallery mode:
```lua
swayimg.gallery.on_image_change(function()
  local image = swayimg.gallery.get_image()
  if image then
    swayimg.set_title("Gallery: "..image.path)
  end
end)
```

## Process marked images

Print paths of all marked files by pressing `Ctrl-p` in gallery mode:
```lua
swayimg.gallery.on_key("Ctrl-p", function()
  local entries = swayimg.imagelist.get()
  for _, entry in ipairs(entries) do
    if entry.mark then
        print(entry.path)
    end
  end
end)
```

## Handle double mouse click

```lua
local double_click_delay = 0.3 -- max 0.3 sec between clicks
local click_counter = 0

swayimg.viewer.on_mouse("MouseLeft", function()
  click_counter = click_counter + 1
  swayimg.defer(double_click_delay, function()
    if click_counter > 1 then
      print("Double click")
    else
      print("Single click")
    end
    click_counter = 0
  end)
end)
```
