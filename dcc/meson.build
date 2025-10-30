skym_dcc_rc = configure_file(
  input: 'skym-dcc.rc.cmake',
  output: 'skym-dcc.rc',
  configuration: config_data
)
skym_dcc_rc_object = windows.compile_resources(skym_dcc_rc)
skym_dcc = executable('skym-dcc',
 ['main.cpp', 'dcc_generator.cpp', skym_dcc_rc_object],
 dependencies: [ssha]
)