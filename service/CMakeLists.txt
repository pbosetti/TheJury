include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
  cpp_httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.16.3
)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.8.1
)

set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(cpp_httplib nlohmann_json catch2)

add_library(ppa_service_lib
  src/api/Handlers.cpp
)

target_include_directories(ppa_service_lib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ppa_service_lib
  PUBLIC
    httplib::httplib
    nlohmann_json::nlohmann_json
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
