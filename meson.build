project('SKYM', ['c', 'cpp'],
  version: '25.12.16',
  default_options: ['buildtype=release', 'warning_level=3', 'cpp_std=c++20'],
  license: 'GPL-3+',
  meson_version: '>=1.4'
  )

windows = import('windows')

if meson.get_compiler('cpp').get_id() == 'msvc'
  add_project_arguments('/FC', '/wd4100', language: 'cpp')
endif

# Get version components (Y.M.B.P)

version_components = meson.project_version().split('.')
version_y = version_components[0].to_int()
version_m = version_components[1].to_int()
version_d = version_components[2].to_int()
version_b = version_components.length() > 3 ? version_components[3].to_int() : 0

config_data = configuration_data()
config_data.set('VERSION_Y', version_y)
config_data.set('VERSION_M', version_m)
config_data.set('VERSION_D', version_d)
config_data.set('VERSION_B', version_b)
config_data.set('VERSION', meson.project_version())
config_data.set('BINARY_DIR', meson.current_build_dir().replace('\\', '/'))
config_data.set('SKYM_COMPILER', meson.get_compiler('cpp').get_id() + ' ' + meson.get_compiler('cpp').version() )
config_data.set('SKYM_BUILD_TYPE', get_option('buildtype'))

if host_machine.cpu_family() == 'x86_64'
  skym_architecture = 'x64'
  skym_manifest_architecture = 'amd64'
elif host_machine.cpu_family() == 'x86'
  skym_architecture = 'x86'
  skym_manifest_architecture = 'x86'
elif host_machine.cpu_family() == 'aarch64'
  skym_architecture = 'arm64'
  skym_manifest_architecture = 'arm64'
else
  error('Unsupported architecture: ' + host_machine.cpu_family())
endif

config_data.set('SKYM_ARCHITECTURE', skym_architecture)
config_data.set('SKYM_MANIFEST_ARCHITECTURE', skym_manifest_architecture)

wtl = dependency('wtl', required: true, method: 'pkg-config')
botan = dependency('botan-3', required: true, method: 'pkg-config')
windows_unicode = declare_dependency(compile_args: ['/DUNICODE', '/D_UNICODE'])
windows_lean_and_mean = declare_dependency(compile_args: ['/DWIN32_LEAN_AND_MEAN', '/D_CRT_SECURE_NO_WARNINGS', '/DNOMINMAX'])
windows_set_version = declare_dependency(compile_args: ['/D_WIN32_WINNT=0x0A00'])

ssha_proj = subproject('libssha')
ssha = dependency('ssha')

# Generate config.h
version_h = configure_file(
  input: 'config.h.cmake',
  output: 'config.h',
  configuration: config_data
)

# Build SKYM

skym_rc_version = configure_file(
  input: 'resources/version.rc.cmake',
  output: 'version.rc',
  configuration: config_data
)
skym_rc_manifest = configure_file(
  input: 'resources/app.manifest.in',
  output: 'app.manifest',
  configuration: config_data
)
skum_rc_manifest_rc = configure_file(
  input: 'resources/app_manifest.rc.in',
  output: 'app_manifest.rc',
  configuration: config_data
)
skym_rc_objects = windows.compile_resources(
  [
    'resources/resources.rc', 
    'resources/dialogs.rc',
    skym_rc_version,
    skum_rc_manifest_rc
  ],
  depend_files: ['RELEASE', 'ChangeLog', 'LICENSE', skym_rc_manifest.full_path()],  
)

skym_include = include_directories('src')

skym = executable('SKYM',
 [
  'src/main.cpp',
  'src/dialogs/ckeyconfirm.cpp',
    'src/dialogs/ckeylist.cpp',
    'src/dialogs/ckeyselection.cpp',
    'src/dialogs/settings/chypervpage.cpp',
    'src/dialogs/addvm.cpp',
    'src/key-constrains-loader.cpp',
    'src/debug-console.cpp',
    'src/factories/icon-factory.cpp',
    'src/factories/bitmap-factory.cpp',
    'src/servers/pipe-agent-session.cpp',
    'src/servers/pipe-server.cpp',
    'src/servers/pageant-server.cpp',
    'src/servers/hyperv-session.cpp',
    'src/servers/windows-session.cpp',
    'src/ssh/process-info-extension.cpp',
    'src/utils/string-converter.cpp',
    'src/system/csid.cpp',
    'src/system/directories.cpp',
    'src/system/uac-helper.cpp',
    skym_rc_objects
  ],
  dependencies: [ssha, wtl, windows_lean_and_mean, windows_set_version],
  win_subsystem: 'windows',
  include_directories: skym_include,
  link_args: ['ws2_32.lib', 'Shcore.lib', '/MANIFEST:NO']
)

subdir('dcc')
subdir('hvh')
subdir('npp')
subdir('installer')
