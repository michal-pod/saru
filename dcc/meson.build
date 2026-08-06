saru_dcc_rc = configure_file(
  input: 'saru-dcc.rc.cmake',
  output: 'saru-dcc.rc',
  configuration: config_data
)
saru_dcc_rc_object = windows.compile_resources(saru_dcc_rc)
saru_dcc = executable('saru-dcc',
 ['main.cpp', 'dcc_generator.cpp', saru_dcc_rc_object],
 dependencies: [ssha]
)