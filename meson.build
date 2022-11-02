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
prog_xxd = find_program('xxd')

cc = meson.get_compiler('c')

warning_args = cc.get_supported_arguments([
  '-Wno-pedantic',
  '-Wno-missing-field-initializers',
  '-Wno-unused-parameter',
])

dep_dl = cc.find_library('dl')
dep_m = cc.find_library('m', required: false)
dep_gbm = cc.find_library('gbm', required: host_machine.system() != 'android')

add_project_arguments(['-D_GNU_SOURCE', warning_args], language: 'c')

idep_eglutil = declare_dependency(
  sources: ['eglutil.h'],
  dependencies: [dep_dl, dep_m, dep_gbm],
  include_directories: ['include'],
)

tests = [
  'clear',
]

foreach t : tests
  executable(
    t,
    sources: [t + '.c'],
    dependencies: [idep_eglutil],
  )
endforeach
