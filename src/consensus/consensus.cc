#include "common.h"

/*
    ____ ___  _   _ ____  _____ _   _ ____  _   _ ____    ___ _   _ ___ _____ ___    _    _     ___ _____   _  _____ ___ ___  _   _
   / ___/ _ \| \ | / ___|| ____| \ | / ___|| | | / ___|  |_ _| \ | |_ _|_   _|_ _|  / \  | |   |_ _|__  /  / \|_   _|_ _/ _ \| \ | |
  | |  | | | |  \| \___ \|  _| |  \| \___ \| | | \___ \   | ||  \| || |  | |  | |  / _ \ | |    | |  / /  / _ \ | |  | | | | |  \| |
  | |__| |_| | |\  |___) | |___| |\  |___) | |_| |___) |  | || |\  || |  | |  | | / ___ \| |___ | | / /_ / ___ \| |  | | |_| | |\  |
   \____\___/|_| \_|____/|_____|_| \_|____/ \___/|____/  |___|_| \_|___| |_| |___/_/   \_\_____|___/____/_/   \_\_| |___\___/|_| \_|
*/

namespace app::consensus {
  using namespace nuraft;

  consensus_state_machine* get_sm() {
    return static_cast<consensus_state_machine*>(app::State::stuff.sm_.get());
  }

  void handle_result(ptr<_TestSuite::TestSuite::Timer> timer, raft_result& result, ptr<std::exception>& err) {
    if (result.get_result_code() != cmd_result_code::OK) {
      // Something went wrong.
      // This means committing this log failed,
      // but the log itself is still in the log store.
      std::cout << "failed: " << result.get_result_code() << ", "
                << _TestSuite::TestSuite::usToString(timer->getTimeUs()) << std::endl;
      return;
    }
    ptr<buffer> buf = result.get();
    uint64_t ret_value = buf->get_ulong();
    std::cout << on_bright_green << "succeeded" << reset << ", " << _TestSuite::TestSuite::usToString(timer->getTimeUs())
              << ", return value: " << ret_value
              << ", state machine value: " << get_sm()->get_current_value() << std::endl;
  }

  void append_log(op_type OP, std::string& path, std::string& contents) {
    // Serialize and generate Raft log to append.
    ptr<buffer> new_log = consensus_state_machine::enc_log({OP, path, contents});

    // To measure the elapsed time.
    ptr<_TestSuite::TestSuite::Timer> timer = cs_new<_TestSuite::TestSuite::Timer>();

    // Do append.
    ptr<raft_result> ret = app::State::stuff.raft_instance_->append_entries({new_log});

    if (!ret->get_accepted()) {
      // Log append rejected, usually because this node is not a leader.
      std::cout << on_bright_red << "failed" << reset << " to replicate: " << ret->get_result_code() << ", "
                << _TestSuite::TestSuite::usToString(timer->getTimeUs()) << std::endl;
      return;
    }
    // Log append accepted, but that doesn't mean the log is committed.
    // Commit result can be obtained below.

    if (CALL_TYPE == raft_params::blocking) {
      // Blocking mode:
      //   `append_entries` returns after getting a consensus,
      //   so that `ret` already has the result from state machine.
      ptr<std::exception> err(nullptr);
      handle_result(timer, *ret, err);

    } else if (CALL_TYPE == raft_params::async_handler) {
      // Async mode:
      //   `append_entries` returns immediately.
      //   `handle_result` will be invoked asynchronously,
      //   after getting a consensus.
      ret->when_ready(std::bind(
          handle_result,
          timer,
          std::placeholders::_1,
          std::placeholders::_2));

    } else {
      assert(0);
    }
  }

  void init_raft(ptr<state_machine> sm_instance, std::string log_path) {
    using namespace app::consensus;

    // Logger.
    fs::create_directories(fs::absolute(log_path));
    std::string log_file_name = utility::concatenatePath(log_path, std::to_string(app::State::stuff.server_id_) + ".log");
    cout << grey << "Server log file: " << log_file_name << reset << endl;

    ptr<app::consensus::_logger::logger_wrapper> log_wrap = cs_new<app::consensus::_logger::logger_wrapper>(log_file_name, 4);
    app::State::stuff.raft_logger_ = log_wrap;

    // State machine.
    app::State::stuff.smgr_ = cs_new<inmem_state_mgr>(app::State::stuff.server_id_, app::State::stuff.endpoint_);
    // State manager.
    app::State::stuff.sm_ = sm_instance;

    // ASIO options.
    asio_service::options asio_opt;
    asio_opt.thread_pool_size_ = 4;

    // Raft parameters.
    raft_params params;
    // heartbeat: 100 ms, election timeout: 200 - 400 ms.
    params.heart_beat_interval_ = 100;
    params.election_timeout_lower_bound_ = 200;
    params.election_timeout_upper_bound_ = 400;
    // Upto 5 logs will be preserved ahead the last snapshot.
    params.reserved_log_items_ = 5;
    // Snapshot will be created for every 5 log appends.
    params.snapshot_distance_ = 5;
    // Client timeout: 3000 ms.
    params.client_req_timeout_ = 3000;
    // According to this method, `append_log` function
    // should be handled differently.
    params.return_method_ = CALL_TYPE;

    // Initialize Raft server.
    app::State::stuff.raft_instance_ = app::State::stuff.launcher_.init(
        app::State::stuff.sm_,
        app::State::stuff.smgr_,
        app::State::stuff.raft_logger_,
        app::State::stuff.port_,
        asio_opt,
        params);
    if (!app::State::stuff.raft_instance_) {
      std::cerr << "Failed to initialize launcher (see the message in the log file)." << std::endl;
      log_wrap.reset();
      exit(-1);
    }

    // Wait until Raft server is ready (upto 5 seconds).
    const size_t MAX_TRY = 100;
    std::cout << cyan << "⚡ consensus service - init Raft instance " << reset;
    for (size_t ii = 0; ii < MAX_TRY; ++ii) {
      if (app::State::stuff.raft_instance_->is_initialized()) {
        std::cout << " done" << std::endl;
        return;
      }
      std::cout << ".";
      fflush(stdout);
      _TestSuite::TestSuite::sleep_ms(250);
    }
    std::cout << " FAILED" << std::endl;
    log_wrap.reset();
    exit(-1);
  }

  void add_server(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
      std::cout << "too few arguments" << std::endl;
      return;
    }

    int server_id_to_add = atoi(tokens[0].c_str());
    if (!server_id_to_add || server_id_to_add == app::State::stuff.server_id_) {
      std::cout << "wrong server id: " << server_id_to_add << std::endl;
      return;
    }

    std::string endpoint_to_add = tokens[1];
    srv_config srv_conf_to_add(server_id_to_add, endpoint_to_add);
    ptr<raft_result> ret = app::State::stuff.raft_instance_->add_srv(srv_conf_to_add);
    if (!ret->get_accepted()) {
      std::cout << "failed to add server: " << ret->get_result_code() << std::endl;
      return;
    }
    std::cout << "async request is in progress (check with `server_list` function)" << std::endl;
  }

  void print_status() {
    ptr<log_store> ls = app::State::stuff.smgr_->load_log_store();

    std::cout << cyan << "my server id: " << app::State::stuff.server_id_ << std::endl
              << "leader id: " << app::State::stuff.raft_instance_->get_leader() << std::endl
              << "Raft log range: ";
    if (ls->start_index() >= ls->next_slot()) {
      // Start index can be the same as next slot when the log store is empty.
      std::cout << "(empty)" << std::endl;
    } else {
      std::cout << ls->start_index() << " - " << (ls->next_slot() - 1) << std::endl;
    }
    std::cout << "last committed index: " << app::State::stuff.raft_instance_->get_committed_log_idx()
              << std::endl
              << "current term: " << app::State::stuff.raft_instance_->get_term() << std::endl
              << "last snapshot log index: "
              << (app::State::stuff.sm_->last_snapshot()
                      ? app::State::stuff.sm_->last_snapshot()->get_last_log_idx()
                      : 0)
              << std::endl
              << "last snapshot log term: "
              << (app::State::stuff.sm_->last_snapshot()
                      ? app::State::stuff.sm_->last_snapshot()->get_last_log_term()
                      : 0)
              << std::endl
              << on_bright_yellow << "💾 state machine value = " << get_sm()->get_current_value() << reset << std::endl;
  }

  void server_list() {
    std::vector<ptr<srv_config>> configs;
    VariadicTable<int, std::string, std::string> table({"id", "address", "node type"}, 2);

    int leader_id = app::State::stuff.raft_instance_->get_leader();
    app::State::stuff.raft_instance_->get_srv_config_all(configs);

    for (auto& entry : configs) {
      ptr<srv_config>& srv = entry;
      table.addRow(srv->get_id(), srv->get_endpoint(), (srv->get_id() == leader_id) ? "leader ⭐ " : "follower");
    }

    cout << grey << "\n Cluster list:"
         << reset << endl;
    table.print(std::cout);
    cout << endl;
  }

};  // namespace app::consensus

