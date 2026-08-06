saru_npp_rc = configure_file(
  input: 'saru-npp.rc.cmake',
  output: 'saru-npp.rc',
  configuration: config_data
)
saru_npp_rc_object = windows.compile_resources(saru_npp_rc)
saru_npp = executable('saru-npp', ['saru-npp.c', saru_npp_rc_object], link_args: ['wintrust.lib'])