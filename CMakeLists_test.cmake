#---SERVER------------------------------------------------
set(TEST_FOLDER "${PROJECT_SOURCE_DIR}/test")

set(SRC_FILES 
${SOURCE_FOLDER}/app.cc
${SOURCE_FOLDER}/misc/utility.cc
${TEST_FOLDER}/test.cc
)
set(SERVER_BINARY_NAME 
  test
)

include(CheckFunctionExists)

add_executable(${SERVER_BINARY_NAME} 
  ${SRC_FILES}
  ${hw_proto_srcs_interface} 
  ${hw_grpc_srcs_interface}
)
target_link_libraries(${SERVER_BINARY_NAME} 
  ${_REFLECTION} 
  ${_GRPC_GRPCPP} 
  ${_PROTOBUF_LIBPROTOBUF} 
  stdc++fs
  # gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts

  ### Boost package
  ${Boost_LIBRARIES}

  ## Benchmark
  benchmark::benchmark benchmark::benchmark_main
)

target_include_directories(${SERVER_BINARY_NAME} PRIVATE ${TERMCOLOR_INCLUDE_DIRS})

target_compile_options(${SERVER_BINARY_NAME} PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++20 -Wall -O0 -D_FILE_OFFSET_BITS=64 -Wextra -Wzero-as-null-pointer-constant -Wextra -Wno-unused -Wno-unused-parameter>)

# optimized: 
# target_compile_options(${SERVER_BINARY_NAME} PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++20 -Wall -g -O3 -D_FILE_OFFSET_BITS=64 -Wextra -Wzero-as-null-pointer-constant -Wextra -Wno-unused -Wno-unused-parameter>)

check_function_exists(fallocate HAVE_FALLOCATE)
check_function_exists(fallocate HAVE_FLOCK)
check_function_exists(utimensat HAVE_UTIMENSAT)
check_function_exists(setxattr HAVE_XATTR)
if (${HAVE_FALLOCATE})
    target_compile_definitions(${SERVER_BINARY_NAME} PUBLIC HAVE_FALLOCATE)
endif ()
if (${HAVE_FLOCK})
    target_compile_definitions(${SERVER_BINARY_NAME} PUBLIC HAVE_FLOCK)
endif ()
if (${HAVE_UTIMENSAT})
    target_compile_definitions(${SERVER_BINARY_NAME} PUBLIC HAVE_UTIMENSAT)
endif ()
if (${HAVE_XATTR})
    target_compile_definitions(${SERVER_BINARY_NAME} PUBLIC HAVE_XATTR)
endif ()


install(TARGETS ${SERVER_BINARY_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
#--------------------------------------------------------





