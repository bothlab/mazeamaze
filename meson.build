project('Syntalos', 'c', 'cpp',
  meson_version: '>=0.57',
  default_options: ['c_std=gnu17',
                    'cpp_std=gnu++17',
                    'b_lto=true'],

  license: 'LGPL-3.0+',
  version: '1.0.0',
)

cpp_compiler = meson.get_compiler('cpp')
source_root = meson.source_root()

sy_version = meson.project_version()
varr = sy_version.split('.')
sy_major_version = varr[0]
sy_minor_version = varr[1]
sy_micro_version = varr[2]

root_include_dir = include_directories('.')

#
# Modules
#
fs     = import('fs')
qt     = import('qt5')
python = import('python').find_installation('python3')

#
# Configure files
#
sy_libdir = join_paths(get_option('prefix'),
                       get_option('libdir'),
                       'syntalos')
sy_modules_dir = join_paths(sy_libdir, 'modules')

conf = configuration_data()
conf.set('SY_MAJOR_VERSION_CONF', sy_major_version)
conf.set('SY_MINOR_VERSION_CONF', sy_minor_version)
conf.set('SY_MICRO_VERSION_CONF', sy_micro_version)
conf.set_quoted('PROJECT_VERSION', sy_version)
conf.set_quoted('LIBEXECDIR',
                join_paths(get_option('prefix'),
                           get_option('libexecdir')))
conf.set_quoted('DATADIR',
                join_paths(get_option('prefix'),
                           get_option('datadir')))
conf.set_quoted('LIBDIR',
                join_paths(get_option('prefix'),
                           get_option('libdir')))
conf.set_quoted('BINDIR',
                join_paths(get_option('prefix'),
                           get_option('bindir')))
conf.set_quoted('SYSCONFDIR',
                join_paths(get_option('prefix'),
                           get_option('sysconfdir')))
conf.set_quoted('SY_MODULESDIR', sy_modules_dir)
conf.set_quoted('SY_VCS_TAG', '@VCS_TAG@')
conf.set_quoted('PYTHON_LANG_VERSION', python.language_version())
config_h = configure_file(output: 'config.h.in', configuration: conf)
config_h = vcs_tag(input: config_h, output: 'config.h')

#
# Custom compile flags
#
if get_option('maintainer')
    maintainer_c_args = ['-Werror',
                         '-Wall',
                         '-Wextra',
                         '-Wcast-align',
                         '-Wno-uninitialized',
                         '-Wempty-body',
                         '-Wformat-security',
                         '-Winit-self',
                         '-Wnull-dereference',
                         '-Winline'
    ]
    maintainer_cpp_args = ['-Wzero-as-null-pointer-constant',
                           '-Wsuggest-final-methods'
    ]

    add_project_arguments(maintainer_c_args, language: 'c')
    add_project_arguments([maintainer_c_args, maintainer_cpp_args], language: 'cpp')
endif

# a few compiler warning/error flags we always want enabled
add_project_arguments('-Werror=implicit-function-declaration', '-Wno-unused-parameter', language: 'c')
add_project_arguments('-Wno-unused-parameter', '-Werror=suggest-override', language: 'cpp')

if get_option('tracing')
    add_project_arguments('-pg', language: 'c')
    add_project_arguments('-pg', language: 'cpp')
    add_project_link_arguments('-pg')
endif

if get_option('optimize-modern-amd64') and build_machine.cpu_family() == 'x86_64' and not get_option('optimize-native')
    if cpp_compiler.has_argument('--with-arch_64=x86-64-v3')
        message('Using x86-64-v3 feature levels from ' + cpp_compiler.get_id() + ' ' + cpp_compiler.version())
        o_flags = ['--with-arch_64=x86-64-v3']
    else
        message('Compiler ' + cpp_compiler.get_id() + ' ' + cpp_compiler.version() + ' ' +
                'does not support x86-64 microarchitecture feature levels. Just enabling AVX/AVX2.')
        o_flags = ['-mavx', '-mavx2']
    endif
    add_project_arguments(o_flags, language: 'c')
    add_project_arguments(o_flags, language: 'cpp')
endif
if get_option('optimize-native') and not get_option('optimize-modern-amd64')
    add_project_arguments('-march=native', language: 'c')
    add_project_arguments('-march=native', language: 'cpp')
endif

#
# Dependencies
#
qt_core_dep = dependency('qt5', modules: ['Core'], version: '>= 5.12', include_type: 'system')
qt_gui_dep = dependency('qt5', modules: ['Gui', 'Widgets'])
qt_test_dep = dependency('qt5', modules: ['Test'])
qt_opengl_dep = dependency('qt5', modules: ['OpenGL'])
qt_remoteobj_dep = dependency('qt5', modules: ['RemoteObjects'])
qt_svg_dep = dependency('qt5', modules: ['Svg'])
qt_dbus_dep = dependency('qt5', modules: ['DBus'])
qt_charts_dep = dependency('qt5', modules: ['Charts'], include_type: 'system')

rt_lib = cpp_compiler.find_library('rt')
thread_dep = dependency('threads')
opengl_dep = dependency('GL')
xxhash_dep = dependency('libxxhash')
eigen_dep = dependency('eigen3', version: '>= 3.3', include_type: 'system')
toml_dep = dependency('tomlplusplus', version: '>=2.0')
opencv_dep = dependency('opencv4', include_type: 'system')

avcodec_dep = dependency('libavcodec', version: '>= 58.20')
avformat_dep = dependency('libavformat', version: '>= 58.20')
avutil_dep = dependency('libavutil', version: '>= 56.20')
swscale_dep = dependency('libswscale', version: '>= 5.5')

glib_dep = dependency('glib-2.0', version: '>=2.58')
gobject_dep = dependency('gobject-2.0', version: '>=2.58')

kfarchive_dep = dependency('KF5Archive')
kfdbusaddons_dep = dependency('KF5DBusAddons')

gstreamer_dep = dependency('gstreamer-1.0')
gstreamer_app_dep = dependency('gstreamer-app-1.0')
gstreamer_video_dep = dependency('gstreamer-video-1.0')

python_dep = dependency('python3')
python_embed_dep = dependency('python3-embed')
pybind11_dep = dependency('pybind11')

# essential dependencies definitely required by
# anything using the Syntalos API
# NOTE: QtWidgets is required since modules expose QAction items
sy_base_deps = [rt_lib,
                thread_dep,
                qt_core_dep,
                qt_gui_dep,
                eigen_dep,
                xxhash_dep
]

#
# Options
#
udevdir = get_option('udevdir')
if udevdir == ''
    udev = dependency('udev')
    udevdir = udev.get_variable(pkgconfig: 'udevdir')
endif


#
# Subdirectories
#
subdir('src')
subdir('modules')
subdir('tests')
subdir('tools')
subdir('data')
