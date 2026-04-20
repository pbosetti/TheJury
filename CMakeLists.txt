cmake_minimum_required(VERSION 3.21)

set(THEJURY_DEFAULT_VERSION_TAG "v0.1.0")
set(THEJURY_VERSION_TAG "${THEJURY_DEFAULT_VERSION_TAG}")

find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} tag --sort=-v:refname
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE THEJURY_GIT_TAGS
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  string(REPLACE "\n" ";" THEJURY_GIT_TAG_LIST "${THEJURY_GIT_TAGS}")
  foreach(tag IN LISTS THEJURY_GIT_TAG_LIST)
    if(tag MATCHES "^v[0-9]+\\.[0-9]+\\.[0-9]+$")
      set(THEJURY_VERSION_TAG "${tag}")
      break()
    endif()
  endforeach()
endif()

if(THEJURY_VERSION_TAG MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
  set(THEJURY_VERSION_MAJOR "${CMAKE_MATCH_1}")
  set(THEJURY_VERSION_MINOR "${CMAKE_MATCH_2}")
  set(THEJURY_VERSION_PATCH "${CMAKE_MATCH_3}")
else()
  message(FATAL_ERROR "Resolved version tag '${THEJURY_VERSION_TAG}' is not in vX.Y.Z format")
endif()

set(THEJURY_VERSION_STRING "${THEJURY_VERSION_MAJOR}.${THEJURY_VERSION_MINOR}.${THEJURY_VERSION_PATCH}")

project(lightroom_ppa_critique VERSION ${THEJURY_VERSION_STRING} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

configure_file(
  ${CMAKE_SOURCE_DIR}/cmake/Version.hpp.in
  ${CMAKE_BINARY_DIR}/generated/include/ppa/runtime/Version.hpp
  @ONLY
)

configure_file(
  ${CMAKE_SOURCE_DIR}/cmake/Info.lua.in
  ${CMAKE_SOURCE_DIR}/plugin/PpaCritique.lrplugin/Info.lua
  @ONLY
)

configure_file(
  ${CMAKE_SOURCE_DIR}/cmake/PluginVersion.lua.in
  ${CMAKE_SOURCE_DIR}/plugin/PpaCritique.lrplugin/PluginVersion.lua
  @ONLY
)

add_subdirectory(service)

install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/plugin/PpaCritique.lrplugin
  DESTINATION .
  COMPONENT TheJuryRelease
)

install(
  FILES
    ${CMAKE_SOURCE_DIR}/INSTALL.md
  DESTINATION .
  COMPONENT TheJuryRelease
)
