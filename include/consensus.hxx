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

// === Timer things ====================================

namespace consensus::TestSuite {
  class Timer {
   public:
    Timer() : duration_ms(0) {
      reset();
    }
    Timer(size_t _duration_ms) : duration_ms(_duration_ms) {
      reset();
    }
    inline bool timeout() { return timeover(); }
    bool timeover() {
      auto cur = std::chrono::system_clock::now();
      std::chrono::duration<double> elapsed = cur - start;
      if (duration_ms < elapsed.count() * 1000) return true;
      return false;
    }
    uint64_t getTimeSec() {
      auto cur = std::chrono::system_clock::now();
      std::chrono::duration<double> elapsed = cur - start;
      return (uint64_t)(elapsed.count());
    }
    uint64_t getTimeMs() {
      auto cur = std::chrono::system_clock::now();
      std::chrono::duration<double> elapsed = cur - start;
      return (uint64_t)(elapsed.count() * 1000);
    }
    uint64_t getTimeUs() {
      auto cur = std::chrono::system_clock::now();
      std::chrono::duration<double> elapsed = cur - start;
      return (uint64_t)(elapsed.count() * 1000000);
    }
    void reset() {
      start = std::chrono::system_clock::now();
    }
    void resetSec(size_t _duration_sec) {
      duration_ms = _duration_sec * 1000;
      reset();
    }
    void resetMs(size_t _duration_ms) {
      duration_ms = _duration_ms;
      reset();
    }

    static std::string usToString(uint64_t us) {
      std::stringstream ss;
      if (us < 1000) {
        // us
        ss << std::fixed << std::setprecision(0) << us << " us";
      } else if (us < 1000000) {
        // ms
        double tmp = static_cast<double>(us / 1000.0);
        ss << std::fixed << std::setprecision(1) << tmp << " ms";
      } else if (us < (uint64_t)600 * 1000000) {
        // second: 1 s -- 600 s (10 mins)
        double tmp = static_cast<double>(us / 1000000.0);
        ss << std::fixed << std::setprecision(1) << tmp << " s";
      } else {
        // minute
        double tmp = static_cast<double>(us / 60.0 / 1000000.0);
        ss << std::fixed << std::setprecision(0) << tmp << " m";
      }
      return ss.str();
    }

   private:
    std::chrono::time_point<std::chrono::system_clock> start;
    size_t duration_ms;
  };  // namespace Timer

}  // namespace consensus::TestSuite

namespace consensus {
  using namespace nuraft;

  static raft_params::return_method_type CALL_TYPE = raft_params::blocking;
  //  = raft_params::async_handler;

  static bool ASYNC_SNAPSHOT_CREATION = false;

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

  void check_additional_flags(int argc, char** argv);
  void calc_usage(int argc, char** argv);  // print usage information
  void usage(int argc, char** argv);
  void set_server_info(int argc, char** argv);
  void init_raft(ptr<state_machine> sm_instance);
  void loop();
  void handle_result(ptr<TestSuite::Timer> timer, raft_result& result, ptr<std::exception>& err);
  void append_log(const std::string& cmd, const std::vector<std::string>& tokens);
  void print_status(const std::string& cmd, const std::vector<std::string>& tokens);
  bool do_cmd(const std::vector<std::string>& tokens);
  void help(const std::string& cmd, const std::vector<std::string>& tokens);
  void add_server(const std::string& cmd, const std::vector<std::string>& tokens);
  void server_list(const std::string& cmd, const std::vector<std::string>& tokens);
  std::vector<std::string> tokenize(const char* str, char c = ' ');

}  // namespace consensus
