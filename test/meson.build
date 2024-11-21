# Rules for building tests

sources = [
  'action_test.cpp',
  'config_test.cpp',
  'imagelist_test.cpp',
  'keybind_test.cpp',
  'loader_test.cpp',
  'memdata_test.cpp',
  'pixmap_test.cpp',
  '../src/action.c',
  '../src/config.c',
  '../src/event.c',
  '../src/image.c',
  '../src/imagelist.c',
  '../src/keybind.c',
  '../src/loader.c',
  '../src/memdata.c',
  '../src/pixmap.c',
  '../src/formats/bmp.c',
  '../src/formats/farbfeld.c',
  '../src/formats/pnm.c',
  '../src/formats/qoi.c',
  '../src/formats/tga.c',
]
if exif.found()
  sources += ['exif_test.cpp', '../src/exif.c']
endif
if exr.found()
  sources += '../src/formats/exr.c'
endif
if gif.found()
  sources += '../src/formats/gif.c'
endif
if heif.found()
  sources += '../src/formats/heif.c'
endif
if avif.found()
  sources += '../src/formats/avif.c'
endif
if jpeg.found()
  sources += '../src/formats/jpeg.c'
endif
if jxl.found()
  sources += '../src/formats/jxl.c'
endif
if png.found()
  sources += '../src/formats/png.c'
endif
if rsvg.found()
  sources += '../src/formats/svg.c'
endif
if tiff.found()
  sources += '../src/formats/tiff.c'
endif
if webp.found() and webp_demux.found()
  sources += '../src/formats/webp.c'
endif

test(
  'swayimg',
  executable(
    'swayimg_test',
    sources,
    dependencies: [
      dependency('gtest', main: true, disabler: true, required: true),
      xkb,
      exif,
      exr,
      gif,
      heif,
      inotify,
      avif,
      jpeg,
      jxl,
      png,
      rsvg,
      tiff,
      webp, webp_demux,
    ],
    include_directories: '../src',
    cpp_args : '-DTEST_DATA_DIR="' + meson.current_source_dir() + '/data"',
  )
)
configure_file(output: 'buildcfg.h', configuration: conf)
