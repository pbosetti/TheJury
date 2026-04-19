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

add_library(ppa_service_lib
  src/config/ServiceConfig.cpp
  src/api/Handlers.cpp
  src/semantic/OllamaClient.cpp
)

target_include_directories(ppa_service_lib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
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
