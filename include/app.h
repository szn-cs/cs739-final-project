#pragma once

#include "library.h"
#include "rpc.h"
#include "struct.h"
#include "utility.h"

namespace app {

  void initializeStaticInstance(std::shared_ptr<utility::parse::Config> config, std::vector<std::string> addressList);
  bool createNode(const std::string &file_name, bool is_dir, uint64_t *instance_number);
  void init_consensus();

}  // namespace app

namespace app::server {

  void init_server_info();
  grpc::Status create_session(std::string);
  grpc::Status close_session(std::string);
  void maintain_session(std::shared_ptr<Session>);
  void end_session(std::shared_ptr<Session>);
  int64_t attempt_extend_session(std::string);  // TODO: Make a version that takes a map of locks, for when in jeopardy
  grpc::Status open_lock(std::string, std::string);
  grpc::Status delete_lock(std::string, std::string);
  grpc::Status acquire_lock(std::string, std::string, LockStatus);
  grpc::Status release_lock(std::string, std::string);
  std::pair<grpc::Status, std::string> read(std::string, std::string);
  grpc::Status write(std::string, std::string, std::string);
  int64_t handle_jeopardy(std::string, google::protobuf::Map<std::string, LockStatus>);
  std::string read_content(std::string path);
  bool is_exists(string path);
}  // namespace app::server

namespace app::client {

  /**
   * @brief Called to start a session with the master server
   * 
   * First, this method polls all of the servers in the cluster, sending init_session rpcs.
   * Only the master will reply to this with rpc::Status::Ok
   */
  grpc::Status start_session();
  void close_session();
  void maintain_session();
  bool open_lock(std::string);
  bool delete_lock(std::string);
  grpc::Status acquire_lock(std::string, LockStatus);
  grpc::Status release_lock(std::string);
  std::pair<grpc::Status, std::string> read(std::string);
  grpc::Status write(std::string, std::string);

}  // namespace app::client
