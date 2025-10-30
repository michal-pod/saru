skym_npp_rc = configure_file(
  input: 'skym-npp.rc.cmake',
  output: 'skym-npp.rc',
  configuration: config_data
)
skym_npp_rc_object = windows.compile_resources(skym_npp_rc)
skym_npp = executable('skym-npp', ['skym-npp.c', skym_npp_rc_object], link_args: ['wintrust.lib'])