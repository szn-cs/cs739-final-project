set(TEST_FOLDER "${PROJECT_SOURCE_DIR}/test")

set(SERVER_BINARY_NAME app)

set(SERVER_SRC 
  ${SOURCE_FOLDER}/entrypoint.cc
  ${SOURCE_FOLDER}/app.cc
  ${SOURCE_FOLDER}/utility.cc
  # ${SOURCE_FOLDER}/consensus.cc
)

include(CheckFunctionExists)

add_executable(${SERVER_BINARY_NAME} 
  ${SERVER_SRC}
  ${hw_proto_srcs_interface} 
  ${hw_grpc_srcs_interface}
)
# NOTE: ordering of libraries is important !
target_link_libraries(${SERVER_BINARY_NAME} PUBLIC
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

target_include_directories(${SERVER_BINARY_NAME} PRIVATE ${TERMCOLOR_INCLUDE_DIRS})

target_compile_options(${SERVER_BINARY_NAME} PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++20 -lssl -lz -lcrypto -ldl -lpthread -fPIC -pthread -fuse-ld=gold -O0 -Wall -g -D_FILE_OFFSET_BITS=64 -Wextra -Wzero-as-null-pointer-constant -Wextra -Wno-unused -Wno-unused-parameter>)

# optimized: 
# target_compile_options(${SERVER_BINARY_NAME} PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++20 -Wall -g -O3 -D_FILE_OFFSET_BITS=64 -Wextra -Wzero-as-null-pointer-constant -Wextra -Wno-unused -Wno-unused-parameter>)

# Gives us access to channels for safe and easy communication with threads/async functions
include(FetchContent)
if (NOT channel_POPULATED)
    FetchContent_Declare(channel URL https://github.com/andreiavrammsd/cpp-channel/archive/v0.7.3.zip)
    FetchContent_Populate(channel)
    include_directories(${channel_SOURCE_DIR}/include)
    # OR
    # add_subdirectory(${channel_SOURCE_DIR}/)
endif ()


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

# include headers --------------------------------
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include) # include headers - simplifies C++ include statements; 
include_directories(${PROJECT_SOURCE_DIR}/dependency/variadic_table/include)
include_directories(${PROJECT_SOURCE_DIR}/dependency/NuRaft/include)
include_directories(${PROJECT_SOURCE_DIR}/dependency/NuRaft/include/libnuraft)

# wait for NuRaft project target binaries
add_dependencies(${SERVER_BINARY_NAME} static_lib) 
add_dependencies(${SERVER_BINARY_NAME} shared_lib) 
# produce executable --------------------------------------------------------
install(TARGETS ${SERVER_BINARY_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
