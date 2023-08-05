# Copyright 2022 Google LLC
# SPDX-License-Identifier: MIT

project(
  'egltest',
  'c',
  version: '0.1',
  license: 'MIT',
  meson_version: '>=0.53',
  default_options: ['c_std=c11', 'warning_level=3'],
)

fs = import('fs')
prog_python = import('python').find_installation('python3')

cc = meson.get_compiler('c')

warning_args = cc.get_supported_arguments([
  '-Wno-pedantic',
  '-Wno-missing-field-initializers',
  '-Wno-unused-parameter',
])

dep_dl = cc.find_library('dl')
dep_m = cc.find_library('m', required: false)
dep_sdl2 = dependency('sdl2', required: false)
dep_nativewindow = cc.find_library('nativewindow', required: host_machine.system() == 'android')

dep_gbm = dependency('gbm', required: host_machine.system() != 'android')

add_project_arguments(['-D_GNU_SOURCE', warning_args], language: 'c')

idep_eglutil = declare_dependency(
  sources: ['eglutil.h'],
  dependencies: [dep_dl, dep_m, dep_gbm, dep_nativewindow],
  include_directories: ['include'],
)

tests = [
  'clear',
  'fbo',
  'formats',
  'image',
  'info',
  'tex',
  'timestamp',
  'tri',
]

if dep_sdl2.found()
  tests += ['sdl']
endif

foreach t : tests
  test_incs = []

  foreach a : ['t:vert', 't:tesc', 't:tese', 't:geom', 't:frag', 't:comp', 'b:ppm']
    asset = a.split(':')
    mode = asset[0]
    suffix = asset[1]

    src = t + '.' + suffix
    dst = t + '_test.' + suffix + '.inc'
    if fs.exists(src)
      test_incs += custom_target(
        dst,
        input: ['hexdump.py', src],
        output: [dst],
        command: [prog_python, '@INPUT0@', mode, '@INPUT1@', '@OUTPUT@'],
      )
    endif
  endforeach

  test_deps = [idep_eglutil]
  if t == 'sdl'
    test_deps += [dep_sdl2]
  endif

  executable(
    t,
    sources: [t + '.c', test_incs],
    dependencies: test_deps,
  )
endforeach
