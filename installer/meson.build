package_config_data = configuration_data()
package_config_data.set('VERSION', meson.project_version())
package_config_data.set('SKYMExePath', skym.full_path().replace('\\', '/'))
package_config_data.set('SKYMNPPExePath', skym_npp.full_path().replace('\\', '/'))
package_config_data.set('SKYKDCCExePath', skym_dcc.full_path().replace('\\', '/'))
package_config_data.set('SKYMHVHExePath', skym_hvh.full_path().replace('\\', '/'))
package_config_data.set('SKYM_ARCHITECTURE', skym_architecture)
package_config_data.set('LICENSEFilePath', meson.source_root().replace('\\', '/') + '/LICENSE')
package_config_data.set('SKYMDocFilePath', meson.current_source_dir().replace('\\', '/') + '/skym-sshkeymanager.pdf')
package_config_data.set('SKYMGithubLink', meson.current_source_dir().replace('\\', '/') + '/SKYM.url')

build_config = configure_file(
  input: 'buildInfo.wxi.in',
  output: 'buildInfo.wxi',
  configuration: package_config_data
)

package_config_data_file = configure_file(
  input: 'manifest.json.in',
  output: 'manifest.json',
  configuration: package_config_data
)


# Build package
package_file_name = 'SKYM-' + meson.project_version() + '-' + config_data.get('SKYM_ARCHITECTURE')

msi_package = custom_target(
  'generate_installer',
  input: [
    build_config,
    'skym.wxs',
    'en-US.wxl',
    'VCRuntimeDetection.wxi',
    'WindowsDetection.wxi',
    'dialog.bmp',
    'banner.bmp',
    'gpl-3.0.rtf'
  ],
  output: [
  package_file_name + '.msi',
  package_file_name + '.wixpdb',
  ],
  command: [
    'wix.exe',
    'build',
    '-ext', 'WixToolset.UI.wixext',
    '-ext', 'WixToolset.Util.wixext',
    '-out', '@OUTPUT0@',
    '@INPUT1@',
    '@INPUT2@',
    '-d', 'Platform=' + config_data.get('SKYM_ARCHITECTURE'),
    '-arch', config_data.get('SKYM_ARCHITECTURE'),
    '-d', 'Configuration=Release',
    '-i', meson.current_build_dir(),
  ],
  depends: [
    skym,
    skym_npp,
    skym_dcc,
    skym_hvh,
  ],
  build_by_default : true,
)

# Validate package
custom_target(
  'validate_installer',
  input: msi_package,
  output: 'validation_report.txt',
  command: [
    'wix.exe',
    'msi',
   'validate',    
    '@INPUT@',
  ],
  console: true,
  depends: [msi_package],
)

python3 = find_program('python3', required: true)
# Now create ZIP package
zip_package = custom_target(
  'create_zip_package',
  input: package_config_data_file,
  output: package_file_name + '.zip',
  command: [
    python3,
    meson.current_source_dir() / 'create_zip.py',
    '@INPUT@',
    '@OUTPUT@',
  ],
  depends: [
    skym,
    skym_npp,
    skym_dcc,
    skym_hvh,
  ],
  build_by_default : true,
)