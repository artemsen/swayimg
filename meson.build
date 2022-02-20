# Rules for building with Meson

project(
  'swayimg',
  'c',
  default_options: [
    'c_std=c99',
    'warning_level=3',
  ],
  license: 'MIT',
  version: '0.0.0',
)

add_project_arguments(
  [
    '-Wno-unused-parameter',
    '-D_POSIX_C_SOURCE=200809'
  ],
  language: 'c',
)

cc = meson.get_compiler('c')
version = get_option('version')

# mandatory dependencies
wlcln = dependency('wayland-client')
cairo = dependency('cairo')
json  = dependency('json-c')
xkb   = dependency('xkbcommon')
rt    = cc.find_library('rt')
# optional dependencies: file formats support
avif  = dependency('libavif', required: get_option('avif'))
gif   = cc.find_library('gif', required: get_option('gif'))
jpeg  = dependency('libjpeg', required: get_option('jpeg'))
jxl   = dependency('libjxl', required: get_option('jxl'))
png   = dependency('libpng', required: get_option('png'))
rsvg  = dependency('librsvg-2.0', version: '>=2.46', required: get_option('svg'))
webp  = dependency('libwebp', required: get_option('webp'))
# optional dependencies: other features
exif  = dependency('libexif', required: get_option('exif'))
bash  = dependency('bash-completion', required: get_option('bash'))

# configuration file
conf = configuration_data()
conf.set('HAVE_LIBAVIF', avif.found())
conf.set('HAVE_LIBGIF', gif.found())
conf.set('HAVE_LIBJPEG', jpeg.found())
conf.set('HAVE_LIBJXL', jxl.found())
conf.set('HAVE_LIBPNG', png.found())
conf.set('HAVE_LIBRSVG', rsvg.found())
conf.set('HAVE_LIBWEBP', webp.found())
conf.set('HAVE_LIBEXIF', exif.found())
conf.set_quoted('APP_NAME', meson.project_name())
conf.set_quoted('APP_VERSION', version)
configure_file(output: 'buildcfg.h', configuration: conf)

# Wayland protocols
wlproto = dependency('wayland-protocols')
wlproto_dir = wlproto.get_pkgconfig_variable('pkgdatadir')
wlscan = dependency('wayland-scanner', required: false, native: true)
if wlscan.found()
  wl_scanner = find_program(wlscan.get_pkgconfig_variable('wayland_scanner'), native: true)
else
  wl_scanner = find_program('wayland-scanner', native: true)
endif

# XDG shell Wayland protocol
xdg_shell_xml = join_paths(wlproto_dir, 'stable/xdg-shell/xdg-shell.xml')
xdg_shell_c = custom_target(
  'xdg-shell-protocol.c',
  output: 'xdg-shell-protocol.c',
  input: xdg_shell_xml,
  command: [wl_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)
xdg_shell_h = custom_target(
  'xdg-shell-protocol.h',
  output: 'xdg-shell-protocol.h',
  input: xdg_shell_xml,
  command: [wl_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)

# man installation
if get_option('man')
  install_man('extra/swayimg.1')
  install_man('extra/swayimgrc.5')
endif

# desktop file + icon
if get_option('desktop')
  install_data('extra/swayimg.desktop',
    install_dir: join_paths(get_option('datadir'), 'applications'))
  install_data('extra/swayimg.png',
    install_dir: join_paths(get_option('datadir'), 'icons/hicolor/64x64/apps'))
endif

# bash completion installation
if bash.found()
  datadir = get_option('datadir')
  bash_install_dir = bash.get_pkgconfig_variable(
    'completionsdir',
    define_variable: ['datadir', datadir]
  )
  install_data('extra/bash.completion',
               install_dir: bash_install_dir,
               rename: 'swayimg')
endif

# source files
sources = [
  'src/canvas.c',
  'src/config.c',
  'src/image.c',
  'src/main.c',
  'src/sway.c',
  'src/viewer.c',
  'src/window.c',
  'src/formats/bmp.c',
  'src/formats/common.c',
  xdg_shell_h,
  xdg_shell_c,
]
if exif.found()
  sources += 'src/exif.c'
endif
if avif.found()
  sources += 'src/formats/avif.c'
endif
if gif.found()
  sources += 'src/formats/gif.c'
endif
if jpeg.found()
  sources += 'src/formats/jpeg.c'
endif
if jxl.found()
  sources += 'src/formats/jxl.c'
endif
if png.found()
  sources += 'src/formats/png.c'
endif
if rsvg.found()
  sources += 'src/formats/svg.c'
endif
if webp.found()
  sources += 'src/formats/webp.c'
endif

executable(
  'swayimg',
  sources,
  dependencies: [
    avif,
    cairo,
    exif,
    gif,
    jpeg,
    json,
    jxl,
    png,
    rsvg,
    rt,
    webp,
    wlcln,
    xkb,
  ],
  install: true
)
