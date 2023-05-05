set(TEST_FOLDER "${PROJECT_SOURCE_DIR}/test")

set(BINARY_NAME test)

set(SRC_FILES 
  ${TEST_FOLDER}/test.cc
  ${SOURCE_FOLDER}/app.cc
  ${SOURCE_FOLDER}/utility.cc
  ${SOURCE_FOLDER}/consensus.cc
)

include(CheckFunctionExists)

add_executable(${BINARY_NAME} 
  ${SRC_FILES}
  ${hw_proto_srcs_interface} 
  ${hw_grpc_srcs_interface}
)

# NOTE: ordering of libraries is important !
target_link_libraries(${BINARY_NAME} PUBLIC
  ${_REFLECTION} 
  ${_GRPC_GRPCPP} 
  gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc++_alts
  ${_PROTOBUF_LIBPROTOBUF} 
  stdc++fs

  ### Boost package
  ${Boost_LIBRARIES}

  # ${NURAFT_LIBRARY} # will pick `.so` library ! instead of `.a` static library
  ${CMAKE_CURRENT_BINARY_DIR}/dependency/NuRaft/libnuraft.so # NURAFT_LIBRARY gives wrong path

  OpenSSL::SSL OpenSSL::Crypto
  asio::asio
  ${SSL_LIBRARY}
)

target_include_directories(${BINARY_NAME} PRIVATE ${TERMCOLOR_INCLUDE_DIRS})

# test specific packages
target_link_libraries(${BINARY_NAME} PUBLIC
  ## Google's Benchmark package
  benchmark::benchmark benchmark::benchmark_main
)

check_function_exists(fallocate HAVE_FALLOCATE)
check_function_exists(fallocate HAVE_FLOCK)
check_function_exists(utimensat HAVE_UTIMENSAT)
check_function_exists(setxattr HAVE_XATTR)
if (${HAVE_FALLOCATE})
    target_compile_definitions(${BINARY_NAME} PUBLIC HAVE_FALLOCATE)
endif ()
if (${HAVE_FLOCK})
    target_compile_definitions(${BINARY_NAME} PUBLIC HAVE_FLOCK)
endif ()
if (${HAVE_UTIMENSAT})
    target_compile_definitions(${BINARY_NAME} PUBLIC HAVE_UTIMENSAT)
endif ()
if (${HAVE_XATTR})
    target_compile_definitions(${BINARY_NAME} PUBLIC HAVE_XATTR)
endif ()

# include headers --------------------------------
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include) # include headers - simplifies C++ include statements;  
include_directories(${PROJECT_SOURCE_DIR}/dependency/variadic_table/include)
include_directories(${PROJECT_SOURCE_DIR}/dependency/NuRaft/include)
include_directories(${PROJECT_SOURCE_DIR}/dependency/NuRaft/include/libnuraft)

IF(CMAKE_BUILD_TYPE MATCHES Debug)
  target_compile_options(${BINARY_NAME} PUBLIC ${compile_flags_debug_variable})
ELSEIF(CMAKE_BUILD_TYPE MATCHES Release)
  target_compile_options(${BINARY_NAME} PUBLIC ${compile_flags_release_variable})
ENDIF()

# wait for NuRaft project target binaries
add_dependencies(${BINARY_NAME} static_lib) 
add_dependencies(${BINARY_NAME} shared_lib) 
# produce executable --------------------------------------------------------
install(TARGETS ${BINARY_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

get_target_property(cflags ${BINARY_NAME} COMPILE_OPTIONS)
message("The project has set the following flags: ${cflags}")



