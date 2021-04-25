# Rules for building with Meson

project(
  'swayimg',
  'c',
  default_options: [
    'c_std=c99',
    'warning_level=3',
  ],
  license: 'MIT',
  version: '1.3',
)

add_project_arguments(
  [
    '-Wno-unused-parameter',
  ],
  language: 'c',
)

cc = meson.get_compiler('c')

# mandatory dependencies
wlcln = dependency('wayland-client')
cairo = dependency('cairo')
json  = dependency('json-c')
xkb   = dependency('xkbcommon')
rt    = cc.find_library('rt')
# optional dependencies
jpeg  = dependency('libjpeg', required: get_option('jpeg'))
rsvg  = dependency('librsvg-2.0', version: '>=2.14', required: get_option('svg'))
webp  = dependency('libwebp', required: get_option('webp'))
avif  = dependency('libavif', required: get_option('avif'))
gif   = cc.find_library('gif', required: get_option('gif'))
bash  = dependency('bash-completion', required: get_option('bash'))

# configuration file
conf = configuration_data()
conf.set('HAVE_LIBJPEG', jpeg.found())
conf.set('HAVE_LIBGIF', gif.found())
conf.set('HAVE_LIBRSVG', rsvg.found())
conf.set('HAVE_LIBWEBP', webp.found())
conf.set('HAVE_LIBAVIF', avif.found())
conf.set_quoted('APP_VERSION', meson.project_version())
conf.set_quoted('APP_NAME', meson.project_name())
configure_file(output: 'config.h', configuration: conf)

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
  install_man('swayimg.1')
endif

# desktop file + icon
if get_option('desktop')
  install_data('swayimg.desktop',
    install_dir: join_paths(get_option('datadir'), 'applications'))
  install_data('swayimg.png',
    install_dir: join_paths(get_option('datadir'), 'icons/hicolor/64x64/apps'))
endif

# bash completion installation
if bash.found()
  datadir = get_option('datadir')
  bash_install_dir = bash.get_pkgconfig_variable(
    'completionsdir',
    define_variable: ['datadir', datadir]
  )
  install_data('bash.completion',
               install_dir: bash_install_dir,
               rename: 'swayimg')
endif

# source files
sources = [
  'src/draw.c',
  'src/image.c',
  'src/main.c',
  'src/sway.c',
  'src/viewer.c',
  'src/window.c',
  'src/formats/png.c',
  'src/formats/bmp.c',
  xdg_shell_h,
  xdg_shell_c,
]
if jpeg.found()
  sources += 'src/formats/jpeg.c'
endif
if gif.found()
  sources += 'src/formats/gif.c'
endif
if rsvg.found()
  sources += 'src/formats/svg.c'
endif
if webp.found()
  sources += 'src/formats/webp.c'
endif
if avif.found()
  sources += 'src/formats/avif.c'
endif

executable(
  'swayimg',
  sources,
  dependencies: [
    wlcln,
    xkb,
    cairo,
    gif,
    jpeg,
    json,
    rsvg,
    rt,
    webp,
    avif,
  ],
  install: true
)
