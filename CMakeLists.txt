cmake_minimum_required(VERSION 3.26)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/dependency/vcpkg/scripts/buildsystems/vcpkg.cmake" ## When using vcpkg as a submodule of your project, you can add the following to your CMakeLists.txt before the first project() call, instead of passing CMAKE_TOOLCHAIN_FILE to the cmake invocation. This will still allow people to not use vcpkg, by passing the CMAKE_TOOLCHAIN_FILE directly, but it will make the configure-build step slightly easier.
CACHE STRING "Vcpkg toolchain file")

project (distributed_project VERSION 1.0.0 LANGUAGES C CXX DESCRIPTION "distributed_project")

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/config/cmake;${CMAKE_MODULE_PATH}")
set(CMAKE_BUILD_TYPE RelWithDebInfo) # debug
set(SOURCE_FOLDER "${PROJECT_SOURCE_DIR}/src")

find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

find_package(Boost 1.81.0 COMPONENTS program_options filesystem REQUIRED) # install correct version https://www.appsloveworld.com/cplus/100/197/cmake-new-boost-version-may-have-incorrect-or-missing-dependencies-and-imported
message("📚 Boost lib: ${Boost_LIBRARIES}")

find_path(TERMCOLOR_INCLUDE_DIRS REQUIRED "termcolor/termcolor.hpp")

find_package(benchmark CONFIG REQUIRED)
message("📚 benchmark inc: ${benchmark}")

find_package(OpenSSL REQUIRED)
find_package(asio CONFIG REQUIRED)
# NOTE: NOT WORKING ! keeps finding the vcpkg path instead of global, overriding using the full path is used instead 
find_library(SSL_LIBRARY NAMES ssl NO_DEFAULT_PATH REQUIRED DOC "SSL library") 
# NOTE: CMake if `.so` shared library & `.a` shared library in same folder, will always no matter what prefer `.so`
# pick static version by removing .so and adding to find_library `NAMES "libnuraft.a"` instead
# NOTE: for some reason PATH doesn't work
find_library(NURAFT_LIBRARY NAMES nuraft NO_DEFAULT_PATH PATHS ${CMAKE_CURRENT_BINARY_DIR}/dependency/NuRaft/ REQUIRED DOC "ebay/NuRaft library") # looking for libnuraft.a for static & .so for dynamic
message("📚 asio inc: ${asio}")
message("📚 openssl inc: ${OpenSSL}")
message("📚 libssl inc: ${SSL_LIBRARY}")
message("📚 nuraft inc: ${NURAFT_LIBRARY}")

# #-------------------------------------------

include_directories(${BOOST_INCLUDE_DIRS})

include_directories(${VCPKG_INSTALLED_DIR}/x64-linux/include)
include_directories(${PROJECT_SOURCE_DIR}/dependency/NuRaft/include)
include_directories(${PROJECT_SOURCE_DIR}/dependency/NuRaft/include/libnuraft)

#-------------------------------------------

# include(./config/PrintVariables.cmake) # log cmake variables (print debug information)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists_packages.cmake)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependency/NuRaft)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists_interface.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists_app.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists_test.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt)

