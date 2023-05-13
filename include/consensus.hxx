#pragma once

#include "debugger.hxx"
#include "library.h"
#include "log_store.hxx"
#include "rpc.h"
#include "state_machine.hxx"
#include "state_manager.hxx"
#include "struct.h"
#include "utility.h"

/*
    ____ ___  _   _ ____  _____ _   _ ____  _   _ ____    ___ _   _ ___ _____ ___    _    _     ___ _____   _  _____ ___ ___  _   _
   / ___/ _ \| \ | / ___|| ____| \ | / ___|| | | / ___|  |_ _| \ | |_ _|_   _|_ _|  / \  | |   |_ _|__  /  / \|_   _|_ _/ _ \| \ | |
  | |  | | | |  \| \___ \|  _| |  \| \___ \| | | \___ \   | ||  \| || |  | |  | |  / _ \ | |    | |  / /  / _ \ | |  | | | | |  \| |
  | |__| |_| | |\  |___) | |___| |\  |___) | |_| |___) |  | || |\  || |  | |  | | / ___ \| |___ | | / /_ / ___ \| |  | | |_| | |\  |
   \____\___/|_| \_|____/|_____|_| \_|____/ \___/|____/  |___|_| \_|___| |_| |___/_/   \_\_____|___/____/_/   \_\_| |___\___/|_| \_|
*/

namespace app::consensus {
  using namespace nuraft;

  static raft_params::return_method_type CALL_TYPE = raft_params::blocking;
  //  = raft_params::async_handler;

  static bool ASYNC_SNAPSHOT_CREATION = false;

  using raft_result = cmd_result<ptr<buffer>>;

  void init_raft(ptr<state_machine> sm_instance, std::string log_path);
  void handle_result(ptr<_TestSuite::TestSuite::Timer> timer, raft_result& result, ptr<std::exception>& err);
  void append_log(op_type, std::string& path, std::string& contents);
  void add_server(const std::vector<std::string>& tokens);
  void print_status();
  void server_list();

}  // namespace app::consensus
