-- Example config for Swayimg

-- The viewer searches for the config file in the following locations:
-- 1. $XDG_CONFIG_HOME/swayimg/init.lua
-- 2. $HOME/.config/swayimg/init.lua
-- 3. $XDG_CONFIG_DIRS/swayimg/init.lua
-- 4. /etc/xdg/swayimg/init.lua

-- set order by file size for the image list
swayimg.imagelist.set_order("size")

-- set font size
swayimg.text.set_size(32)
-- set font color to fully opaque red color
swayimg.text.set_foreground(0xffff0000)

-- set top left text block scheme for viewer mode
swayimg.viewer.set_text_tl({
  "File: {name}",
  "Format: {format}",
  "File size: {sizehr}",
  "File time: {time}",
  "EXIF date: {meta.Exif.Photo.DateTimeOriginal}",
  "EXIF camera: {meta.Exif.Image.Model}"
})

-- bind the left arrow key to move the image to the left by 1/10 of the application window size
swayimg.viewer.on_key("Left", function()
  local w, _ = unpack(swayimg.get_window_size())
  local x, y = unpack(swayimg.viewer.get_position())
  swayimg.viewer.set_abs_position(x - w / 10, y);
end)

-- bind mouse vertical scroll button with pressed Ctrl to zoom in the image at mouse pointer coordinates
swayimg.viewer.on_mouse("Ctrl-ScrollUp", function()
  local x, y = unpack(swayimg.get_mouse_pos())
  local scale = swayimg.viewer.get_scale()
  scale = scale + scale / 10
  swayimg.viewer.set_abs_scale(scale, x, y);
end)

-- bind the Delete key in slide show mode to delete the current file and display a status message
swayimg.slideshow.on_key("Delete", function()
  local image = swayimg.slideshow.current_image()
  os.remove(image['path'])
  swayimg.set_status("File "..image["path"].." removed")
end)

-- set a custom window title in gallery mode
swayimg.gallery.on_change_image(function()
  local image = swayimg.gallery.current_image()
  swayimg.set_title("Gallery: "..image['path'])
end)

-- print paths to all marked files by pressing Ctrl-p in gallery mode
swayimg.gallery.on_key("Ctrl-p", function()
  local entries = swayimg.imagelist.get()
  for _, entry in ipairs(entries) do
    if entry['mark'] then
        print(entry['path'])
    end
  end
end)
