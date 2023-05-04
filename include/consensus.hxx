#pragma once

#include "library.h"
#include "rpc.h"
#include "utility.h"

/* 

TODO: calc_server namespace → consensus namespace
#include "logger_wrapper.hxx"
#include "test_common.h"

*/

/*
   ___ _   _   __  __ _____ __  __  ___  ______   __  _     ___   ____   ____ _____ ___  ____  _____
  |_ _| \ | | |  \/  | ____|  \/  |/ _ \|  _ \ \ / / | |   / _ \ / ___| / ___|_   _/ _ \|  _ \| ____|
   | ||  \| | | |\/| |  _| | |\/| | | | | |_) \ V /  | |  | | | | |  _  \___ \ | || | | | |_) |  _|
   | || |\  | | |  | | |___| |  | | |_| |  _ < | |   | |__| |_| | |_| |  ___) || || |_| |  _ <| |___
  |___|_| \_| |_|  |_|_____|_|  |_|\___/|_| \_\|_|   |_____\___/ \____| |____/ |_| \___/|_| \_\_____|
*/

namespace nuraft {
  class raft_server;

  class inmem_log_store : public log_store {
   public:
    inmem_log_store();

    ~inmem_log_store();

    __nocopy__(inmem_log_store);

   public:
    ulong next_slot() const;

    ulong start_index() const;

    ptr<log_entry> last_entry() const;

    ulong append(ptr<log_entry>& entry);

    void write_at(ulong index, ptr<log_entry>& entry);

    ptr<std::vector<ptr<log_entry>>> log_entries(ulong start, ulong end);

    ptr<std::vector<ptr<log_entry>>> log_entries_ext(
        ulong start,
        ulong end,
        int64 batch_size_hint_in_bytes = 0);

    ptr<log_entry> entry_at(ulong index);

    ulong term_at(ulong index);

    ptr<buffer> pack(ulong index, int32 cnt);

    void apply_pack(ulong index, buffer& pack);

    bool compact(ulong last_log_index);

    bool flush();

    void close();

    ulong last_durable_index();

    void set_disk_delay(raft_server* raft, size_t delay_ms);

   private:
    static ptr<log_entry> make_clone(const ptr<log_entry>& entry);

    void disk_emul_loop();

    /**
     * Map of <log index, log data>.
     */
    std::map<ulong, ptr<log_entry>> logs_;

    /**
     * Lock for `logs_`.
     */
    mutable std::mutex logs_lock_;

    /**
     * The index of the first log.
     */
    std::atomic<ulong> start_idx_;

    /**
     * Backward pointer to Raft server.
     */
    raft_server* raft_server_bwd_pointer_;

    // Testing purpose --------------- BEGIN

    /**
     * If non-zero, this log store will emulate the disk write delay.
     */
    std::atomic<size_t> disk_emul_delay;

    /**
     * Map of <timestamp, log index>, emulating logs that is being written to disk.
     * Log index will be regarded as "durable" after the corresponding timestamp.
     */
    std::map<uint64_t, uint64_t> disk_emul_logs_being_written_;

    /**
     * Thread that will update `last_durable_index_` and call
     * `notify_log_append_completion` at proper time.
     */
    std::unique_ptr<std::thread> disk_emul_thread_;

    /**
     * Flag to terminate the thread.
     */
    std::atomic<bool> disk_emul_thread_stop_signal_;

    /**
     * Event awaiter that emulates disk delay.
     */
    EventAwaiter disk_emul_ea_;

    /**
     * Last written log index.
     */
    std::atomic<uint64_t> disk_emul_last_durable_index_;

    // Testing purpose --------------- END
  };

}  // namespace nuraft

/* 
   ____ _____  _  _____ _____   __  __    _    ____ _   _ ___ _   _ _____
  / ___|_   _|/ \|_   _| ____| |  \/  |  / \  / ___| | | |_ _| \ | | ____|
  \___ \ | | / _ \ | | |  _|   | |\/| | / _ \| |   | |_| || ||  \| |  _|
   ___) || |/ ___ \| | | |___  | |  | |/ ___ \ |___|  _  || || |\  | |___
  |____/ |_/_/   \_\_| |_____| |_|  |_/_/   \_\____|_| |_|___|_| \_|_____|
*/

namespace consensus {
  using namespace nuraft;

  class consensus_state_machine : public state_machine {
   public:
    consensus_state_machine(bool async_snapshot = false)
        : cur_value_(0), last_committed_idx_(0), async_snapshot_(async_snapshot) {}

    ~consensus_state_machine() {}

    enum op_type : int {
      ADD = 0x0,
      SUB = 0x1,
      MUL = 0x2,
      DIV = 0x3,
      SET = 0x4
    };

    struct op_payload {
      op_type type_;
      int oprnd_;
    };

    static ptr<buffer> enc_log(const op_payload& payload) {
      // Encode from {operator, operand} to Raft log.
      ptr<buffer> ret = buffer::alloc(sizeof(op_payload));
      buffer_serializer bs(ret);

      // WARNING: We don't consider endian-safety in this example.
      bs.put_raw(&payload, sizeof(op_payload));

      return ret;
    }

    static void dec_log(buffer& log, op_payload& payload_out) {
      // Decode from Raft log to {operator, operand} pair.
      assert(log.size() == sizeof(op_payload));

      buffer_serializer bs(log);
      memcpy(&payload_out, bs.get_raw(log.size()), sizeof(op_payload));
    }

    ptr<buffer> pre_commit(const ulong log_idx, buffer& data) {
      // Nothing to do with pre-commit in this example.
      return nullptr;
    }

    ptr<buffer> commit(const ulong log_idx, buffer& data) {
      op_payload payload;
      dec_log(data, payload);

      int64_t prev_value = cur_value_;
      switch (payload.type_) {
        case ADD: prev_value += payload.oprnd_; break;
        case SUB: prev_value -= payload.oprnd_; break;
        case MUL: prev_value *= payload.oprnd_; break;
        case DIV: prev_value /= payload.oprnd_; break;
        default:
        case SET: prev_value = payload.oprnd_; break;
      }
      cur_value_ = prev_value;

      last_committed_idx_ = log_idx;

      // Return Raft log number as a return result.
      ptr<buffer> ret = buffer::alloc(sizeof(log_idx));
      buffer_serializer bs(ret);
      bs.put_u64(log_idx);
      return ret;
    }

    void commit_config(const ulong log_idx, ptr<cluster_config>& new_conf) {
      // Nothing to do with configuration change. Just update committed index.
      last_committed_idx_ = log_idx;
    }

    void rollback(const ulong log_idx, buffer& data) {
      // Nothing to do with rollback,
      // as this example doesn't do anything on pre-commit.
    }

    int read_logical_snp_obj(snapshot& s, void*& user_snp_ctx, ulong obj_id, ptr<buffer>& data_out, bool& is_last_obj) {
      ptr<snapshot_ctx> ctx = nullptr;
      {
        std::lock_guard<std::mutex> ll(snapshots_lock_);
        auto entry = snapshots_.find(s.get_last_log_idx());
        if (entry == snapshots_.end()) {
          // Snapshot doesn't exist.
          data_out = nullptr;
          is_last_obj = true;
          return 0;
        }
        ctx = entry->second;
      }

      if (obj_id == 0) {
        // Object ID == 0: first object, put dummy data.
        data_out = buffer::alloc(sizeof(int32));
        buffer_serializer bs(data_out);
        bs.put_i32(0);
        is_last_obj = false;

      } else {
        // Object ID > 0: second object, put actual value.
        data_out = buffer::alloc(sizeof(ulong));
        buffer_serializer bs(data_out);
        bs.put_u64(ctx->value_);
        is_last_obj = true;
      }
      return 0;
    }

    void save_logical_snp_obj(snapshot& s, ulong& obj_id, buffer& data, bool is_first_obj, bool is_last_obj) {
      if (obj_id == 0) {
        // Object ID == 0: it contains dummy value, create snapshot context.
        ptr<buffer> snp_buf = s.serialize();
        ptr<snapshot> ss = snapshot::deserialize(*snp_buf);
        create_snapshot_internal(ss);

      } else {
        // Object ID > 0: actual snapshot value.
        buffer_serializer bs(data);
        int64_t local_value = (int64_t)bs.get_u64();

        std::lock_guard<std::mutex> ll(snapshots_lock_);
        auto entry = snapshots_.find(s.get_last_log_idx());
        assert(entry != snapshots_.end());
        entry->second->value_ = local_value;
      }
      // Request next object.
      obj_id++;
    }

    bool apply_snapshot(snapshot& s) {
      std::lock_guard<std::mutex> ll(snapshots_lock_);
      auto entry = snapshots_.find(s.get_last_log_idx());
      if (entry == snapshots_.end()) return false;

      ptr<snapshot_ctx> ctx = entry->second;
      cur_value_ = ctx->value_;
      return true;
    }

    void free_user_snp_ctx(void*& user_snp_ctx) {
      // In this example, `read_logical_snp_obj` doesn't create
      // `user_snp_ctx`. Nothing to do in this function.
    }

    ptr<snapshot> last_snapshot() {
      // Just return the latest snapshot.
      std::lock_guard<std::mutex> ll(snapshots_lock_);
      auto entry = snapshots_.rbegin();
      if (entry == snapshots_.rend()) return nullptr;

      ptr<snapshot_ctx> ctx = entry->second;
      return ctx->snapshot_;
    }

    ulong last_commit_index() {
      return last_committed_idx_;
    }

    void create_snapshot(snapshot& s, async_result<bool>::handler_type& when_done) {
      if (!async_snapshot_) {
        // Create a snapshot in a synchronous way (blocking the thread).
        create_snapshot_sync(s, when_done);
      } else {
        // Create a snapshot in an asynchronous way (in a different thread).
        create_snapshot_async(s, when_done);
      }
    }

    int64_t get_current_value() const { return cur_value_; }

   private:
    struct snapshot_ctx {
      snapshot_ctx(ptr<snapshot>& s, int64_t v)
          : snapshot_(s), value_(v) {}
      ptr<snapshot> snapshot_;
      int64_t value_;
    };

    void create_snapshot_internal(ptr<snapshot> ss) {
      std::lock_guard<std::mutex> ll(snapshots_lock_);

      // Put into snapshot map.
      ptr<snapshot_ctx> ctx = cs_new<snapshot_ctx>(ss, cur_value_);
      snapshots_[ss->get_last_log_idx()] = ctx;

      // Maintain last 3 snapshots only.
      const int MAX_SNAPSHOTS = 3;
      int num = snapshots_.size();
      auto entry = snapshots_.begin();
      for (int ii = 0; ii < num - MAX_SNAPSHOTS; ++ii) {
        if (entry == snapshots_.end()) break;
        entry = snapshots_.erase(entry);
      }
    }

    void create_snapshot_sync(snapshot& s, async_result<bool>::handler_type& when_done) {
      // Clone snapshot from `s`.
      ptr<buffer> snp_buf = s.serialize();
      ptr<snapshot> ss = snapshot::deserialize(*snp_buf);
      create_snapshot_internal(ss);

      ptr<std::exception> except(nullptr);
      bool ret = true;
      when_done(ret, except);

      std::cout << "snapshot (" << ss->get_last_log_term() << ", "
                << ss->get_last_log_idx() << ") has been created synchronously"
                << std::endl;
    }

    void create_snapshot_async(snapshot& s, async_result<bool>::handler_type& when_done) {
      // Clone snapshot from `s`.
      ptr<buffer> snp_buf = s.serialize();
      ptr<snapshot> ss = snapshot::deserialize(*snp_buf);

      // Note that this is a very naive and inefficient example
      // that creates a new thread for each snapshot creation.
      std::thread t_hdl([this, ss, when_done] {
        create_snapshot_internal(ss);

        ptr<std::exception> except(nullptr);
        bool ret = true;
        when_done(ret, except);

        std::cout << "snapshot (" << ss->get_last_log_term() << ", "
                  << ss->get_last_log_idx() << ") has been created asynchronously"
                  << std::endl;
      });
      t_hdl.detach();
    }

    // State machine's current value.
    std::atomic<int64_t> cur_value_;

    // Last committed Raft log number.
    std::atomic<uint64_t> last_committed_idx_;

    // Keeps the last 3 snapshots, by their Raft log numbers.
    std::map<uint64_t, ptr<snapshot_ctx>> snapshots_;

    // Mutex for `snapshots_`.
    std::mutex snapshots_lock_;

    // If `true`, snapshot will be created asynchronously.
    bool async_snapshot_;
  };

};  // namespace consensus

/*
   ___ _   _   __  __ _____ __  __  ___  ______   __  ____ _____  _  _____ _____   __  __    _    _   _    _    ____ _____ ____
  |_ _| \ | | |  \/  | ____|  \/  |/ _ \|  _ \ \ / / / ___|_   _|/ \|_   _| ____| |  \/  |  / \  | \ | |  / \  / ___| ____|  _ \
   | ||  \| | | |\/| |  _| | |\/| | | | | |_) \ V /  \___ \ | | / _ \ | | |  _|   | |\/| | / _ \ |  \| | / _ \| |  _|  _| | |_) |
   | || |\  | | |  | | |___| |  | | |_| |  _ < | |    ___) || |/ ___ \| | | |___  | |  | |/ ___ \| |\  |/ ___ \ |_| | |___|  _ <
  |___|_| \_| |_|  |_|_____|_|  |_|\___/|_| \_\|_|   |____/ |_/_/   \_\_| |_____| |_|  |_/_/   \_\_| \_/_/   \_\____|_____|_| \_\
*/

namespace nuraft {

  class inmem_state_mgr : public state_mgr {
   public:
    inmem_state_mgr(int srv_id, const std::string& endpoint)
        : my_id_(srv_id), my_endpoint_(endpoint), cur_log_store_(cs_new<inmem_log_store>()) {
      my_srv_config_ = cs_new<srv_config>(srv_id, endpoint);

      // Initial cluster config: contains only one server (myself).
      saved_config_ = cs_new<cluster_config>();
      saved_config_->get_servers().push_back(my_srv_config_);
    }

    ~inmem_state_mgr() {}

    ptr<cluster_config> load_config() {
      // Just return in-memory data in this example.
      // May require reading from disk here, if it has been written to disk.
      return saved_config_;
    }

    void save_config(const cluster_config& config) {
      // Just keep in memory in this example.
      // Need to write to disk here, if want to make it durable.
      ptr<buffer> buf = config.serialize();
      saved_config_ = cluster_config::deserialize(*buf);
    }

    void save_state(const srv_state& state) {
      // Just keep in memory in this example.
      // Need to write to disk here, if want to make it durable.
      ptr<buffer> buf = state.serialize();
      saved_state_ = srv_state::deserialize(*buf);
    }

    ptr<srv_state> read_state() {
      // Just return in-memory data in this example.
      // May require reading from disk here, if it has been written to disk.
      return saved_state_;
    }

    ptr<log_store> load_log_store() {
      return cur_log_store_;
    }

    int32 server_id() {
      return my_id_;
    }

    void system_exit(const int exit_code) {
    }

    ptr<srv_config> get_srv_config() const { return my_srv_config_; }

   private:
    int my_id_;
    std::string my_endpoint_;
    ptr<inmem_log_store> cur_log_store_;
    ptr<srv_config> my_srv_config_;
    ptr<cluster_config> saved_config_;
    ptr<srv_state> saved_state_;
  };

};  // namespace nuraft

/*
    ____ ___  _   _ ____  _____ _   _ ____  _   _ ____    ___ _   _ ___ _____ ___    _    _     ___ _____   _  _____ ___ ___  _   _
   / ___/ _ \| \ | / ___|| ____| \ | / ___|| | | / ___|  |_ _| \ | |_ _|_   _|_ _|  / \  | |   |_ _|__  /  / \|_   _|_ _/ _ \| \ | |
  | |  | | | |  \| \___ \|  _| |  \| \___ \| | | \___ \   | ||  \| || |  | |  | |  / _ \ | |    | |  / /  / _ \ | |  | | | | |  \| |
  | |__| |_| | |\  |___) | |___| |\  |___) | |_| |___) |  | || |\  || |  | |  | | / ___ \| |___ | | / /_ / ___ \| |  | | |_| | |\  |
   \____\___/|_| \_|____/|_____|_| \_|____/ \___/|____/  |___|_| \_|___| |_| |___/_/   \_\_____|___/____/_/   \_\_| |___\___/|_| \_|
*/

/*
using namespace nuraft;

using raft_result = cmd_result<ptr<buffer>>;

struct server_stuff {
  server_stuff()
      : server_id_(1), addr_("localhost"), port_(25000), raft_logger_(nullptr), sm_(nullptr), smgr_(nullptr), raft_instance_(nullptr) {}

  void reset() {
    raft_logger_.reset();
    sm_.reset();
    smgr_.reset();
    raft_instance_.reset();
  }

  // Server ID.
  int server_id_;

  // Server address.
  std::string addr_;

  // Server port.
  int port_;

  // Endpoint: `<addr>:<port>`.
  std::string endpoint_;

  // Logger.
  ptr<logger> raft_logger_;

  // State machine.
  ptr<state_machine> sm_;

  // State manager.
  ptr<state_mgr> smgr_;

  // Raft launcher.
  raft_launcher launcher_;

  // Raft server instance.
  ptr<raft_server> raft_instance_;
};
static server_stuff stuff;

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

bool do_cmd(const std::vector<std::string>& tokens);

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

void loop() {
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
}

void init_raft(ptr<state_machine> sm_instance) {
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

*/
