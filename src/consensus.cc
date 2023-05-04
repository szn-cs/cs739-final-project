#include "common.h"

/*
    ____ ___  _   _ ____  _____ _   _ ____  _   _ ____    ___ _   _ ___ _____ ___    _    _     ___ _____   _  _____ ___ ___  _   _
   / ___/ _ \| \ | / ___|| ____| \ | / ___|| | | / ___|  |_ _| \ | |_ _|_   _|_ _|  / \  | |   |_ _|__  /  / \|_   _|_ _/ _ \| \ | |
  | |  | | | |  \| \___ \|  _| |  \| \___ \| | | \___ \   | ||  \| || |  | |  | |  / _ \ | |    | |  / /  / _ \ | |  | | | | |  \| |
  | |__| |_| | |\  |___) | |___| |\  |___) | |_| |___) |  | || |\  || |  | |  | | / ___ \| |___ | | / /_ / ___ \| |  | | |_| | |\  |
   \____\___/|_| \_|____/|_____|_| \_|____/ \___/|____/  |___|_| \_|___| |_| |___/_/   \_\_____|___/____/_/   \_\_| |___\___/|_| \_|
*/

namespace consensus {
  using namespace nuraft;

  consensus_state_machine* get_sm() {
    return static_cast<consensus_state_machine*>(stuff.sm_.get());
  }

  void handle_result(ptr<TestSuite::Timer> timer, raft_result& result, ptr<std::exception>& err) {
    if (result.get_result_code() != cmd_result_code::OK) {
      // Something went wrong.
      // This means committing this log failed,
      // but the log itself is still in the log store.
      std::cout << "failed: " << result.get_result_code() << ", "
                << TestSuite::Timer::usToString(timer->getTimeUs()) << std::endl;
      return;
    }
    ptr<buffer> buf = result.get();
    uint64_t ret_value = buf->get_ulong();
    std::cout << "succeeded, " << TestSuite::Timer::usToString(timer->getTimeUs())
              << ", return value: " << ret_value
              << ", state machine value: " << get_sm()->get_current_value() << std::endl;
  }

  void append_log(const std::string& cmd, const std::vector<std::string>& tokens) {
    char cmd_char = cmd[0];
    int operand = atoi(tokens[0].substr(1).c_str());
    consensus_state_machine::op_type op = consensus_state_machine::ADD;
    switch (cmd_char) {
      case '+':
        op = consensus_state_machine::ADD;
        break;
      case '-':
        op = consensus_state_machine::SUB;
        break;
      case '*':
        op = consensus_state_machine::MUL;
        break;
      case '/':
        op = consensus_state_machine::DIV;
        if (!operand) {
          std::cout << "cannot divide by zero" << std::endl;
          return;
        }
        break;
      default:
        op = consensus_state_machine::SET;
        break;
    };

    // Serialize and generate Raft log to append.
    ptr<buffer> new_log = consensus_state_machine::enc_log({op, operand});

    // To measure the elapsed time.
    ptr<TestSuite::Timer> timer = cs_new<TestSuite::Timer>();

    // Do append.
    ptr<raft_result> ret = stuff.raft_instance_->append_entries({new_log});

    if (!ret->get_accepted()) {
      // Log append rejected, usually because this node is not a leader.
      std::cout << "failed to replicate: " << ret->get_result_code() << ", "
                << TestSuite::Timer::usToString(timer->getTimeUs()) << std::endl;
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

  void print_status(const std::string& cmd, const std::vector<std::string>& tokens) {
    ptr<log_store> ls = stuff.smgr_->load_log_store();

    std::cout << "my server id: " << stuff.server_id_ << std::endl
              << "leader id: " << stuff.raft_instance_->get_leader() << std::endl
              << "Raft log range: ";
    if (ls->start_index() >= ls->next_slot()) {
      // Start index can be the same as next slot when the log store is empty.
      std::cout << "(empty)" << std::endl;
    } else {
      std::cout << ls->start_index() << " - " << (ls->next_slot() - 1) << std::endl;
    }
    std::cout << "last committed index: " << stuff.raft_instance_->get_committed_log_idx()
              << std::endl
              << "current term: " << stuff.raft_instance_->get_term() << std::endl
              << "last snapshot log index: "
              << (stuff.sm_->last_snapshot()
                      ? stuff.sm_->last_snapshot()->get_last_log_idx()
                      : 0)
              << std::endl
              << "last snapshot log term: "
              << (stuff.sm_->last_snapshot()
                      ? stuff.sm_->last_snapshot()->get_last_log_term()
                      : 0)
              << std::endl
              << "state machine value: " << get_sm()->get_current_value() << std::endl;
  }

  void help(const std::string& cmd, const std::vector<std::string>& tokens) {
    std::cout << "modify value: <+|-|*|/><operand>\n"
              << "    +: add <operand> to state machine's value.\n"
              << "    -: subtract <operand> from state machine's value.\n"
              << "    *: multiple state machine'value by <operand>.\n"
              << "    /: divide state machine's value by <operand>.\n"
              << "    e.g.) +123\n"
              << "\n"
              << "add server: add <server id> <address>:<port>\n"
              << "    e.g.) add 2 127.0.0.1:20000\n"
              << "\n"
              << "get current server status: st (or stat)\n"
              << "\n"
              << "get the list of members: ls (or list)\n"
              << "\n";
  }

  bool do_cmd(const std::vector<std::string>& tokens) {
    if (!tokens.size()) return true;

    const std::string& cmd = tokens[0];

    if (cmd == "q" || cmd == "exit") {
      stuff.launcher_.shutdown(5);
      stuff.reset();
      return false;

    } else if (cmd[0] == '+' || cmd[0] == '-' || cmd[0] == '*' || cmd[0] == '/') {
      // e.g.) +1
      append_log(cmd, tokens);

    } else if (cmd == "add") {
      // e.g.) add 2 localhost:12345
      add_server(cmd, tokens);

    } else if (cmd == "st" || cmd == "stat") {
      print_status(cmd, tokens);

    } else if (cmd == "ls" || cmd == "list") {
      server_list(cmd, tokens);

    } else if (cmd == "h" || cmd == "help") {
      help(cmd, tokens);
    }
    return true;
  }

  void check_additional_flags(int argc, char** argv) {
    for (int ii = 1; ii < argc; ++ii) {
      if (strcmp(argv[ii], "--async-handler") == 0) {
        CALL_TYPE = raft_params::async_handler;
      } else if (strcmp(argv[ii], "--async-snapshot-creation") == 0) {
        ASYNC_SNAPSHOT_CREATION = true;
      }
    }
  }

  void calc_usage(int argc, char** argv) {
    std::stringstream ss;
    ss << "Usage: \n";
    ss << "    " << argv[0] << " <server id> <IP address and port> [<options>]";
    ss << std::endl
       << std::endl;
    ss << "    options:" << std::endl;
    ss << "      --async-handler: use async type handler." << std::endl;
    ss << "      --async-snapshot-creation: create snapshots asynchronously." << std::endl
       << std::endl;

    std::cout << ss.str();
    exit(0);
  }

  void usage(int argc, char** argv) {
    std::stringstream ss;
    ss << "Usage: \n";
    ss << "    " << argv[0] << " <server id> <IP address and port>";
    ss << std::endl;

    std::cout << ss.str();
    exit(0);
  }

  void set_server_info(int argc, char** argv) {
    // Get server ID.
    stuff.server_id_ = atoi(argv[1]);
    if (stuff.server_id_ < 1) {
      std::cerr << "wrong server id (should be >= 1): " << stuff.server_id_
                << std::endl;
      usage(argc, argv);
    }

    // Get server address and port.
    std::string str = argv[2];
    size_t pos = str.rfind(":");
    if (pos == std::string::npos) {
      std::cerr << "wrong endpoint: " << str << std::endl;
      usage(argc, argv);
    }

    stuff.port_ = atoi(str.substr(pos + 1).c_str());
    if (stuff.port_ < 1000) {
      std::cerr << "wrong port (should be >= 1000): " << stuff.port_ << std::endl;
      usage(argc, argv);
    }

    stuff.addr_ = str.substr(0, pos);
    stuff.endpoint_ = stuff.addr_ + ":" + std::to_string(stuff.port_);
  }

  void init_raft(ptr<state_machine> sm_instance) {  // TODO:
    /*
    // Logger.
    std::string log_file_name = "./srv" + std::to_string(stuff.server_id_) + ".log";
    ptr<logger_wrapper> log_wrap = cs_new<logger_wrapper>(log_file_name, 4);
    stuff.raft_logger_ = log_wrap;

    // State machine.
    stuff.smgr_ = cs_new<inmem_state_mgr>(stuff.server_id_, stuff.endpoint_);
    // State manager.
    stuff.sm_ = sm_instance;

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
    stuff.raft_instance_ = stuff.launcher_.init(
        stuff.sm_,
        stuff.smgr_,
        stuff.raft_logger_,
        stuff.port_,
        asio_opt,
        params);
    if (!stuff.raft_instance_) {
      std::cerr << "Failed to initialize launcher (see the message "
                   "in the log file)."
                << std::endl;
      log_wrap.reset();
      exit(-1);
    }

    // Wait until Raft server is ready (upto 5 seconds).
    const size_t MAX_TRY = 20;
    std::cout << "init Raft instance ";
    for (size_t ii = 0; ii < MAX_TRY; ++ii) {
      if (stuff.raft_instance_->is_initialized()) {
        std::cout << " done" << std::endl;
        return;
      }
      std::cout << ".";
      fflush(stdout);
      TestSuite::sleep_ms(250);
    }
    std::cout << " FAILED" << std::endl;
    log_wrap.reset();
    exit(-1);
    */
  }

  void loop() {  // TODO:
    /*
    char cmd[1000];
    std::string prompt = "calc " + std::to_string(stuff.server_id_) + "> ";
    while (true) {
#if defined(__linux__) || defined(__APPLE__)
      std::cout << _CLM_GREEN << prompt << _CLM_END;
#else
      std::cout << prompt;
#endif
      if (!std::cin.getline(cmd, 1000)) {
        break;
      }

      std::vector<std::string> tokens = tokenize(cmd);
      bool cont = do_cmd(tokens);
      if (!cont) break;
    }
    */
  }

  void add_server(const std::string& cmd, const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) {
      std::cout << "too few arguments" << std::endl;
      return;
    }

    int server_id_to_add = atoi(tokens[1].c_str());
    if (!server_id_to_add || server_id_to_add == stuff.server_id_) {
      std::cout << "wrong server id: " << server_id_to_add << std::endl;
      return;
    }

    std::string endpoint_to_add = tokens[2];
    srv_config srv_conf_to_add(server_id_to_add, endpoint_to_add);
    ptr<raft_result> ret = stuff.raft_instance_->add_srv(srv_conf_to_add);
    if (!ret->get_accepted()) {
      std::cout << "failed to add server: " << ret->get_result_code() << std::endl;
      return;
    }
    std::cout << "async request is in progress (check with `list` command)" << std::endl;
  }

  void server_list(const std::string& cmd, const std::vector<std::string>& tokens) {
    std::vector<ptr<srv_config>> configs;
    stuff.raft_instance_->get_srv_config_all(configs);

    int leader_id = stuff.raft_instance_->get_leader();

    for (auto& entry : configs) {
      ptr<srv_config>& srv = entry;
      std::cout << "server id " << srv->get_id() << ": " << srv->get_endpoint();
      if (srv->get_id() == leader_id) {
        std::cout << " (LEADER)";
      }
      std::cout << std::endl;
    }
  }

  std::vector<std::string> tokenize(const char* str, char c = ' ') {
    std::vector<std::string> tokens;
    do {
      const char* begin = str;
      while (*str != c && *str)
        str++;
      if (begin != str) tokens.push_back(std::string(begin, str));
    } while (0 != *str++);

    return tokens;
  }

};  // namespace consensus

/*
   ___ _   _   __  __ _____ __  __  ___  ______   __  _     ___   ____   ____ _____ ___  ____  _____
  |_ _| \ | | |  \/  | ____|  \/  |/ _ \|  _ \ \ / / | |   / _ \ / ___| / ___|_   _/ _ \|  _ \| ____|
   | ||  \| | | |\/| |  _| | |\/| | | | | |_) \ V /  | |  | | | | |  _  \___ \ | || | | | |_) |  _|
   | || |\  | | |  | | |___| |  | | |_| |  _ < | |   | |__| |_| | |_| |  ___) || || |_| |  _ <| |___
  |___|_| \_| |_|  |_|_____|_|  |_|\___/|_| \_\|_|   |_____\___/ \____| |____/ |_| \___/|_| \_\_____|
*/

namespace nuraft {

  inmem_log_store::inmem_log_store()
      : start_idx_(1), raft_server_bwd_pointer_(nullptr), disk_emul_delay(0), disk_emul_thread_(nullptr), disk_emul_thread_stop_signal_(false), disk_emul_last_durable_index_(0) {
    // Dummy entry for index 0.
    ptr<buffer> buf = buffer::alloc(sz_ulong);
    logs_[0] = cs_new<log_entry>(0, buf);
  }

  inmem_log_store::~inmem_log_store() {
    if (disk_emul_thread_) {
      disk_emul_thread_stop_signal_ = true;
      disk_emul_ea_.invoke();
      if (disk_emul_thread_->joinable()) {
        disk_emul_thread_->join();
      }
    }
  }

  ptr<log_entry> inmem_log_store::make_clone(const ptr<log_entry>& entry) {
    // NOTE:
    //   Timestamp is used only when `replicate_log_timestamp_` option is on.
    //   Otherwise, log store does not need to store or load it.
    ptr<log_entry> clone = cs_new<log_entry>(entry->get_term(), buffer::clone(entry->get_buf()), entry->get_val_type(), entry->get_timestamp());
    return clone;
  }

  ulong inmem_log_store::next_slot() const {
    std::lock_guard<std::mutex> l(logs_lock_);
    // Exclude the dummy entry.
    return start_idx_ + logs_.size() - 1;
  }

  ulong inmem_log_store::start_index() const {
    return start_idx_;
  }

  ptr<log_entry> inmem_log_store::last_entry() const {
    ulong next_idx = next_slot();
    std::lock_guard<std::mutex> l(logs_lock_);
    auto entry = logs_.find(next_idx - 1);
    if (entry == logs_.end()) {
      entry = logs_.find(0);
    }

    return make_clone(entry->second);
  }

  ulong inmem_log_store::append(ptr<log_entry>& entry) {
    ptr<log_entry> clone = make_clone(entry);

    std::lock_guard<std::mutex> l(logs_lock_);
    size_t idx = start_idx_ + logs_.size() - 1;
    logs_[idx] = clone;

    if (disk_emul_delay) {
      uint64_t cur_time = timer_helper::get_timeofday_us();
      disk_emul_logs_being_written_[cur_time + disk_emul_delay * 1000] = idx;
      disk_emul_ea_.invoke();
    }

    return idx;
  }

  void inmem_log_store::write_at(ulong index, ptr<log_entry>& entry) {
    ptr<log_entry> clone = make_clone(entry);

    // Discard all logs equal to or greater than `index.
    std::lock_guard<std::mutex> l(logs_lock_);
    auto itr = logs_.lower_bound(index);
    while (itr != logs_.end()) {
      itr = logs_.erase(itr);
    }
    logs_[index] = clone;

    if (disk_emul_delay) {
      uint64_t cur_time = timer_helper::get_timeofday_us();
      disk_emul_logs_being_written_[cur_time + disk_emul_delay * 1000] = index;

      // Remove entries greater than `index`.
      auto entry = disk_emul_logs_being_written_.begin();
      while (entry != disk_emul_logs_being_written_.end()) {
        if (entry->second > index) {
          entry = disk_emul_logs_being_written_.erase(entry);
        } else {
          entry++;
        }
      }
      disk_emul_ea_.invoke();
    }
  }

  ptr<std::vector<ptr<log_entry>>>
  inmem_log_store::log_entries(ulong start, ulong end) {
    ptr<std::vector<ptr<log_entry>>> ret =
        cs_new<std::vector<ptr<log_entry>>>();

    ret->resize(end - start);
    ulong cc = 0;
    for (ulong ii = start; ii < end; ++ii) {
      ptr<log_entry> src = nullptr;
      {
        std::lock_guard<std::mutex> l(logs_lock_);
        auto entry = logs_.find(ii);
        if (entry == logs_.end()) {
          entry = logs_.find(0);
          assert(0);
        }
        src = entry->second;
      }
      (*ret)[cc++] = make_clone(src);
    }
    return ret;
  }

  ptr<std::vector<ptr<log_entry>>>
  inmem_log_store::log_entries_ext(ulong start, ulong end, int64 batch_size_hint_in_bytes) {
    ptr<std::vector<ptr<log_entry>>> ret =
        cs_new<std::vector<ptr<log_entry>>>();

    if (batch_size_hint_in_bytes < 0) {
      return ret;
    }

    size_t accum_size = 0;
    for (ulong ii = start; ii < end; ++ii) {
      ptr<log_entry> src = nullptr;
      {
        std::lock_guard<std::mutex> l(logs_lock_);
        auto entry = logs_.find(ii);
        if (entry == logs_.end()) {
          entry = logs_.find(0);
          assert(0);
        }
        src = entry->second;
      }
      ret->push_back(make_clone(src));
      accum_size += src->get_buf().size();
      if (batch_size_hint_in_bytes &&
          accum_size >= (ulong)batch_size_hint_in_bytes) break;
    }
    return ret;
  }

  ptr<log_entry> inmem_log_store::entry_at(ulong index) {
    ptr<log_entry> src = nullptr;
    {
      std::lock_guard<std::mutex> l(logs_lock_);
      auto entry = logs_.find(index);
      if (entry == logs_.end()) {
        entry = logs_.find(0);
      }
      src = entry->second;
    }
    return make_clone(src);
  }

  ulong inmem_log_store::term_at(ulong index) {
    ulong term = 0;
    {
      std::lock_guard<std::mutex> l(logs_lock_);
      auto entry = logs_.find(index);
      if (entry == logs_.end()) {
        entry = logs_.find(0);
      }
      term = entry->second->get_term();
    }
    return term;
  }

  ptr<buffer> inmem_log_store::pack(ulong index, int32 cnt) {
    std::vector<ptr<buffer>> logs;

    size_t size_total = 0;
    for (ulong ii = index; ii < index + cnt; ++ii) {
      ptr<log_entry> le = nullptr;
      {
        std::lock_guard<std::mutex> l(logs_lock_);
        le = logs_[ii];
      }
      assert(le.get());
      ptr<buffer> buf = le->serialize();
      size_total += buf->size();
      logs.push_back(buf);
    }

    ptr<buffer> buf_out = buffer::alloc(sizeof(int32) + cnt * sizeof(int32) + size_total);
    buf_out->pos(0);
    buf_out->put((int32)cnt);

    for (auto& entry : logs) {
      ptr<buffer>& bb = entry;
      buf_out->put((int32)bb->size());
      buf_out->put(*bb);
    }
    return buf_out;
  }

  void inmem_log_store::apply_pack(ulong index, buffer& pack) {
    pack.pos(0);
    int32 num_logs = pack.get_int();

    for (int32 ii = 0; ii < num_logs; ++ii) {
      ulong cur_idx = index + ii;
      int32 buf_size = pack.get_int();

      ptr<buffer> buf_local = buffer::alloc(buf_size);
      pack.get(buf_local);

      ptr<log_entry> le = log_entry::deserialize(*buf_local);
      {
        std::lock_guard<std::mutex> l(logs_lock_);
        logs_[cur_idx] = le;
      }
    }

    {
      std::lock_guard<std::mutex> l(logs_lock_);
      auto entry = logs_.upper_bound(0);
      if (entry != logs_.end()) {
        start_idx_ = entry->first;
      } else {
        start_idx_ = 1;
      }
    }
  }

  bool inmem_log_store::compact(ulong last_log_index) {
    std::lock_guard<std::mutex> l(logs_lock_);
    for (ulong ii = start_idx_; ii <= last_log_index; ++ii) {
      auto entry = logs_.find(ii);
      if (entry != logs_.end()) {
        logs_.erase(entry);
      }
    }

    // WARNING:
    //   Even though nothing has been erased,
    //   we should set `start_idx_` to new index.
    if (start_idx_ <= last_log_index) {
      start_idx_ = last_log_index + 1;
    }
    return true;
  }

  bool inmem_log_store::flush() {
    disk_emul_last_durable_index_ = next_slot() - 1;
    return true;
  }

  void inmem_log_store::close() {}

  void inmem_log_store::set_disk_delay(raft_server* raft, size_t delay_ms) {
    disk_emul_delay = delay_ms;
    raft_server_bwd_pointer_ = raft;

    if (!disk_emul_thread_) {
      disk_emul_thread_ =
          std::unique_ptr<std::thread>(
              new std::thread(&inmem_log_store::disk_emul_loop, this));
    }
  }

  ulong inmem_log_store::last_durable_index() {
    uint64_t last_log = next_slot() - 1;
    if (!disk_emul_delay) {
      return last_log;
    }

    return disk_emul_last_durable_index_;
  }

  void inmem_log_store::disk_emul_loop() {
    // This thread mimics async disk writes.

    size_t next_sleep_us = 100 * 1000;
    while (!disk_emul_thread_stop_signal_) {
      disk_emul_ea_.wait_us(next_sleep_us);
      disk_emul_ea_.reset();
      if (disk_emul_thread_stop_signal_) break;

      uint64_t cur_time = timer_helper::get_timeofday_us();
      next_sleep_us = 100 * 1000;

      bool call_notification = false;
      {
        std::lock_guard<std::mutex> l(logs_lock_);
        // Remove all timestamps equal to or smaller than `cur_time`,
        // and pick the greatest one among them.
        auto entry = disk_emul_logs_being_written_.begin();
        while (entry != disk_emul_logs_being_written_.end()) {
          if (entry->first <= cur_time) {
            disk_emul_last_durable_index_ = entry->second;
            entry = disk_emul_logs_being_written_.erase(entry);
            call_notification = true;
          } else {
            break;
          }
        }

        entry = disk_emul_logs_being_written_.begin();
        if (entry != disk_emul_logs_being_written_.end()) {
          next_sleep_us = entry->first - cur_time;
        }
      }

      if (call_notification) {
        raft_server_bwd_pointer_->notify_log_append_completion(true);
      }
    }
  }

}  // namespace nuraft
