# Cmake build for libpuzzle by Henri Hyyryläinen, 2019

cmake_minimum_required(VERSION 3.0)

project(libpuzzle)

add_library(puzzle STATIC
  src/compress.c
  src/dvec.c
  src/puzzle.c
  src/puzzle.h
  src/tunables.c
  src/cvec.c
  src/globals.h
  src/puzzle_common.h
  src/puzzle_p.h
  src/vector_ops.c
  )



set_target_properties(puzzle PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
