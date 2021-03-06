# vim: fdm=marker fdl=0

# usage:
# scons [-j n]: compile in release mode
# scons debug=1 [-j n]: compile in debug mode
#
# author: heconghui@gmail.com

import os

# compiler options
compiler_set        = 'gnu' # intel, gnu, sw, swintel
debug_mode          = 0
additional_includes = [os.environ['HOME'] + '/tar/boost_1_61_0/',]
additional_libpath  = []
additional_libs     = []

if compiler_set == 'sw':#{{{
  c_compiler      = ['mpicc', '-host']
  cxx_compiler    = ['mpiCC', '-host']
  linker          = ['swaCC']
  warn_flags      = ['-Wno-write-strings']
  optimize_flags  = ['-O2', '-OPT:IEEE_arith=2']
  debug_flags     = ['-O0', '-g']
  other_flags     = ['-DNO_BLAS', '-DMPICH_IGNORE_CXX_SEEK']
  link_flags      = ['-O1']
#}}}
elif compiler_set == 'swintel':#{{{
  c_compiler      = ['mpiicc',  '-openmp']
  cxx_compiler    = ['mpiicpc', '-openmp']
  linker          = cxx_compiler
  warn_flags      = ['-Wall']
  optimize_flags  = ['-O2']
  debug_flags     = ['-O0', '-g']
  other_flags     = ['-DNO_BLAS', '-DMPICH_IGNORE_CXX_SEEK']
  link_flags      = ['-O1']
#}}}
elif compiler_set == 'gnu':#{{{
  c_compiler      = ['mpicc',  '-cc=gcc',  '-fopenmp', '-DUSE_OPENMP']
  cxx_compiler    = ['mpicxx', '-cxx=g++', '-fopenmp', '-DUSE_OPENMP']
  linker          = cxx_compiler
  warn_flags      = ['-Wall', '-Wextra', '-Wno-write-strings']
  optimize_flags  = ['-O2']
  debug_flags     = ['-O0', '-g']
  other_flags     = ['-DNO_BLAS', '-DMPICH_IGNORE_CXX_SEEK']
  link_flags      = ['-O1']#, '-Wl', '--hash-style=sysv']
#}}}
elif compiler_set == 'intel':#{{{
  c_compiler      = ['mpicc',  '-cc=icc',   '-openmp']
  cxx_compiler    = ['mpicxx', '-cxx=icpc', '-openmp']
  linker          = cxx_compiler
  warn_flags      = ['-Wall']
  optimize_flags  = ['-O2']
  debug_flags     = ['-O0', '-g']
  other_flags     = ['-DNO_BLAS', '-DMPICH_IGNORE_CXX_SEEK']
  link_flags      = ['-O1']
else:
  print 'Only GNU, Intel, SW, SWIntel Compilers are supported now'
  Exit(-1)
#}}}
# set the sub directories (key, value), where value is the name of directory#{{{
# please make sure the source code are in src subdirectory
dirlist = [
   ('lib', 'lib'),
   ('bin', 'bin'),
   ('fwi', 'src/fwi'),
   ('essfwi', 'src/essfwi'),
   ('enfwi', 'src/enfwi'),
   ('tool', 'src/tool'),
   ('common', 'src/common'),
   ('modeling', 'src/modeling'),
   ('rsf', 'src/rsf'),
]
dirs = dict(dirlist)
#}}}
# choose debug/release flags#{{{
is_debug_mode = ARGUMENTS.get('debug', debug_mode)
if int(is_debug_mode):
  print 'Debug mode'
  cur_cflags = debug_flags + warn_flags + other_flags
else:
  print 'Release mode'
  cur_cflags = optimize_flags + warn_flags + other_flags
#}}}
# set includes and libs#{{{
inc_path          = []
libpath           = ['#' + dirs['lib'], '#scalapack', '#rsf']
scalapack_gnu_lib = ['scalapack-gnu', 'lapack-gnu', 'refblas-gnu', 'gfortran'] # don't change the order
scalapack_sw_lib =  ['scalapack-sw', 'lapack-sw', 'refblas-sw'] # don't change the order
libs              = []

if compiler_set == 'sw':
  libs = scalapack_sw_lib
else:
  libs = scalapack_gnu_lib
for inc in additional_includes:
  inc_path += ['-isystem', inc]
for lib in additional_libs:
  libs += [lib]
for path in additional_libpath:
  libpath += [path]
#}}}
# setup environment#{{{
env = Environment(CC      = c_compiler,
                  CXX     = cxx_compiler,
                  LINK    = linker + link_flags,
                  CCFLAGS = cur_cflags + inc_path,
                  LIBPATH = libpath,
                  LIBS    = libs,
                  ENV     = os.environ)
#}}}
# compile#{{{
for d in dirlist:
  if not 'src/' in d[1]:
    continue

  SConscript(d[1] + '/SConscript',
             variant_dir = d[1].replace('src', 'build'),
             duplicate = 0,
             exports = 'env dirs compiler_set c_compiler cxx_compiler')
#}}}
