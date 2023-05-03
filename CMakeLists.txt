# TODO: https://stackoverflow.com/questions/7724569/debug-vs-release-in-cmake

cmake_minimum_required(VERSION 3.26)
set(CMAKE_CXX_STANDARD 20)

## When using vcpkg as a submodule of your project, you can add the following to your CMakeLists.txt before the first project() call, instead of passing CMAKE_TOOLCHAIN_FILE to the cmake invocation. This will still allow people to not use vcpkg, by passing the CMAKE_TOOLCHAIN_FILE directly, but it will make the configure-build step slightly easier.
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/dependency/vcpkg/scripts/buildsystems/vcpkg.cmake"
CACHE STRING "Vcpkg toolchain file")

project (replicated_db
  VERSION 1.0.0
  LANGUAGES C CXX
  DESCRIPTION "Replicated Database Project."
)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/config/cmake;${CMAKE_MODULE_PATH}")
set(CMAKE_BUILD_TYPE RelWithDebInfo) # debug
set(SOURCE_FOLDER "${PROJECT_SOURCE_DIR}/src")

# print debug information 
# include(./config/PrintVariables.cmake) # log cmake variables

# include(CTest)
# enable_testing()


# packages ----------------------------
    # find vcpkg packages
    find_package(gRPC CONFIG REQUIRED)
    message(STATUS "Using gRPC ${gRPC_VERSION}")

    # Boost package
    # install correct version https://www.appsloveworld.com/cplus/100/197/cmake-new-boost-version-may-have-incorrect-or-missing-dependencies-and-imported
    find_package(Boost 1.81.0 COMPONENTS program_options filesystem REQUIRED)
    include_directories(${BOOST_INCLUDE_DIRS})
    message("📚 boost lib: ${Boost_LIBRARIES}")
    message("📚 boost inc: ${Boost_INCLUDE_DIR}")

    # Termcolor package
    find_path(TERMCOLOR_INCLUDE_DIRS "termcolor/termcolor.hpp")

    # Benchmark package 
    find_package(benchmark CONFIG REQUIRED)

#-------------------------------------------

# TODO: expose client header functions once build moves to a library rather than an executable

# extend cmake config ----------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists_interface.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists_app.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists_test.cmake)

#-------------------------------------------

# CPACK steers build files to proper paths (maybe used later in build process)
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

option(ENABLE_ASAN "Enable AddressSanitizer, a fast memory error detector based on compiler instrumentation" OFF)
if(ENABLE_ASAN)
    target_compile_options(${PROJECT_NAME} PUBLIC -fsanitize=address)
    target_link_options(${PROJECT_NAME} PUBLIC -fsanitize=address)
endif()

option(ENABLE_UBSAN "Enable UBSanitizer, undefined behaviour detector based on compiler instrumentation" OFF)
if(ENABLE_UBSAN)
    target_compile_options(${PROJECT_NAME} PUBLIC -fsanitize=undefined)
    target_link_options(${PROJECT_NAME} PUBLIC -fsanitize=undefined)
endif()
