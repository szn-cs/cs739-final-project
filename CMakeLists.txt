cmake_minimum_required(VERSION 3.26)
set(CMAKE_CXX_STANDARD 20)
set(compile_flags_debug_variable $<$<COMPILE_LANGUAGE:CXX>:-std=c++20 -lssl -lz -lcrypto -ldl -lpthread -fPIC -pthread -fuse-ld=gold -O0 -Wall -g -D_FILE_OFFSET_BITS=64 -Wextra -Wno-zero-as-null-pointer-constant -Wno-unused -Wno-unused-parameter>)
set(compile_flags_release_variable $<$<COMPILE_LANGUAGE:CXX>:-std=c++20 -lssl -lz -lcrypto -ldl -lpthread -fPIC -pthread -fuse-ld=gold -O3 -Wall -D_FILE_OFFSET_BITS=64 -Wextra -Wno-zero-as-null-pointer-constant -Wno-unused -Wno-unused-parameter>)
message(STATUS "build type: '${CMAKE_BUILD_TYPE}'")
message("Generated with config types: ${CMAKE_CONFIGURATION_TYPES}")

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/dependency/vcpkg/scripts/buildsystems/vcpkg.cmake" ## When using vcpkg as a submodule of your project, you can add the following to your CMakeLists.txt before the first project() call, instead of passing CMAKE_TOOLCHAIN_FILE to the cmake invocation. This will still allow people to not use vcpkg, by passing the CMAKE_TOOLCHAIN_FILE directly, but it will make the configure-build step slightly easier.
CACHE STRING "Vcpkg toolchain file")

project (distributed_project VERSION 1.0.0 LANGUAGES C CXX DESCRIPTION "distributed_project")

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/config/cmake;${CMAKE_MODULE_PATH}") # NOTE: must be defined after `project` call
set(SOURCE_FOLDER "${PROJECT_SOURCE_DIR}/src")

#-------------------------------------------

IF(APPLE)
  include_directories(${VCPKG_INSTALLED_DIR}/x64-osx/include/openssl)
  include_directories(${VCPKG_INSTALLED_DIR}/x64-osx/include)
ELSE()
  include_directories(${VCPKG_INSTALLED_DIR}/x64-linux/include/openssl)
  include_directories(${VCPKG_INSTALLED_DIR}/x64-linux/include)
ENDIF()


include_directories(${BOOST_INCLUDE_DIRS})

include_directories(${PROJECT_SOURCE_DIR}/dependency/NuRaft/include)
include_directories(${PROJECT_SOURCE_DIR}/dependency/NuRaft/include/libnuraft)

#-------------------------------------------

find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

find_package(Boost 1.81.0 COMPONENTS program_options filesystem REQUIRED) # install correct version https://www.appsloveworld.com/cplus/100/197/cmake-new-boost-version-may-have-incorrect-or-missing-dependencies-and-imported
message("📚 Boost lib: ${Boost_LIBRARIES}")

find_path(TERMCOLOR_INCLUDE_DIRS "termcolor/termcolor.hpp")

find_package(benchmark CONFIG REQUIRED)

find_package(OpenSSL REQUIRED)
find_package(asio CONFIG REQUIRED)
# NOTE: NOT WORKING ! keeps finding the vcpkg path instead of global, overriding using the full path is used instead 
find_library(SSL_LIBRARY NAMES ssl REQUIRED DOC "SSL library") 
message("📚 asio inc: ${asio}")
message("📚 openssl inc: ${OpenSSL}")
message("📚 libssl inc: ${SSL_LIBRARY}")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependency/NuRaft)
# NOTE: CMake if `.so` shared library & `.a` shared library in same folder, will always no matter what prefer `.so`
# pick static version by removing .so and adding to find_library `NAMES "libnuraft.a"` instead
# NOTE: for some reason PATH doesn't work
find_library(NURAFT_LIBRARY NAMES nuraft PATHS ${CMAKE_CURRENT_BINARY_DIR}/dependency/NuRaft/ NO_DEFAULT_PATH DOC "ebay/NuRaft library") # looking for libnuraft.a for static & .so for dynamic
if (NOT NURAFT_LIBRARY)
  set(NURAFT_LIBRARY ${CMAKE_CURRENT_BINARY_DIR}/dependency/NuRaft/libnuraft.so)
endif()
message("📚 nuraft inc: ${NURAFT_LIBRARY}")

# #-------------------------------------------

# include(./config/PrintVariables.cmake) # log cmake variables (print debug information)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists_interface.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists_app.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists_test.cmake)
