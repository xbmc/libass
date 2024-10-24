cmake_minimum_required(VERSION 3.15)

project(libass VERSION 0.15.2 LANGUAGES C)

find_package(iconv REQUIRED NO_MODULE)
find_package(freetype 2.6 REQUIRED NO_MODULE)
find_package(fribidi REQUIRED NO_MODULE)
find_package(harfbuzz REQUIRED NO_MODULE)

include(FindGit)
if(GIT_EXECUTABLE AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/.git)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --long --always --dirty --broken --abbrev=40
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
       OUTPUT_VARIABLE OUT RESULT_VARIABLE RES)
   IF(RES EQUAL 0)
    string(REGEX REPLACE "\n$" "" VCS_TAG "${OUT}")
  endif()
else()
  set(VCS_TAG "")
endif()

add_library(libass 
  libass/ass_bitmap.c
  libass/ass_bitmap.h
  libass/ass_bitmap_engine.c
  libass/ass_bitmap_engine.h
  libass/ass_blur.c
  libass/ass_cache.c
  libass/ass_cache.h
  libass/ass_cache_template.h
  libass/ass_compat.h
  libass/ass_drawing.c
  libass/ass_drawing.h
  libass/ass_filesystem.h
  libass/ass_filesystem.c
  libass/ass_font.c
  libass/ass_font.h
  libass/ass_fontselect.c
  libass/ass_fontselect.h
  libass/ass_library.c
  libass/ass_library.h
  libass/ass_outline.c
  libass/ass_outline.h
  libass/ass_parse.c
  libass/ass_parse.h
  libass/ass_rasterizer.c
  libass/ass_rasterizer.h
  libass/ass_render.c
  libass/ass_render.h
  libass/ass_render_api.c
  libass/ass_shaper.c
  libass/ass_shaper.h
  libass/ass_string.c
  libass/ass_string.h
  libass/ass_strtod.c
  libass/ass_types.h
  libass/ass_utils.c
  libass/ass_utils.h
  libass/ass.c
  libass/ass.h
  libass/wyhash.h
  libass/c/blur_template.h
  libass/c/c_be_blur.c
  libass/c/c_blend_bitmaps.c
  libass/c/c_blur.c
  libass/c/c_rasterizer.c
  libass/c/rasterizer_template.h
)

target_link_libraries(libass
  PRIVATE
  freetype::freetype
  fribidi::fribidi
  iconv::iconv
  harfbuzz::harfbuzz
)
target_compile_definitions(libass
  PRIVATE
    CONFIG_RASTERIZER
    BUILDING_LIBASS
    CONFIG_ICONV
    CONFIG_SOURCEVERSION="${VCS_TAG}"
)

if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(libass PRIVATE ASS_STATIC)
endif()

target_include_directories(libass
  INTERFACE
  $<INSTALL_INTERFACE:include/ass>
)

if(WINDOWS_STORE)
  target_compile_definitions(libass
    PRIVATE
      MS_APP
  )
  target_compile_options(libass
    PRIVATE
      /sdl-
  )
endif()

if(MSVC)
  target_sources(libass PRIVATE
    libass/dwrite_c.h
    libass/ass_directwrite.h
    libass/ass_directwrite.c
  )
  target_compile_definitions(libass PRIVATE
    CONFIG_DIRECTWRITE
  )
  target_link_libraries(libass PRIVATE dwrite.lib)
set(CMAKE_DEBUG_POSTFIX "d")
endif()

set(libass_VERSION ${PROJECT_VERSION})
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/libass-config-version.cmake
  VERSION ${libass_VERSION}
  COMPATIBILITY AnyNewerVersion
)

install(TARGETS libass EXPORT libass
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib)


install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass_render.h
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass_shaper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass_types.h
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass_utils.h
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass.h
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass_bitmap.h
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass_bitmap_engine.h
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass_cache.h
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass_cache_template.h
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass_drawing.h
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass_filesystem.h
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass_font.h
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass_fontconfig.h
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass_library.h
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/libass/ass_rasterizer.h
  DESTINATION include/ass)

install(EXPORT libass
  FILE
    libass.cmake
  NAMESPACE
    libass::
  DESTINATION
    lib/cmake/libass
)
install(
  FILES
    cmake/libass-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/libass-config-version.cmake
  DESTINATION
    lib/cmake/libass
)

if(MSVC AND BUILD_SHARED_LIBS)
  install(FILES
    $<TARGET_PDB_FILE:libass>
    DESTINATION lib
  )
endif()
