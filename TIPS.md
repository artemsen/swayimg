# Swayimg config: Tips and Tricks

* [Set window size to image size](#Set-window-size-to-image-size)
* [Change window title](#Change-window-title)
* [Delete image files](#Delete-image-files)
* [Process marked images](#Process-marked-images)
* [Handle double mouse click](#Handle-double-mouse-click)

## Set window size to image size

```lua
swayimg.viewer.on_image_change(function()
  local image = swayimg.viewer.get_image()
  if image then
    swayimg.set_window_size(image.width, image.height)
  end
end)
swayimg.on_window_resize(function()
  swayimg.viewer.set_fix_scale("fit")
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

## Delete image files

Bind the `Delete` key in viewer mode to delete the current file and display
a status message:

```lua
swayimg.viewer.on_key("Delete", function()
  local image = swayimg.viewer.get_image()
  if image then
    os.remove(image.path)
    swayimg.text.set_status("File "..image.path.." removed")
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
