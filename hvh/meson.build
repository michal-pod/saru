skym_hvh_rc = configure_file(
  input: 'skym-hvh.rc.cmake',
  output: 'skym-hvh.rc',
  configuration: config_data
  
)
skym_hvh_rc_object = windows.compile_resources(skym_hvh_rc)

skym_hvh = executable('skym-hvh',
 ['skym-hvh.cpp', skym_hvh_rc_object],
  dependencies: [wtl, windows_unicode],
  win_subsystem: 'windows',
  link_args: ['wbemuuid.lib'],
  include_directories: include_directories('..')
)
