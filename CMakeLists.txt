cmake_minimum_required(VERSION 3.21)
project(lightroom_ppa_critique VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

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
