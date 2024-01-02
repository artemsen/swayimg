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
  meson_version: '>=0.59.0',
)

add_project_arguments(
  [
    '-D_POSIX_C_SOURCE=200809'
  ],
  language: 'c',
)

cc = meson.get_compiler('c')

# version info
version = get_option('version')
if version == '0.0.0'
  git = find_program('git', native: true, required: false)
  if git.found()
    git_ver = run_command([git, 'describe', '--tags', '--long', '--always', '--dirty'], check: false)
    if git_ver.returncode() == 0
      version = git_ver.stdout().strip().substring(1)
    endif
  endif
endif

# mandatory dependencies
wlcln = dependency('wayland-client')
json = dependency('json-c')
xkb = dependency('xkbcommon')
fontconfig = dependency('fontconfig')
freetype = dependency('freetype2')
threads = dependency('threads')
rt = cc.find_library('rt')
# optional dependencies: file formats support
exr = dependency('OpenEXR', version: '>=3.1', required: get_option('exr'))
gif = cc.find_library('gif', required: get_option('gif'))
heif = dependency('libheif', required: get_option('heif'))
avif = dependency('libavif', required: get_option('avif'))
jpeg = dependency('libjpeg', required: get_option('jpeg'))
png = dependency('libpng', required: get_option('png'))
rsvg = dependency('librsvg-2.0', version: '>=2.46', required: get_option('svg'))
tiff = dependency('libtiff-4', required: get_option('tiff'))
webp = dependency('libwebp', required: get_option('webp'))
webp_demux = dependency('libwebpdemux', required: get_option('webp'))
# optional dependencies: other features
exif = dependency('libexif', required: get_option('exif'))
bash = dependency('bash-completion', required: get_option('bash'))
# Arch specific: https://bugs.archlinux.org/task/73931
jxl_feature = get_option('jxl')
jxl = dependency('libjxl', required: false)
if not jxl.found() and (jxl_feature.auto() or jxl_feature.enabled())
  jxl = cc.find_library('libjxl', required: jxl_feature)
endif
# non-Linux (BSD specific)
epoll = dependency('epoll-shim', required: false)

# configuration file
conf = configuration_data()
conf.set('HAVE_LIBEXR', exr.found())
conf.set('HAVE_LIBGIF', gif.found())
conf.set('HAVE_LIBHEIF', heif.found())
conf.set('HAVE_LIBAVIF', avif.found())
conf.set('HAVE_LIBJPEG', jpeg.found())
conf.set('HAVE_LIBJXL', jxl.found())
conf.set('HAVE_LIBPNG', png.found())
conf.set('HAVE_LIBRSVG', rsvg.found())
conf.set('HAVE_LIBTIFF', tiff.found())
conf.set('HAVE_LIBWEBP', webp.found() and webp_demux.found())
conf.set('HAVE_LIBEXIF', exif.found())
conf.set_quoted('APP_NAME', meson.project_name())
conf.set_quoted('APP_VERSION', version)
configure_file(output: 'buildcfg.h', configuration: conf)

# Wayland protocols
wlproto = dependency('wayland-protocols')
wlproto_dir = wlproto.get_variable(pkgconfig : 'pkgdatadir')
wlscan = dependency('wayland-scanner', required: false, native: true)
if wlscan.found()
  wl_scanner = find_program(wlscan.get_variable(pkgconfig : 'wayland_scanner'), native: true)
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

# install sample config
install_data('extra/swayimgrc',
  install_dir: join_paths(get_option('datadir'), 'swayimg'))

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

# zsh completion
zsh = get_option('zsh')
if zsh.auto()
  shell = find_program('zsh', required: false)
  zsh = zsh.disable_auto_if(not shell.found())
endif
if zsh.allowed()
  datadir = get_option('datadir')
  zsh_install_dir = join_paths(datadir, 'zsh', 'site-functions')
  install_data('extra/zsh.completion',
               install_dir: zsh_install_dir,
               rename: '_swayimg')
endif

# bash completion installation
if bash.found()
  datadir = get_option('datadir')
  bash_install_dir = bash.get_variable(pkgconfig : 'completionsdir',
                                       pkgconfig_define: ['datadir', datadir])
  install_data('extra/bash.completion',
               install_dir: bash_install_dir,
               rename: 'swayimg')
endif

# source files
sources = [
  'src/canvas.c',
  'src/config.c',
  'src/font.c',
  'src/image.c',
  'src/imagelist.c',
  'src/info.c',
  'src/keybind.c',
  'src/main.c',
  'src/str.c',
  'src/sway.c',
  'src/ui.c',
  'src/viewer.c',
  'src/formats/bmp.c',
  'src/formats/pnm.c',
  'src/formats/loader.c',
  xdg_shell_h,
  xdg_shell_c,
]
if exif.found()
  sources += 'src/exif.c'
endif
if exr.found()
  sources += 'src/formats/exr.c'
endif
if gif.found()
  sources += 'src/formats/gif.c'
endif
if heif.found()
  sources += 'src/formats/heif.c'
endif
if avif.found()
  sources += 'src/formats/avif.c'
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
if tiff.found()
  sources += 'src/formats/tiff.c'
endif
if webp.found() and webp_demux.found()
  sources += 'src/formats/webp.c'
endif

executable(
  'swayimg',
  sources,
  dependencies: [
    # runtime
    rt,
    threads,
    wlcln,
    epoll,
    json,
    xkb,
    fontconfig,
    freetype,
    exif,
    # image support
    exr,
    gif,
    heif,
    avif,
    jpeg,
    jxl,
    png,
    rsvg,
    tiff,
    webp, webp_demux,
  ],
  install: true
)
