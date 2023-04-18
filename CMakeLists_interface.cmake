# protobuf & gRPC --------------------------------------------------------
# Protobuf
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${protobuf_VERSION}")
# Protobuf-compiler
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)

# gRPC
set(_GRPC_GRPCPP gRPC::grpc++)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)

# Proto file
get_filename_component(hw_proto_consensus "${SOURCE_FOLDER}/consensus_interface.proto" ABSOLUTE)
get_filename_component(hw_proto_path_consensus "${hw_proto_consensus}" PATH)
# Generated sources
set(hw_proto_srcs_consensus "${CMAKE_CURRENT_BINARY_DIR}/consensus_interface.pb.cc")
set(hw_proto_hdrs_consensus "${CMAKE_CURRENT_BINARY_DIR}/consensus_interface.pb.h")
set(hw_grpc_srcs_consensus "${CMAKE_CURRENT_BINARY_DIR}/consensus_interface.grpc.pb.cc")
set(hw_grpc_hdrs_consensus "${CMAKE_CURRENT_BINARY_DIR}/consensus_interface.grpc.pb.h")

# Proto file 
get_filename_component(hw_proto_database "${SOURCE_FOLDER}/database_interface.proto" ABSOLUTE)
get_filename_component(hw_proto_path_database "${hw_proto_database}" PATH)
# Generated sources
set(hw_proto_srcs_database "${CMAKE_CURRENT_BINARY_DIR}/database_interface.pb.cc")
set(hw_proto_hdrs_database "${CMAKE_CURRENT_BINARY_DIR}/database_interface.pb.h")
set(hw_grpc_srcs_database "${CMAKE_CURRENT_BINARY_DIR}/database_interface.grpc.pb.cc")
set(hw_grpc_hdrs_database "${CMAKE_CURRENT_BINARY_DIR}/database_interface.grpc.pb.h")

add_custom_command(
      OUTPUT 
        "${hw_proto_srcs_consensus}" 
        "${hw_proto_hdrs_consensus}" 
        "${hw_grpc_srcs_consensus}" 
        "${hw_grpc_hdrs_consensus}" 
        "${hw_proto_srcs_database}" 
        "${hw_proto_hdrs_database}" 
        "${hw_grpc_srcs_database}" 
        "${hw_grpc_hdrs_database}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${hw_proto_path_consensus}"
        -I "${hw_proto_path_database}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${hw_proto_consensus}"
        "${hw_proto_database}"
      DEPENDS "${hw_proto_consensus}" "${hw_proto_database}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# custom_hw_grpc_proto
add_library(custom_hw_grpc_proto
  ${hw_grpc_srcs_consensus}
  ${hw_grpc_hdrs_consensus}
  ${hw_proto_srcs_consensus}
  ${hw_proto_hdrs_consensus}
  ${hw_grpc_srcs_database}
  ${hw_grpc_hdrs_database}
  ${hw_proto_srcs_database}
  ${hw_proto_hdrs_database}
)
target_link_libraries(custom_hw_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})
#--------------------------------------------------------

