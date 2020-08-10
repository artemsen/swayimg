# Rules for building with Meson

project(
  'swayimg',
  'c',
  default_options: [
    'c_std=c99',
    'warning_level=3',
  ],
  license: 'MIT',
  version: '1.0',
)

# most of Wayland callback functions are not used
add_project_arguments(
  [
    '-Wno-unused-parameter',
  ],
  language: 'c',
)

cc = meson.get_compiler('c')

# project dependencies
wlcln = dependency('wayland-client')
cairo = dependency('cairo')
json  = dependency('json-c')
jpeg  = dependency('libjpeg', required: get_option('jpeg'))
gif   = cc.find_library('gif', required: get_option('gif'))
rt    = cc.find_library('rt')
bash  = dependency('bash-completion', required: get_option('bash'))

# configuration file
conf = configuration_data()
conf.set('HAVE_LIBJPEG', jpeg.found())
conf.set('HAVE_LIBGIF', gif.found())
conf.set_quoted('APP_VERSION', meson.project_version())
conf.set_quoted('APP_NAME', meson.project_name())
configure_file(output: 'config.h', configuration: conf)

# man installation
if get_option('man')
  install_man('swayimg.1')
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

# XDG shell Wayland protocol
xdg_shell_c = custom_target(
  'xdg-shell-protocol.c',
  output: 'xdg-shell-protocol.c',
  input: '/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml',
  command: ['wayland-scanner', 'private-code', '@INPUT@', '@OUTPUT@'],
)
xdg_shell_h = custom_target(
  'xdg-shell-protocol.h',
  output: 'xdg-shell-protocol.h',
  input: '/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml',
  command: ['wayland-scanner', 'client-header', '@INPUT@', '@OUTPUT@'],
)

executable(
  'swayimg',
  [
    'src/image.c',
    'src/main.c',
    'src/sway.c',
    'src/viewer.c',
    'src/window.c',
    xdg_shell_h,
    xdg_shell_c,
  ],
  dependencies: [
    wlcln,
    cairo,
    json,
    jpeg,
    gif,
    rt,
  ],
  install: true
)
