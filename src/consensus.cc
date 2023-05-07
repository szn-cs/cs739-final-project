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

  void append_log(const std::string& cmd, const std::vector<std::string>& tokens) {
    char cmd_char = cmd[0];
    int operand = atoi(tokens[0].substr(1).c_str());
    consensus_state_machine::op_type op = consensus_state_machine::ADD;

    switch (cmd_char) {
      // TODO: changing values of files
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
      std::cerr << "Failed to initialize launcher (see the message "
                   "in the log file)."
                << std::endl;
      log_wrap.reset();
      exit(-1);
    }

    // Wait until Raft server is ready (upto 5 seconds).
    const size_t MAX_TRY = 100;
    std::cout << "init Raft instance ";
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

/*
   ____  _____ ____  _   _  ____   _     ___   ____  ____ _____ ____
  |  _ \| ____| __ )| | | |/ ___| | |   / _ \ / ___|/ ___| ____|  _ \
  | | | |  _| |  _ \| | | | |  _  | |  | | | | |  _| |  _|  _| | |_) |
  | |_| | |___| |_) | |_| | |_| | | |__| |_| | |_| | |_| | |___|  _ <
  |____/|_____|____/ \___/ \____| |_____\___/ \____|\____|_____|_| \_\
*/

namespace app::consensus::_logger {

  std::atomic<SimpleLoggerMgr*> SimpleLoggerMgr::instance(nullptr);
  std::mutex SimpleLoggerMgr::instanceLock;
  std::mutex SimpleLoggerMgr::displayLock;

  // Number of digits to represent thread IDs (Linux only).
  std::atomic<int> tid_digits(2);

  struct SimpleLoggerMgr::CompElem {
    CompElem(uint64_t num, SimpleLogger* logger)
        : fileNum(num), targetLogger(logger) {}
    uint64_t fileNum;
    SimpleLogger* targetLogger;
  };

  SimpleLoggerMgr::TimeInfo::TimeInfo(std::tm* src)
      : year(src->tm_year + 1900), month(src->tm_mon + 1), day(src->tm_mday), hour(src->tm_hour), min(src->tm_min), sec(src->tm_sec), msec(0), usec(0) {}

  SimpleLoggerMgr::TimeInfo::TimeInfo(std::chrono::system_clock::time_point now) {
    std::time_t raw_time = std::chrono::system_clock::to_time_t(now);
    std::tm new_time;

    std::tm* lt_tm = localtime_r(&raw_time, &new_time);

    year = lt_tm->tm_year + 1900;
    month = lt_tm->tm_mon + 1;
    day = lt_tm->tm_mday;
    hour = lt_tm->tm_hour;
    min = lt_tm->tm_min;
    sec = lt_tm->tm_sec;

    size_t us_epoch =
        std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch())
            .count();
    msec = (us_epoch / 1000) % 1000;
    usec = us_epoch % 1000;
  }

  SimpleLoggerMgr* SimpleLoggerMgr::init() {
    SimpleLoggerMgr* mgr = instance.load(SimpleLogger::MOR);
    if (!mgr) {
      std::lock_guard<std::mutex> l(instanceLock);
      mgr = instance.load(SimpleLogger::MOR);
      if (!mgr) {
        mgr = new SimpleLoggerMgr();
        instance.store(mgr, SimpleLogger::MOR);
      }
    }
    return mgr;
  }

  SimpleLoggerMgr* SimpleLoggerMgr::get() {
    SimpleLoggerMgr* mgr = instance.load(SimpleLogger::MOR);
    if (!mgr) return init();
    return mgr;
  }

  SimpleLoggerMgr* SimpleLoggerMgr::getWithoutInit() {
    SimpleLoggerMgr* mgr = instance.load(SimpleLogger::MOR);
    return mgr;
  }

  void SimpleLoggerMgr::destroy() {
    std::lock_guard<std::mutex> l(instanceLock);
    SimpleLoggerMgr* mgr = instance.load(SimpleLogger::MOR);
    if (mgr) {
      mgr->flushAllLoggers();
      delete mgr;
      instance.store(nullptr, SimpleLogger::MOR);
    }
  }

  int SimpleLoggerMgr::getTzGap() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t raw_time = std::chrono::system_clock::to_time_t(now);
    std::tm new_time;

    std::tm* lt_tm = localtime_r(&raw_time, &new_time);
    std::tm* gmt_tm = std::gmtime(&raw_time);

    TimeInfo lt(lt_tm);
    TimeInfo gmt(gmt_tm);

    return ((lt.day * 60 * 24 + lt.hour * 60 + lt.min) - (gmt.day * 60 * 24 + gmt.hour * 60 + gmt.min));
  }

  // LCOV_EXCL_START

  void SimpleLoggerMgr::flushCriticalInfo() {
    std::string msg = " === Critical info (given by user): ";
    msg += std::to_string(globalCriticalInfo.size()) + " bytes";
    msg += " ===";
    if (!globalCriticalInfo.empty()) {
      msg += "\n" + globalCriticalInfo;
    }
    flushAllLoggers(2, msg);
    if (crashDumpFile.is_open()) {
      crashDumpFile << msg << std::endl
                    << std::endl;
    }
  }

  void SimpleLoggerMgr::_flushStackTraceBuffer(size_t buffer_len, uint32_t tid_hash, uint64_t kernel_tid, bool crash_origin) {
    std::string msg;
    char temp_buf[256];
    snprintf(temp_buf, 256, "\nThread %04x", tid_hash);
    msg += temp_buf;
    if (kernel_tid) {
      msg += " (" + std::to_string(kernel_tid) + ")";
    }
    if (crash_origin) {
      msg += " (crashed here)";
    }
    msg += "\n\n";
    msg += std::string(stackTraceBuffer, buffer_len);

    size_t msg_len = msg.size();
    size_t per_log_size = SimpleLogger::MSG_SIZE - 1024;
    for (size_t ii = 0; ii < msg_len; ii += per_log_size) {
      flushAllLoggers(2, msg.substr(ii, per_log_size));
    }

    if (crashDumpFile.is_open()) {
      crashDumpFile << msg << std::endl;
    }
  }

  void SimpleLoggerMgr::flushStackTraceBuffer(RawStackInfo& stack_info) {
    size_t len = _stack_interpret(&stack_info.stackPtrs[0], stack_info.stackPtrs.size(), stackTraceBuffer, stackTraceBufferSize);
    if (!len) return;

    _flushStackTraceBuffer(
        len,
        stack_info.tidHash,
        stack_info.kernelTid,
        stack_info.crashOrigin);
  }

  void SimpleLoggerMgr::logStackBackTraceOtherThreads() {
    bool got_other_stacks = false;
    if (!crashDumpOriginOnly) {
      // Not support non-Linux platform.
      uint64_t my_tid = pthread_self();
      uint64_t exp = 0;
      if (!crashOriginThread.compare_exchange_strong(exp, my_tid)) {
        // Other thread is already working on it, stop here.
        return;
      }

      std::lock_guard<std::mutex> l(activeThreadsLock);
      std::string msg = "captured ";
      msg += std::to_string(activeThreads.size()) + " active threads";
      flushAllLoggers(2, msg);
      if (crashDumpFile.is_open()) crashDumpFile << msg << "\n\n";

      for (uint64_t _tid : activeThreads) {
        pthread_t tid = (pthread_t)_tid;
        if (_tid == crashOriginThread) continue;

        struct sigaction _action;
        sigfillset(&_action.sa_mask);
        _action.sa_flags = SA_SIGINFO;
        _action.sa_sigaction = SimpleLoggerMgr::handleStackTrace;
        sigaction(SIGUSR2, &_action, NULL);

        pthread_kill(tid, SIGUSR2);

        sigset_t _mask;
        sigfillset(&_mask);
        sigdelset(&_mask, SIGUSR2);
        sigsuspend(&_mask);
      }

      msg = "got all stack traces, now flushing them";
      flushAllLoggers(2, msg);

      got_other_stacks = true;
    }

    if (!got_other_stacks) {
      std::string msg = "will not explore other threads (disabled by user)";
      flushAllLoggers(2, msg);
      if (crashDumpFile.is_open()) {
        crashDumpFile << msg << "\n\n";
      }
    }
  }

  void SimpleLoggerMgr::flushRawStack(RawStackInfo& stack_info) {
    if (!crashDumpFile.is_open()) return;

    crashDumpFile << "Thread " << std::hex << std::setw(4) << std::setfill('0')
                  << stack_info.tidHash << std::dec << " " << stack_info.kernelTid
                  << std::endl;
    if (stack_info.crashOrigin) {
      crashDumpFile << "(crashed here)" << std::endl;
    }
    for (void* stack_ptr : stack_info.stackPtrs) {
      crashDumpFile << std::hex << stack_ptr << std::dec << std::endl;
    }
    crashDumpFile << std::endl;
  }

  void SimpleLoggerMgr::addRawStackInfo(bool crash_origin) {
    void* stack_ptr[256];
    size_t len = _stack_backtrace(stack_ptr, 256);

    crashDumpThreadStacks.push_back(RawStackInfo());
    RawStackInfo& stack_info = *(crashDumpThreadStacks.rbegin());
    std::thread::id tid = std::this_thread::get_id();
    stack_info.tidHash = std::hash<std::thread::id>{}(tid) % 0x10000;
    stack_info.kernelTid = (uint64_t)syscall(SYS_gettid);
    stack_info.crashOrigin = crash_origin;
    for (size_t ii = 0; ii < len; ++ii) {
      stack_info.stackPtrs.push_back(stack_ptr[ii]);
    }
  }

  void SimpleLoggerMgr::logStackBacktrace(size_t timeout_ms) {
    // Set abort timeout: 60 seconds.
    abortTimer = timeout_ms;

    if (!crashDumpPath.empty() && !crashDumpFile.is_open()) {
      // Open crash dump file.
      TimeInfo lt(std::chrono::system_clock::now());
      int tz_gap = getTzGap();
      int tz_gap_abs = (tz_gap < 0) ? (tz_gap * -1) : (tz_gap);

      char filename[128];
      snprintf(filename, 128, "dump_%04d%02d%02d_%02d%02d%02d%c%02d%02d.txt", lt.year, lt.month, lt.day, lt.hour, lt.min, lt.sec, (tz_gap >= 0) ? '+' : '-', (int)(tz_gap_abs / 60), tz_gap_abs % 60);
      std::string path = crashDumpPath + "/" + filename;
      crashDumpFile.open(path);

      char time_fmt[64];
      snprintf(time_fmt, 64, "%04d-%02d-%02dT%02d:%02d:%02d.%03d%03d%c%02d:%02d", lt.year, lt.month, lt.day, lt.hour, lt.min, lt.sec, lt.msec, lt.usec, (tz_gap >= 0) ? '+' : '-', (int)(tz_gap_abs / 60), tz_gap_abs % 60);
      crashDumpFile << "When: " << time_fmt << std::endl
                    << std::endl;
    }

    flushCriticalInfo();
    addRawStackInfo(true);
    // Collect other threads' stack info.
    logStackBackTraceOtherThreads();

    // Now print out.
    // For the case where `addr2line` is hanging, flush raw pointer first.
    for (RawStackInfo& entry : crashDumpThreadStacks) {
      flushRawStack(entry);
    }
    for (RawStackInfo& entry : crashDumpThreadStacks) {
      flushStackTraceBuffer(entry);
    }
  }

  bool SimpleLoggerMgr::chkExitOnCrash() {
    if (exitOnCrash) return true;

    std::string env_segv_str;
    const char* env_segv = std::getenv("SIMPLELOGGER_EXIT_ON_CRASH");
    if (env_segv) env_segv_str = env_segv;

    if (env_segv_str == "ON" || env_segv_str == "on" || env_segv_str == "TRUE" || env_segv_str == "true") {
      // Manually turned off by user, via env var.
      return true;
    }

    return false;
  }

  void SimpleLoggerMgr::handleSegFault(int sig) {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    signal(SIGSEGV, mgr->oldSigSegvHandler);
    mgr->enableOnlyOneDisplayer();
    mgr->flushAllLoggers(1, "Segmentation fault");
    mgr->logStackBacktrace();

    printf("[SEG FAULT] Flushed all logs safely.\n");
    fflush(stdout);

    if (mgr->chkExitOnCrash()) {
      printf("[SEG FAULT] Exit on crash.\n");
      fflush(stdout);
      exit(-1);
    }

    if (mgr->oldSigSegvHandler) {
      mgr->oldSigSegvHandler(sig);
    }
  }

  void SimpleLoggerMgr::handleSegAbort(int sig) {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    signal(SIGABRT, mgr->oldSigAbortHandler);
    mgr->enableOnlyOneDisplayer();
    mgr->flushAllLoggers(1, "Abort");
    mgr->logStackBacktrace();

    printf("[ABORT] Flushed all logs safely.\n");
    fflush(stdout);

    if (mgr->chkExitOnCrash()) {
      printf("[ABORT] Exit on crash.\n");
      fflush(stdout);
      exit(-1);
    }

    abort();
  }

  void SimpleLoggerMgr::handleStackTrace(int sig, siginfo_t* info, void* secret) {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    if (!mgr->crashOriginThread) return;

    pthread_t myself = pthread_self();
    if (mgr->crashOriginThread == myself) return;

    // NOTE:
    //   As getting exact line number is too expensive,
    //   keep stack pointers first and then interpret it.
    mgr->addRawStackInfo();

    // Go back to origin thread.
    pthread_kill(mgr->crashOriginThread, SIGUSR2);
  }

  // LCOV_EXCL_STOP

  void SimpleLoggerMgr::flushWorker() {
    pthread_setname_np(pthread_self(), "sl_flusher");
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    while (!mgr->chkTermination()) {
      // Every 500ms.
      size_t sub_ms = 500;
      mgr->sleepFlusher(sub_ms);
      mgr->flushAllLoggers();
      if (mgr->abortTimer) {
        if (mgr->abortTimer > sub_ms) {
          mgr->abortTimer.fetch_sub(sub_ms);
        } else {
          std::cerr << "STACK DUMP TIMEOUT, FORCE ABORT" << std::endl;
          exit(-1);
        }
      }
    }
  }

  void SimpleLoggerMgr::compressWorker() {
    pthread_setname_np(pthread_self(), "sl_compressor");

    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    bool sleep_next_time = true;
    while (!mgr->chkTermination()) {
      // Every 500ms.
      size_t sub_ms = 500;
      if (sleep_next_time) {
        mgr->sleepCompressor(sub_ms);
      }
      sleep_next_time = true;

      CompElem* elem = nullptr;
      {
        std::lock_guard<std::mutex> l(mgr->pendingCompElemsLock);
        auto entry = mgr->pendingCompElems.begin();
        if (entry != mgr->pendingCompElems.end()) {
          elem = *entry;
          mgr->pendingCompElems.erase(entry);
        }
      }

      if (elem) {
        elem->targetLogger->doCompression(elem->fileNum);
        delete elem;
        // Continuous compression if pending item exists.
        sleep_next_time = false;
      }
    }
  }

  void SimpleLoggerMgr::setCrashDumpPath(const std::string& path, bool origin_only) {
    crashDumpPath = path;
    setStackTraceOriginOnly(origin_only);
  }

  void SimpleLoggerMgr::setStackTraceOriginOnly(bool origin_only) {
    crashDumpOriginOnly = origin_only;
  }

  void SimpleLoggerMgr::setExitOnCrash(bool exit_on_crash) { exitOnCrash = exit_on_crash; }

  SimpleLoggerMgr::SimpleLoggerMgr()
      : termination(false), oldSigSegvHandler(nullptr), oldSigAbortHandler(nullptr), stackTraceBuffer(nullptr), crashOriginThread(0), crashDumpOriginOnly(true), exitOnCrash(false), abortTimer(0) {
    std::string env_segv_str;
    const char* env_segv = std::getenv("SIMPLELOGGER_HANDLE_SEGV");
    if (env_segv) env_segv_str = env_segv;

    if (env_segv_str == "OFF" || env_segv_str == "off" || env_segv_str == "FALSE" || env_segv_str == "false") {
      // Manually turned off by user, via env var.
    } else {
      oldSigSegvHandler = signal(SIGSEGV, SimpleLoggerMgr::handleSegFault);
      oldSigAbortHandler = signal(SIGABRT, SimpleLoggerMgr::handleSegAbort);
    }
    stackTraceBuffer = (char*)malloc(stackTraceBufferSize);

    tFlush = std::thread(SimpleLoggerMgr::flushWorker);
    tCompress = std::thread(SimpleLoggerMgr::compressWorker);
  }

  SimpleLoggerMgr::~SimpleLoggerMgr() {
    termination = true;

    signal(SIGSEGV, oldSigSegvHandler);
    signal(SIGABRT, oldSigAbortHandler);
    {
      std::unique_lock<std::mutex> l(cvFlusherLock);
      cvFlusher.notify_all();
    }
    {
      std::unique_lock<std::mutex> l(cvCompressorLock);
      cvCompressor.notify_all();
    }
    if (tFlush.joinable()) {
      tFlush.join();
    }
    if (tCompress.joinable()) {
      tCompress.join();
    }

    free(stackTraceBuffer);
  }

  // LCOV_EXCL_START
  void SimpleLoggerMgr::enableOnlyOneDisplayer() {
    bool marked = false;
    std::unique_lock<std::mutex> l(loggersLock);
    for (auto& entry : loggers) {
      SimpleLogger* logger = entry;
      if (!logger) continue;
      if (!marked) {
        // The first logger: enable display
        if (logger->getLogLevel() < 4) {
          logger->setLogLevel(4);
        }
        logger->setDispLevel(4);
        marked = true;
      } else {
        // The others: disable display
        logger->setDispLevel(-1);
      }
    }
  }
  // LCOV_EXCL_STOP

  void SimpleLoggerMgr::flushAllLoggers(int level, const std::string& msg) {
    std::unique_lock<std::mutex> l(loggersLock);
    for (auto& entry : loggers) {
      SimpleLogger* logger = entry;
      if (!logger) continue;
      if (!msg.empty()) {
        logger->put(level, __FILE__, __func__, __LINE__, "%s", msg.c_str());
      }
      logger->flushAll();
    }
  }

  void SimpleLoggerMgr::addLogger(SimpleLogger* logger) {
    std::unique_lock<std::mutex> l(loggersLock);
    loggers.insert(logger);
  }

  void SimpleLoggerMgr::removeLogger(SimpleLogger* logger) {
    std::unique_lock<std::mutex> l(loggersLock);
    loggers.erase(logger);
  }

  void SimpleLoggerMgr::addThread(uint64_t tid) {
    std::unique_lock<std::mutex> l(activeThreadsLock);
    activeThreads.insert(tid);
  }

  void SimpleLoggerMgr::removeThread(uint64_t tid) {
    std::unique_lock<std::mutex> l(activeThreadsLock);
    activeThreads.erase(tid);
  }

  void SimpleLoggerMgr::addCompElem(SimpleLoggerMgr::CompElem* elem) {
    {
      std::unique_lock<std::mutex> l(pendingCompElemsLock);
      pendingCompElems.push_back(elem);
    }
    {
      std::unique_lock<std::mutex> l(cvCompressorLock);
      cvCompressor.notify_all();
    }
  }

  void SimpleLoggerMgr::sleepFlusher(size_t ms) {
    std::unique_lock<std::mutex> l(cvFlusherLock);
    cvFlusher.wait_for(l, std::chrono::milliseconds(ms));
  }

  void SimpleLoggerMgr::sleepCompressor(size_t ms) {
    std::unique_lock<std::mutex> l(cvCompressorLock);
    cvCompressor.wait_for(l, std::chrono::milliseconds(ms));
  }

  bool SimpleLoggerMgr::chkTermination() const { return termination; }

  void SimpleLoggerMgr::setCriticalInfo(const std::string& info_str) {
    globalCriticalInfo = info_str;
  }

  const std::string& SimpleLoggerMgr::getCriticalInfo() const { return globalCriticalInfo; }

  // ==========================================

  struct ThreadWrapper {
    ThreadWrapper() {
      mySelf = (uint64_t)pthread_self();
      myTid = (uint32_t)syscall(SYS_gettid);

      // Get the number of digits for alignment.
      int num_digits = 0;
      uint32_t tid = myTid;
      while (tid) {
        num_digits++;
        tid /= 10;
      }
      int exp = tid_digits;
      const size_t MAX_NUM_CMP = 10;
      size_t count = 0;
      while (exp < num_digits && count++ < MAX_NUM_CMP) {
        if (tid_digits.compare_exchange_strong(exp, num_digits)) {
          break;
        }
        exp = tid_digits;
      }

      SimpleLoggerMgr* mgr = SimpleLoggerMgr::getWithoutInit();
      if (mgr) {
        mgr->addThread(mySelf);
      }
    }
    ~ThreadWrapper() {
      SimpleLoggerMgr* mgr = SimpleLoggerMgr::getWithoutInit();
      if (mgr) {
        mgr->removeThread(mySelf);
      }
    }
    uint64_t mySelf;
    uint32_t myTid;
  };

  // ==========================================

  SimpleLogger::LogElem::LogElem()
      : len(0), status(CLEAN) {
    memset(ctx, 0x0, MSG_SIZE);
  }

  // True if dirty.
  bool SimpleLogger::LogElem::needToFlush() { return status.load(MOR) == DIRTY; }

  // True if no other thread is working on it.
  bool SimpleLogger::LogElem::available() {
    Status s = status.load(MOR);
    return s == CLEAN || s == DIRTY;
  }

  int SimpleLogger::LogElem::write(size_t _len, char* msg) {
    Status exp = CLEAN;
    Status val = WRITING;
    if (!status.compare_exchange_strong(exp, val)) return -1;

    len = (_len > MSG_SIZE) ? MSG_SIZE : _len;
    memcpy(ctx, msg, len);

    status.store(LogElem::DIRTY);
    return 0;
  }

  int SimpleLogger::LogElem::flush(std::ofstream& fs) {
    Status exp = DIRTY;
    Status val = FLUSHING;
    if (!status.compare_exchange_strong(exp, val)) return -1;

    fs.write(ctx, len);

    status.store(LogElem::CLEAN);
    return 0;
  }

  // ==========================================

  SimpleLogger::SimpleLogger(const std::string& file_path, size_t max_log_elems, uint64_t log_file_size_limit, uint32_t max_log_files)
      : filePath(replaceString(file_path, "//", "/")), maxLogFiles(max_log_files), maxLogFileSize(log_file_size_limit), numCompJobs(0), curLogLevel(4), curDispLevel(4), tzGap(SimpleLoggerMgr::getTzGap()), cursor(0), logs(max_log_elems) {
    findMinMaxRevNum(minRevnum, curRevnum);
  }

  SimpleLogger::~SimpleLogger() { stop(); }

  void SimpleLogger::setCriticalInfo(const std::string& info_str) {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    if (mgr) {
      mgr->setCriticalInfo(info_str);
    }
  }

  void SimpleLogger::setCrashDumpPath(const std::string& path, bool origin_only) {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    if (mgr) {
      mgr->setCrashDumpPath(path, origin_only);
    }
  }

  void SimpleLogger::setStackTraceOriginOnly(bool origin_only) {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    if (mgr) {
      mgr->setStackTraceOriginOnly(origin_only);
    }
  }

  void SimpleLogger::logStackBacktrace() {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    if (mgr) {
      mgr->enableOnlyOneDisplayer();
      mgr->logStackBacktrace(0);
    }
  }

  void SimpleLogger::shutdown() {
    SimpleLoggerMgr* mgr = SimpleLoggerMgr::getWithoutInit();
    if (mgr) {
      mgr->destroy();
    }
  }

  std::string SimpleLogger::replaceString(const std::string& src_str, const std::string& before, const std::string& after) {
    size_t last = 0;
    size_t pos = src_str.find(before, last);
    std::string ret;
    while (pos != std::string::npos) {
      ret += src_str.substr(last, pos - last);
      ret += after;
      last = pos + before.size();
      pos = src_str.find(before, last);
    }
    if (last < src_str.size()) {
      ret += src_str.substr(last);
    }
    return ret;
  }

  void SimpleLogger::findMinMaxRevNum(size_t& min_revnum_out, size_t& max_revnum_out) {
    std::string dir_path = "./";
    std::string file_name_only = filePath;
    size_t last_pos = filePath.rfind("/");
    if (last_pos != std::string::npos) {
      dir_path = filePath.substr(0, last_pos);
      file_name_only = filePath.substr(last_pos + 1, filePath.size() - last_pos - 1);
    }

    bool min_revnum_initialized = false;
    size_t min_revnum = 0;
    size_t max_revnum = 0;

    DIR* dir_info = opendir(dir_path.c_str());
    struct dirent* dir_entry = nullptr;
    while (dir_info && (dir_entry = readdir(dir_info))) {
      std::string f_name(dir_entry->d_name);
      size_t f_name_pos = f_name.rfind(file_name_only);
      // Irrelavent file: skip.
      if (f_name_pos == std::string::npos) continue;

      findMinMaxRevNumInternal(min_revnum_initialized, min_revnum, max_revnum, f_name);
    }
    if (dir_info) {
      closedir(dir_info);
    }

    min_revnum_out = min_revnum;
    max_revnum_out = max_revnum;
  }

  void SimpleLogger::findMinMaxRevNumInternal(bool& min_revnum_initialized, size_t& min_revnum, size_t& max_revnum, std::string& f_name) {
    size_t last_dot = f_name.rfind(".");
    if (last_dot == std::string::npos) return;

    bool comp_file = false;
    std::string ext = f_name.substr(last_dot + 1, f_name.size() - last_dot - 1);
    if (ext == "gz" && f_name.size() > 7) {
      // Compressed file: asdf.log.123.tar.gz => need to get 123.
      f_name = f_name.substr(0, f_name.size() - 7);
      last_dot = f_name.rfind(".");
      if (last_dot == std::string::npos) return;
      ext = f_name.substr(last_dot + 1, f_name.size() - last_dot - 1);
      comp_file = true;
    }

    size_t revnum = atoi(ext.c_str());
    max_revnum = std::max(max_revnum, ((comp_file) ? (revnum + 1) : (revnum)));
    if (!min_revnum_initialized) {
      min_revnum = revnum;
      min_revnum_initialized = true;
    }
    min_revnum = std::min(min_revnum, revnum);
  }

  std::string SimpleLogger::getLogFilePath(size_t file_num) const {
    if (file_num) {
      return filePath + "." + std::to_string(file_num);
    }
    return filePath;
  }

  int SimpleLogger::start() {
    if (filePath.empty()) return 0;

    // Append at the end.
    fs.open(getLogFilePath(curRevnum), std::ofstream::out | std::ofstream::app);
    if (!fs) return -1;

    SimpleLoggerMgr* mgr = SimpleLoggerMgr::get();
    SimpleLogger* ll = this;
    mgr->addLogger(ll);

    _log_sys(ll, "Start logger: %s (%zu MB per file, up to %zu files)", filePath.c_str(), maxLogFileSize / 1024 / 1024, maxLogFiles.load());

    const std::string& critical_info = mgr->getCriticalInfo();
    if (!critical_info.empty()) {
      _log_info(ll, "%s", critical_info.c_str());
    }

    return 0;
  }

  int SimpleLogger::stop() {
    if (fs.is_open()) {
      SimpleLoggerMgr* mgr = SimpleLoggerMgr::getWithoutInit();
      if (mgr) {
        SimpleLogger* ll = this;
        mgr->removeLogger(ll);

        _log_sys(ll, "Stop logger: %s", filePath.c_str());
        flushAll();
        fs.flush();
        fs.close();

        while (numCompJobs.load() > 0)
          std::this_thread::yield();
      }
    }

    return 0;
  }

  void SimpleLogger::setLogLevel(int level) {
    if (level > 6) return;
    if (!fs) return;

    curLogLevel = level;
  }

  void SimpleLogger::setDispLevel(int level) {
    if (level > 6) return;

    curDispLevel = level;
  }

  void SimpleLogger::setMaxLogFiles(size_t max_log_files) {
    if (max_log_files == 0) return;

    maxLogFiles = max_log_files;
  }

#define _snprintf(msg, avail_len, cur_len, msg_len, ...)         \
  avail_len = (avail_len > cur_len) ? (avail_len - cur_len) : 0; \
  msg_len = snprintf(msg + cur_len, avail_len, __VA_ARGS__);     \
  cur_len += (avail_len > msg_len) ? msg_len : avail_len

#define _vsnprintf(msg, avail_len, cur_len, msg_len, ...)        \
  avail_len = (avail_len > cur_len) ? (avail_len - cur_len) : 0; \
  msg_len = vsnprintf(msg + cur_len, avail_len, __VA_ARGS__);    \
  cur_len += (avail_len > msg_len) ? msg_len : avail_len

  void SimpleLogger::put(int level, const char* source_file, const char* func_name, size_t line_number, const char* format, ...) {
    if (level > curLogLevel.load(MOR)) return;
    if (!fs) return;

    static const char* lv_names[7] = {
        "====",
        "FATL",
        "ERRO",
        "WARN",
        "INFO",
        "DEBG",
        "TRAC"};
    char msg[MSG_SIZE];
    thread_local ThreadWrapper thread_wrapper;
    const int TID_DIGITS = tid_digits;
    thread_local uint32_t tid_hash = thread_wrapper.myTid;

    // Print filename part only (excluding directory path).
    size_t last_slash = 0;
    for (size_t ii = 0; source_file && source_file[ii] != 0; ++ii) {
      if (source_file[ii] == '/' || source_file[ii] == '\\') last_slash = ii;
    }

    SimpleLoggerMgr::TimeInfo lt(std::chrono::system_clock::now());
    int tz_gap_abs = (tzGap < 0) ? (tzGap * -1) : (tzGap);

    // [time] [tid] [log type] [user msg] [stack info]
    // Timestamp: ISO 8601 format.
    size_t cur_len = 0;
    size_t avail_len = MSG_SIZE;
    size_t msg_len = 0;

    _snprintf(msg, avail_len, cur_len, msg_len,
              "%04d-%02d-%02dT%02d:%02d:%02d.%03d_%03d%c%02d:%02d "
              "[%*u] "
              "[%s] ",
              lt.year,
              lt.month,
              lt.day,
              lt.hour,
              lt.min,
              lt.sec,
              lt.msec,
              lt.usec,
              (tzGap >= 0) ? '+' : '-',
              tz_gap_abs / 60,
              tz_gap_abs % 60,
              TID_DIGITS,
              tid_hash,
              lv_names[level]);

    va_list args;
    va_start(args, format);
    _vsnprintf(msg, avail_len, cur_len, msg_len, format, args);
    va_end(args);

    if (source_file && func_name) {
      _snprintf(msg, avail_len, cur_len, msg_len, "\t[%s:%zu, %s()]\n", source_file + ((last_slash) ? (last_slash + 1) : 0), line_number, func_name);
    } else {
      _snprintf(msg, avail_len, cur_len, msg_len, "\n");
    }

    size_t num = logs.size();
    uint64_t cursor_exp = 0, cursor_val = 0;
    LogElem* ll = nullptr;
    do {
      cursor_exp = cursor.load(MOR);
      cursor_val = (cursor_exp + 1) % num;
      ll = &logs[cursor_exp];
    } while (!cursor.compare_exchange_strong(cursor_exp, cursor_val, MOR));
    while (!ll->available())
      std::this_thread::yield();

    if (ll->needToFlush()) {
      // Allow only one thread to flush.
      if (!flush(cursor_exp)) {
        // Other threads: wait.
        while (ll->needToFlush())
          std::this_thread::yield();
      }
    }
    ll->write(cur_len, msg);

    if (level > curDispLevel) return;

    // Console part.
    static const char* colored_lv_names[7] = {_CL_B_BROWN("===="), _CL_WHITE_FG_RED_BG("FATL"), _CL_B_RED("ERRO"), _CL_B_MAGENTA("WARN"), "INFO", _CL_D_GRAY("DEBG"), _CL_D_GRAY("TRAC")};

    cur_len = 0;
    avail_len = MSG_SIZE;
    _snprintf(
        msg,
        avail_len,
        cur_len,
        msg_len,
        " [" _CL_BROWN("%02d") ":" _CL_BROWN("%02d") ":" _CL_BROWN("%02d") "." _CL_BROWN(
            "%03d") " " _CL_BROWN("%03d") "] [tid " _CL_B_BLUE("%*u")
            "] "
            "[%s] ",
        lt.hour,
        lt.min,
        lt.sec,
        lt.msec,
        lt.usec,
        TID_DIGITS,
        tid_hash,
        colored_lv_names[level]);

    if (source_file && func_name) {
      _snprintf(msg, avail_len, cur_len, msg_len, "[" _CL_GREEN("%s") ":" _CL_B_RED("%zu") ", " _CL_CYAN("%s()") "]\n", source_file + ((last_slash) ? (last_slash + 1) : 0), line_number, func_name);
    } else {
      _snprintf(msg, avail_len, cur_len, msg_len, "\n");
    }

    va_start(args, format);

#ifndef LOGGER_NO_COLOR
    if (level == 0) {
      _snprintf(msg, avail_len, cur_len, msg_len, _CLM_B_BROWN);
    } else if (level == 1) {
      _snprintf(msg, avail_len, cur_len, msg_len, _CLM_B_RED);
    }
#endif

    _vsnprintf(msg, avail_len, cur_len, msg_len, format, args);

#ifndef LOGGER_NO_COLOR
    _snprintf(msg, avail_len, cur_len, msg_len, _CLM_END);
#endif

    va_end(args);
    (void)cur_len;

    std::unique_lock<std::mutex> l(SimpleLoggerMgr::displayLock);
    std::cout << msg << std::endl;
    l.unlock();
  }

  void SimpleLogger::execCmd(const std::string& cmd_given) {
    int r = 0;
    std::string cmd = cmd_given;

    cmd += " > /dev/null";
    r = system(cmd.c_str());

    (void)r;
  }

  void SimpleLogger::doCompression(size_t file_num) {
    std::string filename = getLogFilePath(file_num);
    std::string cmd;
    cmd = "tar zcvf " + filename + ".tar.gz " + filename;
    execCmd(cmd);

    cmd = "rm -f " + filename;
    execCmd(cmd);

    size_t max_log_files = maxLogFiles.load();
    // Remove previous log files.
    if (max_log_files && file_num >= max_log_files) {
      for (size_t ii = minRevnum; ii <= file_num - max_log_files; ++ii) {
        filename = getLogFilePath(ii);
        std::string filename_tar = getLogFilePath(ii) + ".tar.gz";
        cmd = "rm -f " + filename + " " + filename_tar;
        execCmd(cmd);
        minRevnum = ii + 1;
      }
    }

    numCompJobs.fetch_sub(1);
  }

  bool SimpleLogger::flush(size_t start_pos) {
    std::unique_lock<std::mutex> ll(flushingLogs, std::try_to_lock);
    if (!ll.owns_lock()) return false;

    size_t num = logs.size();
    // Circular flush into file.
    for (size_t ii = start_pos; ii < num; ++ii) {
      LogElem& ll = logs[ii];
      ll.flush(fs);
    }
    for (size_t ii = 0; ii < start_pos; ++ii) {
      LogElem& ll = logs[ii];
      ll.flush(fs);
    }
    fs.flush();

    if (maxLogFileSize && fs.tellp() > (int64_t)maxLogFileSize) {
      // Exceeded limit, make a new file.
      curRevnum++;
      fs.close();
      fs.open(getLogFilePath(curRevnum), std::ofstream::out | std::ofstream::app);

      // Compress it (tar gz). Register to the global queue.
      SimpleLoggerMgr* mgr = SimpleLoggerMgr::getWithoutInit();
      if (mgr) {
        numCompJobs.fetch_add(1);
        SimpleLoggerMgr::CompElem* elem =
            new SimpleLoggerMgr::CompElem(curRevnum - 1, this);
        mgr->addCompElem(elem);
      }
    }

    return true;
  }

  void SimpleLogger::flushAll() {
    uint64_t start_pos = cursor.load(MOR);
    flush(start_pos);
  }
}  // namespace app::consensus::_logger