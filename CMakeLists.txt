cmake_minimum_required(VERSION 3.4)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_COVERAGE "Build coverage" OFF)

set(
  HUNTER_CACHE_SERVERS
  "https://github.com/bmstu-iu8-cpp-sem-3/hunter-cache"
  CACHE STRING "Default cache server"
)

include("tools/gate/cmake/HunterGate.cmake")

huntergate(
  URL "https://github.com/ruslo/hunter/archive/v0.23.34.tar.gz"
  SHA1 "70287b1ffa810ee4e952052a9adff9b4856d0d54"
)

project(krauler)
string(APPEND CMAKE_CXX_FLAGS " -pedantic -Werror -Wall -Wextra")
string(APPEND CMAKE_CXX_FLAGS " -Wno-unused-command-line-argument")
string(APPEND CMAKE_CXX_FLAGS " -Wshadow -Wnon-virtual-dtor")

# add_definitions(-DBOOST_ASIO_SEPARATE_COMPILATION=1)
hunter_add_package(GTest)
find_package(GTest CONFIG REQUIRED)

hunter_add_package(Boost)
find_package(Boost CONFIG REQUIRED)

hunter_add_package(Beast)
find_package(Beast CONFIG REQUIRED)

hunter_add_package(OpenSSL)
find_package(OpenSSL REQUIRED)

add_library(krauler STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/sources/source.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/gumbo-parser/src/parser.c
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/gumbo-parser/src/attribute.c
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/gumbo-parser/src/char_ref.c 
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/gumbo-parser/src/error.c 
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/gumbo-parser/src/string_buffer.c 
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/gumbo-parser/src/string_piece.c
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/gumbo-parser/src/tag.c 
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/gumbo-parser/src/tokenizer.c 
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/gumbo-parser/src/utf8.c 
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/gumbo-parser/src/util.c
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/gumbo-parser/src/vector.c 
)
add_executable(tests
  ${CMAKE_CURRENT_SOURCE_DIR}/sources/source.cpp
)

target_include_directories(krauler 
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/gumbo-parser/src
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/ThreadPool
)
target_include_directories(tests 
 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/gumbo-parser/src
 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/ThreadPool
)
target_link_libraries(tests GTest::main Boost::boost Beast::Beast OpenSSL::SSL OpenSSL::Crypto krauler)
target_link_libraries(krauler Boost::boost Beast::Beast OpenSSL::SSL OpenSSL::Crypto)
enable_testing()
add_test(NAME unit_tests COMMAND tests) 

if(BUILD_COVERAGE)
  set(ENABLE_COVERAGE ON CACHE BOOL "Enable coverage build." FORCE)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
  find_package(codecov)
  add_coverage(krauler)
  add_coverage(tests)
  list(APPEND LCOV_REMOVE_PATTERNS "'${PROJECT_SOURCE_DIR}/tests/*'")
  coverage_evaluate()
endif()
