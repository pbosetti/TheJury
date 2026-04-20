include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
  cpp_httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.16.3
)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.8.1
)
FetchContent_Declare(
  tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG v3.4.0
)

set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(cpp_httplib nlohmann_json catch2 tomlplusplus)

configure_file(
  ${CMAKE_SOURCE_DIR}/ppa_service.toml.example
  ${CMAKE_CURRENT_BINARY_DIR}/ppa_service.toml
  COPYONLY
)

if(APPLE)
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(THEJURY_PLUGIN_BIN_DIR "macos-arm64")
  else()
    set(THEJURY_PLUGIN_BIN_DIR "macos-x86_64")
  endif()
elseif(WIN32)
  set(THEJURY_PLUGIN_BIN_DIR "windows-x86_64")
else()
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" THEJURY_PROCESSOR)
  set(THEJURY_PLUGIN_BIN_DIR "linux-${THEJURY_PROCESSOR}")
endif()

set(THEJURY_PLUGIN_BIN_ROOT "${CMAKE_SOURCE_DIR}/plugin/PpaCritique.lrplugin/bin/${THEJURY_PLUGIN_BIN_DIR}")

add_library(ppa_service_lib
  src/config/ServiceConfig.cpp
  src/api/Handlers.cpp
  src/runtime/RuntimeManager.cpp
  src/runtime/RuntimeSupport.cpp
  src/semantic/OllamaClient.cpp
)

target_include_directories(ppa_service_lib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated/include
)

target_link_libraries(ppa_service_lib
  PUBLIC
    httplib::httplib
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
)

target_compile_features(ppa_service_lib PUBLIC cxx_std_20)

add_executable(ppa_service
  src/main.cpp
)

target_link_libraries(ppa_service PRIVATE ppa_service_lib)

add_executable(ppa_service_host
  src/host_main.cpp
)

target_link_libraries(ppa_service_host PRIVATE ppa_service_lib)

add_custom_command(
  TARGET ppa_service
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${THEJURY_PLUGIN_BIN_ROOT}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:ppa_service> ${THEJURY_PLUGIN_BIN_ROOT}/
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/ppa_service.toml ${THEJURY_PLUGIN_BIN_ROOT}/
)

add_custom_command(
  TARGET ppa_service_host
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${THEJURY_PLUGIN_BIN_ROOT}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:ppa_service_host> ${THEJURY_PLUGIN_BIN_ROOT}/
)

install(
  TARGETS ppa_service ppa_service_host
  RUNTIME DESTINATION PpaCritique.lrplugin/bin/${THEJURY_PLUGIN_BIN_DIR}
  COMPONENT TheJuryRelease
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/ppa_service.toml
  DESTINATION PpaCritique.lrplugin/bin/${THEJURY_PLUGIN_BIN_DIR}
  COMPONENT TheJuryRelease
)

if(BUILD_TESTING)
  add_executable(ppa_service_tests
    tests/service_tests.cpp
  )

  target_link_libraries(ppa_service_tests
    PRIVATE
      Catch2::Catch2WithMain
      ppa_service_lib
  )

  include(Catch)
  catch_discover_tests(ppa_service_tests)
endif()
