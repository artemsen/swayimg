# Swayimg — Wayland Image Viewer

## Active Technologies
- Lua 5.4 (embedded in swayimg C++ binary) + swayimg (local build from `~/src/swayimg`), chezmoi (dotfile deployment) (011-swayimg-lua-config)
- N/A (single config file on disk) (011-swayimg-lua-config)
- C++20 (swayimg codebase at `~/src/swayimg/`) + Wayland client libs, image decoder libs (libjpeg, libpng, libwebp, etc.), Lua 5.4, pthreads (015-fix-swayimg-loader-threading)
- N/A (in-memory image caches) (015-fix-swayimg-loader-threading)
- C++20 (compiled with meson/ninja) + swayimg (local build at `~/src/swayimg/`), Wayland, Vulkan, LuaBridge (016-fix-swayimg-gallery-loading)
- In-memory thumbnail cache (`std::deque<ThumbEntry>`) + optional persistent storage (filesystem cache at `~/.cache/swayimg`) (016-fix-swayimg-gallery-loading)
