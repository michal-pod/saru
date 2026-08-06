package_config_data = configuration_data()
package_config_data.set('VERSION', meson.project_version())
package_config_data.set('SARUExePath', saru.full_path().replace('\\', '/'))
package_config_data.set('SARUNPPExePath', saru_npp.full_path().replace('\\', '/'))
package_config_data.set('SKYKDCCExePath', saru_dcc.full_path().replace('\\', '/'))
package_config_data.set('SARUHVHExePath', saru_hvh.full_path().replace('\\', '/'))
package_config_data.set('SARU_ARCHITECTURE', saru_architecture)
package_config_data.set('LICENSEFilePath', meson.source_root().replace('\\', '/') + '/LICENSE')
package_config_data.set('SARUDocFilePath', meson.current_source_dir().replace('\\', '/') + '/saru-ssh-agent.pdf')
package_config_data.set('SARUGithubLink', meson.current_source_dir().replace('\\', '/') + '/SARU.url')

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
package_file_name = 'SARU-' + meson.project_version() + '-' + config_data.get('SARU_ARCHITECTURE')

msi_package = custom_target(
  'generate_installer',
  input: [
    build_config,
    'saru.wxs',
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
    '-d', 'Platform=' + config_data.get('SARU_ARCHITECTURE'),
    '-arch', config_data.get('SARU_ARCHITECTURE'),
    '-d', 'Configuration=Release',
    '-i', meson.current_build_dir(),
  ],
  depends: [
    saru,
    saru_npp,
    saru_dcc,
    saru_hvh,
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
    saru,
    saru_npp,
    saru_dcc,
    saru_hvh,
  ],
  build_by_default : true,
)