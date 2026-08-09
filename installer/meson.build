python3 = find_program('python3', required: true)

package_file_name = 'SARU-' + meson.project_version() + '-' + config_data.get('SARU_ARCHITECTURE')
unsigned_msi_file_name = 'unsigned-' + package_file_name + '.msi'
signed_binary_dir = join_paths(meson.current_build_dir(), 'signed').replace('\\', '/')
release_dir = meson.current_build_dir().replace('\\', '/')

sign_config_data = configuration_data()
sign_config_data.set('SARUExePath', saru.full_path().replace('\\', '/'))
sign_config_data.set('SARUNPPExePath', saru_npp.full_path().replace('\\', '/'))
sign_config_data.set('SARUDCCExePath', saru_dcc.full_path().replace('\\', '/'))
sign_config_data.set('SARUHVHExePath', saru_hvh.full_path().replace('\\', '/'))
sign_config_data.set('SIGNED_BINARY_DIR', signed_binary_dir)
sign_config_data.set('UNSIGNED_MSI_PATH', join_paths(release_dir, unsigned_msi_file_name))
sign_config_data.set('RELEASE_DIRECTORY', release_dir)
sign_config_data.set('MSI_FILE_NAME', package_file_name + '.msi')
sign_config_data.set('LICENSEFilePath', meson.source_root().replace('\\', '/') + '/LICENSE')
sign_config_data.set('SARUDocFilePath', meson.current_source_dir().replace('\\', '/') + '/saru-ssh-agent.pdf')
sign_config_data.set('SARUGithubLink', meson.current_source_dir().replace('\\', '/') + '/SARU.url')

sign_manifest = configure_file(
  input: 'manifest.json.in',
  output: 'manifest.json',
  configuration: sign_config_data
)

signed_binaries = custom_target(
  'sign_binaries',
  input: [sign_manifest, saru, saru_npp, saru_dcc, saru_hvh],
  output: 'signed-binaries.stamp',
  command: [python3, meson.current_source_dir() / 'sign-binaries.py', '@INPUT0@', '--group', 'binaries', '--stamp', '@OUTPUT@'],
  build_always_stale: true,
)

package_config_data = configuration_data()
package_config_data.set('VERSION', meson.project_version())
package_config_data.set('SARUExePath', join_paths(signed_binary_dir, 'SARU.exe'))
package_config_data.set('SARUNPPExePath', join_paths(signed_binary_dir, 'saru-npp.exe'))
package_config_data.set('SKYKDCCExePath', join_paths(signed_binary_dir, 'saru-dcc.exe'))
package_config_data.set('SARUHVHExePath', join_paths(signed_binary_dir, 'saru-hvh.exe'))
package_config_data.set('SARU_ARCHITECTURE', saru_architecture)

build_config = configure_file(
  input: 'buildInfo.wxi.in',
  output: 'buildInfo.wxi',
  configuration: package_config_data
)

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
    'gpl-3.0.rtf',
    signed_binaries,
  ],
  output: [
  unsigned_msi_file_name,
  'unsigned-' + package_file_name + '.wixpdb',
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
)

signed_msi = custom_target(
  'sign_installer',
  input: [sign_manifest, msi_package],
  output: 'signed-installer.stamp',
  command: [python3, meson.current_source_dir() / 'sign-binaries.py', '@INPUT0@', '--group', 'packages', '--stamp', '@OUTPUT@'],
)

validate_installer = custom_target(
  'validate_installer',
  input: signed_msi,
  output: 'validation_report.txt',
  command: [
    'wix.exe',
    'msi',
    'validate',
    join_paths(release_dir, package_file_name + '.msi'),
  ],
  console: true,
)

zip_package = custom_target(
  'create_zip_package',
  input: [sign_manifest, signed_binaries],
  output: package_file_name + '.zip',
  command: [
    python3,
    meson.current_source_dir() / 'create_zip.py',
    '@INPUT0@',
    '@OUTPUT@',
  ],
)

gpg_msi_signature = custom_target(
  'gpg_sign_installer',
  input: signed_msi,
  output: package_file_name + '.msi.asc',
  command: [python3, meson.current_source_dir() / 'gpg-sign.py', join_paths(release_dir, package_file_name + '.msi'), '@OUTPUT@'],
)

gpg_zip_signature = custom_target(
  'gpg_sign_zip',
  input: zip_package,
  output: package_file_name + '.zip.asc',
  command: [python3, meson.current_source_dir() / 'gpg-sign.py', '@INPUT@', '@OUTPUT@'],
)

alias_target('package-release', [validate_installer, gpg_msi_signature, gpg_zip_signature])
