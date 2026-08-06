saru_hvh_rc = configure_file(
  input: 'saru-hvh.rc.cmake',
  output: 'saru-hvh.rc',
  configuration: config_data
  
)
saru_hvh_rc_object = windows.compile_resources(saru_hvh_rc)

saru_hvh = executable('saru-hvh',
 ['saru-hvh.cpp', saru_hvh_rc_object],
  dependencies: [wtl, windows_unicode],
  win_subsystem: 'windows',
  link_args: ['wbemuuid.lib'],
  include_directories: include_directories('..')
)
