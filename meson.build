# Rules for building with Meson

project(
    'swayimg',
    'c',
    'cpp',
    default_options: [
        'cpp_std=c++20',
        'warning_level=3',
        'buildtype=release',
        'b_ndebug=if-release',
    ],
    license: 'MIT',
    version: '0.0.0',
    meson_version: '>=1.1',
)

# version info
version = get_option('version')
if version == '0.0.0'
    git = find_program('git', native: true, required: false)
    if git.found()
        git_ver = run_command(
            [git, 'describe', '--tags', '--long', '--always', '--dirty'],
            check: false,
        )
        if git_ver.returncode() == 0
            version = git_ver.stdout().strip().substring(1)
        endif
    endif
endif

# mandatory dependencies
xkb = dependency('xkbcommon')
fontconfig = dependency('fontconfig')
freetype = dependency('freetype2')
threads = dependency('threads')
lua = dependency('luajit')

# wayland support
wl_protocols = []
wlcln = dependency('wayland-client', required: get_option('wayland'))
if wlcln.found()
    wlproto = dependency('wayland-protocols', version: '>=1.35')
    wlproto_dir = wlproto.get_variable(pkgconfig: 'pkgdatadir')
    wlscan = dependency('wayland-scanner', required: false, native: true)
    if wlscan.found()
        wl_scanner = find_program(
            wlscan.get_variable(pkgconfig: 'wayland_scanner'),
            native: true,
        )
    else
        wl_scanner = find_program('wayland-scanner', native: true)
    endif

    protocols = [
        wlproto_dir / 'stable/xdg-shell/xdg-shell.xml',
        wlproto_dir / 'stable/tablet/tablet-v2.xml',
        wlproto_dir / 'stable/viewporter/viewporter.xml',
        wlproto_dir / 'staging/content-type/content-type-v1.xml',
        wlproto_dir / 'staging/cursor-shape/cursor-shape-v1.xml',
        wlproto_dir / 'staging/ext-idle-notify/ext-idle-notify-v1.xml',
        wlproto_dir / 'staging/fractional-scale/fractional-scale-v1.xml',
        wlproto_dir / 'unstable/xdg-decoration/xdg-decoration-unstable-v1.xml',
    ]

    foreach xml : protocols
        wl_protocols += custom_target(
            xml.underscorify() + '_c',
            input: xml,
            output: '@BASENAME@-protocol.c',
            command: [wl_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
        )
        wl_protocols += custom_target(
            xml.underscorify() + '_client_h',
            input: xml,
            output: '@BASENAME@-client-protocol.h',
            command: [wl_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
        )
    endforeach
endif

# DRM support
drm = dependency('libdrm', required: get_option('drm'))

assert(wlcln.found() or drm.found(), 'UI libs not available')

# optional dependencies: file formats support
exr = dependency('OpenEXR', version: '>=3.4', required: get_option('exr'))
heif = dependency('libheif', required: get_option('heif'))
avif = dependency('libavif', version: '>=1.0', required: get_option('avif'))
jpeg = dependency('libjpeg', required: get_option('jpeg'))
jxl = dependency('libjxl', required: get_option('jxl'))
jxl_threads = dependency('libjxl_threads', required: get_option('jxl'))
png = dependency('libpng', required: get_option('png'))
rsvg = dependency('librsvg-2.0', version: '>=2.46', required: get_option('svg'))
tiff = dependency('libtiff-4', required: get_option('tiff'))
sixel = dependency('libsixel', required: get_option('sixel'))
raw = dependency('libraw', required: get_option('raw'))
webp = dependency('libwebp', required: get_option('webp'))
webp_demux = dependency('libwebpdemux', required: get_option('webp'))

# hack to build "pkgconfigless" gif in FreeBSD
cc = meson.get_compiler('c')
gif_opt = get_option('gif')
if gif_opt.disabled()
    gif = cc.find_library('gif', required: gif_opt)
else
    gif = cc.find_library('gif', required: false)
    if not gif.found() and gif_opt.allowed()
        gif = cc.find_library(
            'gif',
            dirs: xkb.get_variable(pkgconfig: 'libdir'),
            required: gif_opt,
        )
    endif
endif

# optional dependencies: other features
exiv = dependency('exiv2', required: get_option('exif'))
bash = dependency('bash-completion', required: get_option('bash'))

# non-Linux (BSD specific)
epoll = dependency('epoll-shim', required: false)
inotify = dependency('libinotify', required: false)

# configuration file
conf = configuration_data()
conf.set('HAVE_COMPOSITOR', wlcln.found() and get_option('compositor').allowed())
conf.set('HAVE_WAYLAND', wlcln.found())
conf.set('HAVE_DRM', drm.found())
conf.set('HAVE_INOTIFY', cc.has_header('sys/inotify.h', dependencies: inotify))
conf.set('HAVE_LIBEXIV2', exiv.found())
conf.set('HAVE_LIBEXR', exr.found())
conf.set('HAVE_LIBGIF', gif.found())
conf.set('HAVE_LIBHEIF', heif.found())
conf.set('HAVE_LIBAVIF', avif.found())
conf.set('HAVE_LIBJPEG', jpeg.found())
conf.set('HAVE_LIBJXL', jxl.found() and jxl_threads.found())
conf.set('HAVE_LIBPNG', png.found())
conf.set('HAVE_LIBRSVG', rsvg.found())
conf.set('HAVE_LIBTIFF', tiff.found())
conf.set('HAVE_LIBSIXEL', sixel.found())
conf.set('HAVE_LIBRAW', raw.found())
conf.set('HAVE_LIBWEBP', webp.found() and webp_demux.found())
conf.set_quoted('APP_VERSION', version)
configure_file(output: 'buildconf.hpp', configuration: conf)

# install sample config
install_data('extra/swayimgrc', install_dir: get_option('datadir') / 'swayimg')

# man installation
if get_option('man')
    scdoc = find_program('scdoc', native: true, required: false)
    if scdoc.found()
        mandir = get_option('mandir')
        foreach filename : ['extra/swayimg.1.scd', 'extra/swayimgrc.5.scd']
            topic = filename.split('.')[-3].split('/')[-1]
            section = filename.split('.')[-2]
            output = '@0@.@1@'.format(topic, section)
            custom_target(
                output,
                input: filename,
                output: output,
                command: scdoc,
                install: true,
                feed: true,
                capture: true,
                install_dir: '@0@/man@1@'.format(mandir, section),
            )
        endforeach
    else
        install_man('extra/swayimg.1')
        install_man('extra/swayimgrc.5')
    endif
endif

# desktop file + icon
if get_option('desktop')
    install_data(
        'extra/swayimg.desktop',
        install_dir: get_option('datadir') / 'applications',
    )
    install_data(
        'extra/icon_64.png',
        rename: 'swayimg.png',
        install_dir: get_option('datadir') / 'icons/hicolor/64x64/apps',
    )
    install_data(
        'extra/icon_128.png',
        rename: 'swayimg.png',
        install_dir: get_option('datadir') / 'icons/hicolor/128x128/apps',
    )
    install_data(
        'extra/icon_256.png',
        rename: 'swayimg.png',
        install_dir: get_option('datadir') / 'icons/hicolor/256x256/apps',
    )
endif

# zsh completion
zsh = get_option('zsh')
if zsh.auto()
    shell = find_program('zsh', required: false)
    zsh = zsh.disable_auto_if(not shell.found())
endif
if zsh.allowed()
    datadir = get_option('datadir')
    zsh_install_dir = datadir / 'zsh' / 'site-functions'
    install_data(
        'extra/zsh.completion',
        install_dir: zsh_install_dir,
        rename: '_swayimg',
    )
endif

# bash completion installation
if bash.found()
    datadir = get_option('datadir')
    bash_install_dir = bash.get_variable(
        pkgconfig: 'completionsdir',
        pkgconfig_define: ['datadir', datadir],
    )
    install_data(
        'extra/bash.completion',
        install_dir: bash_install_dir,
        rename: 'swayimg',
    )
endif

# source files
sources = [
    'src/application.cpp',
    'src/appmode.cpp',
    'src/fdevent.cpp',
    'src/font.cpp',
    'src/fsmonitor.cpp',
    'src/gallery.cpp',
    'src/geometry.cpp',
    'src/image.cpp',
    'src/imagelist.cpp',
    'src/imageloader.cpp',
    'src/input.cpp',
    'src/luaengine.cpp',
    'src/pixmap.cpp',
    'src/render.cpp',
    'src/slideshow.cpp',
    'src/text.cpp',
    'src/threadpool.cpp',
    'src/viewer.cpp',
    'src/xkb.cpp',
    'src/formats/bmp.cpp',
    'src/formats/dicom.cpp',
    'src/formats/farbfeld.cpp',
    'src/formats/pnm.cpp',
    'src/formats/qoi.cpp',
    'src/formats/tga.cpp',
    wl_protocols,
]

if wlcln.found()
    sources += 'src/ui_wayland.cpp'
endif
if drm.found()
    sources += 'src/ui_drm.cpp'
endif
if wlcln.found() and get_option('compositor').allowed()
    sources += 'src/compositor.cpp'
endif
if exr.found()
    sources += 'src/formats/exr.cpp'
endif
if gif.found()
    sources += 'src/formats/gif.cpp'
endif
if heif.found()
    sources += 'src/formats/heif.cpp'
endif
if avif.found()
    sources += 'src/formats/avif.cpp'
endif
if jpeg.found()
    sources += 'src/formats/jpeg.cpp'
endif
if jxl.found()
    sources += 'src/formats/jxl.cpp'
endif
if png.found()
    sources += 'src/formats/png.cpp'
endif
if rsvg.found()
    sources += 'src/formats/svg.cpp'
endif
if tiff.found()
    sources += 'src/formats/tiff.cpp'
endif
if sixel.found()
    sources += 'src/formats/sixel.cpp'
endif
if raw.found()
    sources += 'src/formats/raw.cpp'
endif
if webp.found() and webp_demux.found()
    sources += 'src/formats/webp.cpp'
endif

dependencies = [
    # runtime
    drm,
    epoll,
    fontconfig,
    freetype,
    inotify,
    lua,
    threads,
    wlcln,
    xkb,
    exiv,
    # image support
    avif,
    exr,
    gif,
    heif,
    jpeg,
    jxl,
    jxl_threads,
    png,
    raw,
    rsvg,
    sixel,
    tiff,
    webp,
    webp_demux,
]

executable(
    'swayimg',
    sources + 'src/main.cpp',
    dependencies: dependencies,
    include_directories: 'src/external',
    install: true,
)

# unit tests
if get_option('tests').enabled()
    gtest = dependency('gtest', main: true, disabler: true, required: true)
    test_exe = executable(
        'swayimg_test',
        sources + [
            'test/color_test.cpp',
            'test/geometry_test.cpp',
            'test/image_test.cpp',
            'test/imagelist_test.cpp',
            'test/input_test.cpp',
            'test/pixmap_test.cpp',
        ],
        include_directories: ['src', 'src/external'],
        dependencies: dependencies + gtest,
        cpp_args: '-DTEST_DATA_DIR="' + meson.current_source_dir() + '/test/data"',
    )
    test('Unit tests', test_exe)
endif
