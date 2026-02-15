-- Example config for Swayimg

-- set font face name
-- swayimg.font.name = "monospace"

-- set font size in pixels
-- swayimg.font.size = 32

-- set top left text block scheme
-- swayimg.text.scheme_tl({
--   "File:{name}",
--   "Format:{format}",
--   "File size:{size}",
--   "File time:{time}",
--   "EXIF date:{meta.Exif.Photo.DateTimeOriginal}",
--   "EXIF camera:{meta.Exif.Image.Model}"
-- })

-- key bindings
swayimg.view.bind_key("F3", function()
   swayimg.exit()
end)

swayimg.view.on_open(function(image)
  print('open image:')
  for key, value in pairs(image) do
    print('', key, value)
  end
end)
