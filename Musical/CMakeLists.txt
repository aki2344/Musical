cmake_minimum_required(VERSION 3.16)
project(Musical C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(SDL2 CONFIG REQUIRED)

# SDL2_image などが SDL2::Headers を期待する場合の救済
if(NOT TARGET SDL2::Headers)
  add_library(SDL2::Headers INTERFACE IMPORTED)
  # SDL2::SDL2 が持っている include dirs を流用
  get_target_property(_sdl2_inc SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
  set_target_properties(SDL2::Headers PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_sdl2_inc}"
  )
endif()

find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(SDL2_ttf   CONFIG REQUIRED)

add_executable(Musical
  animation.c
  dynamic_font_atlas.c
  easing.c
  gamepad.c
  image.c
  key.c
  sprite.c
  mouse.c
  vector2.c
  main.c
  mainGame.c
  title.c
  particle.c
  jewelry.c
  player.c
  enemy.c
  star.c
)

target_link_libraries(Musical PRIVATE
  SDL2::SDL2
  SDL2_image::SDL2_image
  SDL2_mixer::SDL2_mixer
  SDL2_ttf::SDL2_ttf
  m
)
