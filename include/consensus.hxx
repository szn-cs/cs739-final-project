#pragma once

#include "library.h"
#include "rpc.h"
#include "struct.h"
#include "utility.h"

/*
  _     ___   ____   ____ _____ ___  ____  _____
 | |   / _ \ / ___| / ___|_   _/ _ \|  _ \| ____|
 | |  | | | | |  _  \___ \ | || | | | |_) |  _|
 | |__| |_| | |_| |  ___) || || |_| |  _ <| |___
 |_____\___/ \____| |____/ |_| \___/|_| \_\_____|
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

namespace app::consensus {
  using namespace nuraft;

  typedef std::shared_ptr<std::string> path_t;  // TODO: enforce MAX_PATH_SIZE

  class consensus_state_machine : public state_machine {
   public:
    consensus_state_machine(bool async_snapshot = false)
        : cur_value_(nullptr), last_committed_idx_(0), async_snapshot_(async_snapshot) {}

    ~consensus_state_machine() {}

    struct op_payload {
      app::consensus::op_type type_;
      std::string path_;     // file path
      std::string content_;  // file content
    };

    static ptr<buffer> enc_log(const op_payload& payload) {
      // Encode from {type, path, content} to Raft log.
      ptr<buffer> ret = buffer::alloc(sizeof(op_payload));
      buffer_serializer bs(ret);

      bs.put_i32((int)payload.type_);
      bs.put_str(payload.path_);
      bs.put_str(payload.content_);

      return ret;
    }

    static void dec_log(buffer& data, op_payload& payload_out) {
      buffer_serializer bs(data);
      payload_out.type_ = (app::consensus::op_type)bs.get_i32();
      payload_out.path_ = bs.get_str();
      payload_out.content_ = bs.get_str();
    }

    ptr<buffer> pre_commit(const ulong log_idx, buffer& data) {
      // Nothing to do with pre-commit in this example.
      return nullptr;
    }

    ptr<buffer> commit(const ulong log_idx, buffer& data) {
      op_payload payload;
      dec_log(data, payload);

      string mapped_path = utility::concatenatePath(fs::absolute(app::State::config->directory), payload.path_);
      string directory = std::filesystem::path(mapped_path).parent_path().string();

      path_t prev_value = cur_value_;  // prev_value should be cleaned up automatically by shared_ptr

      switch (payload.type_) {
        case app::consensus::op_type::CREATE: {
          // create file
        } break;
        case app::consensus::op_type::WRITE: {
          // std::cout << static_cast<std::underlying_type<app::consensus::op_type>::type>(payload.type_) << std::endl;
          srand((unsigned)time(NULL));
          // Get a random number
          int random = rand();

          fs::create_directories(directory);

          string temp = mapped_path + "_" + std::to_string(random) + ".tmp";
          ofstream o;  //Write to a temporary file
          o.open(temp);
          o << payload.content_;
          o.close();

          //Perform an atomic move operation... needed so readers can't open a partially written file
          if (std::rename(temp.c_str(), mapped_path.c_str()) != 0) {
            perror("Error renaming file");
            cout << red << mapped_path << reset << endl;
            cout << red << temp << reset << endl;
          } else if (app::State::config->flag.debug) {
            cout << cyan << "📄 writing file: " << mapped_path << reset << endl;
          }

        } break;
        case app::consensus::op_type::DELETE: {
          // remove file
        } break;
        default:
          cout << red << "consensus_state_machine: Unknown operation type !" << reset << endl;
      }

      cur_value_ = std::make_shared<std::string>(mapped_path);

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
        bs.put_str(ctx->value_);
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
        std::string local_value = (std::string)bs.get_str();

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
      cur_value_ = std::make_shared<std::string>(ctx->value_);
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

    std::string get_current_value() const {
      if (cur_value_ != nullptr)
        return *cur_value_;
      else
        return "/";
    }

   private:
    struct snapshot_ctx {
      snapshot_ctx(ptr<snapshot>& s, std::string v)
          : snapshot_(s), value_(v) {}
      ptr<snapshot> snapshot_;
      std::string value_;
    };

    void create_snapshot_internal(ptr<snapshot> ss) {
      std::lock_guard<std::mutex> ll(snapshots_lock_);

      // Put into snapshot map.
      ptr<snapshot_ctx> ctx;
      if (cur_value_ != nullptr)
        ctx = cs_new<snapshot_ctx>(ss, *cur_value_);
      else
        ctx = cs_new<snapshot_ctx>(ss, "/");

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
    path_t cur_value_;  // represents the path // used to be std::atomic<shared_ptr> but C++20 not yet implemented completely

    // Last committed Raft log number.
    std::atomic<uint64_t> last_committed_idx_;

    // Keeps the last 3 snapshots, by their Raft log numbers.
    std::map<uint64_t, ptr<snapshot_ctx>> snapshots_;

    // Mutex for `snapshots_`.
    std::mutex snapshots_lock_;

    // If `true`, snapshot will be created asynchronously.
    bool async_snapshot_;
  };

};  // namespace app::consensus

/*
  ____ _____  _  _____ _____   __  __    _    _   _    _    ____ _____ ____
 / ___|_   _|/ \|_   _| ____| |  \/  |  / \  | \ | |  / \  / ___| ____|  _ \
 \___ \ | | / _ \ | | |  _|   | |\/| | / _ \ |  \| | / _ \| |  _|  _| | |_) |
  ___) || |/ ___ \| | | |___  | |  | |/ ___ \| |\  |/ ___ \ |_| | |___|  _ <
 |____/ |_/_/   \_\_| |_____| |_|  |_/_/   \_\_| \_/_/   \_\____|_____|_| \_\
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
      ptr<srv_config> peer_srv_config_;

      // cluster servers during initialization (preventing choosing leaders before setting quorum sized)
      for (const auto& [endpoint, Node_ptr] : *(app::State::memberList)) {
        utility::parse::Address a = utility::parse::make_address(Node_ptr->endpoint.address);
        int id = a.port;
        if (id == app::State::stuff.server_id_)
          continue;  //skip self node
        std::cout << " load_config server id " << id << "    endpoint:    " << Node_ptr->endpoint.address << std::endl;
        peer_srv_config_ = cs_new<srv_config>(id, Node_ptr->endpoint.address);
        saved_config_->get_servers().push_back(peer_srv_config_);
      }

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

}  // namespace nuraft

/*
   ____  _____ ____  _   _  ____   _     ___   ____  ____ _____ ____
  |  _ \| ____| __ )| | | |/ ___| | |   / _ \ / ___|/ ___| ____|  _ \
  | | | |  _| |  _ \| | | | |  _  | |  | | | | |  _| |  _|  _| | |_) |
  | |_| | |___| |_) | |_| | |_| | | |__| |_| | |_| | |_| | |___|  _ <
  |____/|_____|____/ \___/ \____| |_____\___/ \____|\____|_____|_| \_\
*/

namespace app::consensus::_logger::_backtrace {
  // LCOV_EXCL_START

#define SIZE_T_UNUSED size_t __attribute__((unused))
#define VOID_UNUSED void __attribute__((unused))
#define UINT64_T_UNUSED uint64_t __attribute__((unused))
#define STR_UNUSED std::string __attribute__((unused))
#define INTPTR_UNUSED intptr_t __attribute__((unused))

#define _snprintf(msg, avail_len, cur_len, msg_len, ...)         \
  avail_len = (avail_len > cur_len) ? (avail_len - cur_len) : 0; \
  msg_len = snprintf(msg + cur_len, avail_len, __VA_ARGS__);     \
  cur_len += (avail_len > msg_len) ? msg_len : avail_len

  static SIZE_T_UNUSED _stack_backtrace(void** stack_ptr, size_t stack_ptr_capacity) {
    return backtrace(stack_ptr, stack_ptr_capacity);
  }

  static SIZE_T_UNUSED _stack_interpret_linux(void** stack_ptr, char** stack_msg, int stack_size, char* output_buf, size_t output_buflen);

  static SIZE_T_UNUSED _stack_interpret_apple(void** stack_ptr, char** stack_msg, int stack_size, char* output_buf, size_t output_buflen);

  static SIZE_T_UNUSED _stack_interpret_other(void** stack_ptr, char** stack_msg, int stack_size, char* output_buf, size_t output_buflen);

  static SIZE_T_UNUSED _stack_interpret(void** stack_ptr, int stack_size, char* output_buf, size_t output_buflen) {
    char** stack_msg = nullptr;
    stack_msg = backtrace_symbols(stack_ptr, stack_size);

    size_t len = 0;

    len = _stack_interpret_linux(
        stack_ptr,
        stack_msg,
        stack_size,
        output_buf,
        output_buflen);

    free(stack_msg);

    return len;
  }

  static SIZE_T_UNUSED _stack_interpret_linux(void** stack_ptr, char** stack_msg, int stack_size, char* output_buf, size_t output_buflen) {
    size_t cur_len = 0;
    size_t frame_num = 0;

    // NOTE: starting from 1, skipping this frame.
    for (int i = 1; i < stack_size; ++i) {
      // `stack_msg[x]` format:
      //   /foo/bar/executable() [0xabcdef]
      //   /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xf0) [0x123456]

      // NOTE: with ASLR
      //   /foo/bar/executable(+0x5996) [0x555555559996]

      int fname_len = 0;
      while (stack_msg[i][fname_len] != '(' && stack_msg[i][fname_len] != ' ' && stack_msg[i][fname_len] != 0x0) {
        ++fname_len;
      }

      char addr_str[256];
      uintptr_t actual_addr = 0x0;
      if (stack_msg[i][fname_len] == '(' && stack_msg[i][fname_len + 1] == '+') {
        // ASLR is enabled, get the offset from here.
        int upto = fname_len + 2;
        while (stack_msg[i][upto] != ')' && stack_msg[i][upto] != 0x0) {
          upto++;
        }
        snprintf(addr_str, 256, "%.*s", upto - fname_len - 2, &stack_msg[i][fname_len + 2]);

        // Convert hex string -> integer address.
        std::stringstream ss;
        ss << std::hex << addr_str;
        ss >> actual_addr;

      } else {
        actual_addr = (uintptr_t)stack_ptr[i];
        snprintf(addr_str, 256, "%" PRIxPTR, actual_addr);
      }

      char cmd[1024];
      snprintf(cmd, 1024, "addr2line -f -e %.*s %s", fname_len, stack_msg[i], addr_str);
      FILE* fp = popen(cmd, "r");
      if (!fp) continue;

      char mangled_name[1024];
      char file_line[1024];
      int ret = fscanf(fp, "%1023s %1023s", mangled_name, file_line);
      (void)ret;
      pclose(fp);

      size_t msg_len = 0;
      size_t avail_len = output_buflen;
      _snprintf(output_buf, avail_len, cur_len, msg_len, "#%-2zu 0x%016" PRIxPTR " in ", frame_num++, actual_addr);

      int status;
      char* cc = abi::__cxa_demangle(mangled_name, 0, 0, &status);
      if (cc) {
        _snprintf(output_buf, avail_len, cur_len, msg_len, "%s at ", cc);
      } else {
        std::string msg_str = stack_msg[i];
        std::string _func_name = msg_str;
        size_t s_pos = msg_str.find("(");
        size_t e_pos = msg_str.rfind("+");
        if (e_pos == std::string::npos) e_pos = msg_str.rfind(")");
        if (s_pos != std::string::npos && e_pos != std::string::npos) {
          _func_name = msg_str.substr(s_pos + 1, e_pos - s_pos - 1);
        }
        _snprintf(output_buf, avail_len, cur_len, msg_len, "%s() at ", (_func_name.empty() ? mangled_name : _func_name.c_str()));
      }

      _snprintf(output_buf, avail_len, cur_len, msg_len, "%s\n", file_line);
    }

    return cur_len;
  }

  static VOID_UNUSED skip_whitespace(const std::string base_str, size_t& cursor) {
    while (base_str[cursor] == ' ')
      cursor++;
  }

  static VOID_UNUSED skip_glyph(const std::string base_str, size_t& cursor) {
    while (base_str[cursor] != ' ')
      cursor++;
  }

  static SIZE_T_UNUSED _stack_interpret_apple(void** stack_ptr, char** stack_msg, int stack_size, char* output_buf, size_t output_buflen) {
    size_t cur_len = 0;
    return cur_len;
  }

  static SIZE_T_UNUSED _stack_interpret_other(void** stack_ptr, char** stack_msg, int stack_size, char* output_buf, size_t output_buflen) {
    size_t cur_len = 0;
    size_t frame_num = 0;
    (void)frame_num;

    // NOTE: starting from 1, skipping this frame.
    for (int i = 1; i < stack_size; ++i) {
      // On non-Linux platform, just use the raw symbols.
      size_t msg_len = 0;
      size_t avail_len = output_buflen;
      _snprintf(output_buf, avail_len, cur_len, msg_len, "%s\n", stack_msg[i]);
    }
    return cur_len;
  }

  static SIZE_T_UNUSED stack_backtrace(char* output_buf, size_t output_buflen) {
    void* stack_ptr[256];
    int stack_size = _stack_backtrace(stack_ptr, 256);
    return _stack_interpret(stack_ptr, stack_size, output_buf, output_buflen);
  }

  // LCOV_EXCL_STOP
}  // namespace app::consensus::_logger::_backtrace

#ifndef _CLM_DEFINED
#define _CLM_DEFINED (1)

#ifdef LOGGER_NO_COLOR
#define _CLM_D_GRAY ""
#define _CLM_GREEN ""
#define _CLM_B_GREEN ""
#define _CLM_RED ""
#define _CLM_B_RED ""
#define _CLM_BROWN ""
#define _CLM_B_BROWN ""
#define _CLM_BLUE ""
#define _CLM_B_BLUE ""
#define _CLM_MAGENTA ""
#define _CLM_B_MAGENTA ""
#define _CLM_CYAN ""
#define _CLM_END ""

#define _CLM_WHITE_FG_RED_BG ""
#else
#define _CLM_D_GRAY "\033[1;30m"
#define _CLM_GREEN "\033[32m"
#define _CLM_B_GREEN "\033[1;32m"
#define _CLM_RED "\033[31m"
#define _CLM_B_RED "\033[1;31m"
#define _CLM_BROWN "\033[33m"
#define _CLM_B_BROWN "\033[1;33m"
#define _CLM_BLUE "\033[34m"
#define _CLM_B_BLUE "\033[1;34m"
#define _CLM_MAGENTA "\033[35m"
#define _CLM_B_MAGENTA "\033[1;35m"
#define _CLM_CYAN "\033[36m"
#define _CLM_B_GREY "\033[1;37m"
#define _CLM_END "\033[0m"

#define _CLM_WHITE_FG_RED_BG "\033[37;41m"
#endif

#define _CL_D_GRAY(str) _CLM_D_GRAY str _CLM_END
#define _CL_GREEN(str) _CLM_GREEN str _CLM_END
#define _CL_RED(str) _CLM_RED str _CLM_END
#define _CL_B_RED(str) _CLM_B_RED str _CLM_END
#define _CL_MAGENTA(str) _CLM_MAGENTA str _CLM_END
#define _CL_BROWN(str) _CLM_BROWN str _CLM_END
#define _CL_B_BROWN(str) _CLM_B_BROWN str _CLM_END
#define _CL_B_BLUE(str) _CLM_B_BLUE str _CLM_END
#define _CL_B_MAGENTA(str) _CLM_B_MAGENTA str _CLM_END
#define _CL_CYAN(str) _CLM_CYAN str _CLM_END
#define _CL_B_GRAY(str) _CLM_B_GREY str _CLM_END

#define _CL_WHITE_FG_RED_BG(str) _CLM_WHITE_FG_RED_BG str _CLM_END

#endif

namespace app::consensus::_logger {
  using namespace app::consensus::_logger::_backtrace;

// To suppress false alarms by thread sanitizer,
// add -DSUPPRESS_TSAN_FALSE_ALARMS=1 flag to CXXFLAGS.
// #define SUPPRESS_TSAN_FALSE_ALARMS (1)

// 0: System  [====]
// 1: Fatal   [FATL]
// 2: Error   [ERRO]
// 3: Warning [WARN]
// 4: Info    [INFO]
// 5: Debug   [DEBG]
// 6: Trace   [TRAC]

// printf style log macro
#define _log_(level, l, ...)          \
  if (l && l->getLogLevel() >= level) \
  (l)->put(level, __FILE__, __func__, __LINE__, __VA_ARGS__)

#define _log_sys(l, ...) _log_(SimpleLogger::SYS, l, __VA_ARGS__)
#define _log_fatal(l, ...) _log_(SimpleLogger::FATAL, l, __VA_ARGS__)
#define _log_err(l, ...) _log_(SimpleLogger::ERROR, l, __VA_ARGS__)
#define _log_warn(l, ...) _log_(SimpleLogger::WARNING, l, __VA_ARGS__)
#define _log_info(l, ...) _log_(SimpleLogger::INFO, l, __VA_ARGS__)
#define _log_debug(l, ...) _log_(SimpleLogger::DEBUG, l, __VA_ARGS__)
#define _log_trace(l, ...) _log_(SimpleLogger::TRACE, l, __VA_ARGS__)

// stream log macro
#define _stream_(level, l)            \
  if (l && l->getLogLevel() >= level) \
  l->eos() = l->stream(level, l, __FILE__, __func__, __LINE__)

#define _s_sys(l) _stream_(SimpleLogger::SYS, l)
#define _s_fatal(l) _stream_(SimpleLogger::FATAL, l)
#define _s_err(l) _stream_(SimpleLogger::ERROR, l)
#define _s_warn(l) _stream_(SimpleLogger::WARNING, l)
#define _s_info(l) _stream_(SimpleLogger::INFO, l)
#define _s_debug(l) _stream_(SimpleLogger::DEBUG, l)
#define _s_trace(l) _stream_(SimpleLogger::TRACE, l)

// Do printf style log, but print logs in `lv1` level during normal time,
// once in given `interval_ms` interval, print a log in `lv2` level.
// The very first log will be printed in `lv2` level.
//
// This function is global throughout the process, so that
// multiple threads will share the interval.
#define _timed_log_g(l, interval_ms, lv1, lv2, ...)         \
  {                                                         \
    _timed_log_definition(static);                          \
    _timed_log_body(l, interval_ms, lv1, lv2, __VA_ARGS__); \
  }

// Same as `_timed_log_g` but per-thread level.
#define _timed_log_t(l, interval_ms, lv1, lv2, ...)         \
  {                                                         \
    _timed_log_definition(thread_local);                    \
    _timed_log_body(l, interval_ms, lv1, lv2, __VA_ARGS__); \
  }

#define _timed_log_definition(prefix)                         \
  prefix std::mutex timer_lock;                               \
  prefix bool first_event_fired = false;                      \
  prefix std::chrono::system_clock::time_point last_timeout = \
      std::chrono::system_clock::now();

#define _timed_log_body(l, interval_ms, lv1, lv2, ...)          \
  std::chrono::system_clock::time_point cur =                   \
      std::chrono::system_clock::now();                         \
  bool timeout = false;                                         \
  {                                                             \
    std::lock_guard<std::mutex> l(timer_lock);                  \
    std::chrono::duration<double> elapsed = cur - last_timeout; \
    if (elapsed.count() * 1000 > interval_ms ||                 \
        !first_event_fired) {                                   \
      cur = std::chrono::system_clock::now();                   \
      elapsed = cur - last_timeout;                             \
      if (elapsed.count() * 1000 > interval_ms ||               \
          !first_event_fired) {                                 \
        timeout = first_event_fired = true;                     \
        last_timeout = cur;                                     \
      }                                                         \
    }                                                           \
  }                                                             \
  if (timeout) {                                                \
    _log_(lv2, l, __VA_ARGS__);                                 \
  } else {                                                      \
    _log_(lv1, l, __VA_ARGS__);                                 \
  }

  class SimpleLoggerMgr;
  class SimpleLogger {
    friend class SimpleLoggerMgr;

   public:
    static const int MSG_SIZE = 4096;
    static const std::memory_order MOR = std::memory_order_relaxed;

    enum Levels {
      SYS = 0,
      FATAL = 1,
      ERROR = 2,
      WARNING = 3,
      INFO = 4,
      DEBUG = 5,
      TRACE = 6,
      UNKNOWN = 99,
    };

    class LoggerStream : public std::ostream {
     public:
      LoggerStream() : std::ostream(&buf), level(0), logger(nullptr), file(nullptr), func(nullptr), line(0) {}

      template <typename T>
      inline LoggerStream& operator<<(const T& data) {
        sStream << data;
        return *this;
      }

      using MyCout = std::basic_ostream<char, std::char_traits<char>>;
      typedef MyCout& (*EndlFunc)(MyCout&);
      inline LoggerStream& operator<<(EndlFunc func) {
        func(sStream);
        return *this;
      }

      inline void put() {
        if (logger) {
          logger->put(level, file, func, line, "%s", sStream.str().c_str());
        }
      }

      inline void setLogInfo(int _level, SimpleLogger* _logger, const char* _file, const char* _func, size_t _line) {
        sStream.str(std::string());
        level = _level;
        logger = _logger;
        file = _file;
        func = _func;
        line = _line;
      }

     private:
      std::stringbuf buf;
      std::stringstream sStream;
      int level;
      SimpleLogger* logger;
      const char* file;
      const char* func;
      size_t line;
    };

    class EndOfStmt {
     public:
      EndOfStmt() {}
      EndOfStmt(LoggerStream& src) { src.put(); }
      EndOfStmt& operator=(LoggerStream& src) {
        src.put();
        return *this;
      }
    };

    LoggerStream& stream(int level, SimpleLogger* logger, const char* file, const char* func, size_t line) {
      thread_local LoggerStream msg;
      msg.setLogInfo(level, logger, file, func, line);
      return msg;
    }

    EndOfStmt& eos() {
      thread_local EndOfStmt _eos;
      return _eos;
    }

   private:
    struct LogElem {
      enum Status {
        CLEAN = 0,
        WRITING = 1,
        DIRTY = 2,
        FLUSHING = 3,
      };

      LogElem();

      // True if dirty.
      bool needToFlush();

      // True if no other thread is working on it.
      bool available();

      int write(size_t _len, char* msg);
      int flush(std::ofstream& fs);

      size_t len;
      char ctx[MSG_SIZE];
      std::atomic<Status> status;
    };

   public:
    SimpleLogger(const std::string& file_path, size_t max_log_elems = 4096, uint64_t log_file_size_limit = 32 * 1024 * 1024, uint32_t max_log_files = 16);
    ~SimpleLogger();

    static void setCriticalInfo(const std::string& info_str);
    static void setCrashDumpPath(const std::string& path, bool origin_only = true);
    static void setStackTraceOriginOnly(bool origin_only);
    static void logStackBacktrace();

    static void shutdown();
    static std::string replaceString(const std::string& src_str, const std::string& before, const std::string& after);

    int start();
    int stop();

    inline bool traceAllowed() const { return (curLogLevel.load(MOR) >= 6); }
    inline bool debugAllowed() const { return (curLogLevel.load(MOR) >= 5); }

    void setLogLevel(int level);
    void setDispLevel(int level);
    void setMaxLogFiles(size_t max_log_files);

    inline int getLogLevel() const { return curLogLevel.load(MOR); }
    inline int getDispLevel() const { return curDispLevel.load(MOR); }

    void put(int level, const char* source_file, const char* func_name, size_t line_number, const char* format, ...);
    void flushAll();

   private:
    void calcTzGap();
    void findMinMaxRevNum(size_t& min_revnum_out, size_t& max_revnum_out);
    void findMinMaxRevNumInternal(bool& min_revnum_initialized, size_t& min_revnum, size_t& max_revnum, std::string& f_name);
    std::string getLogFilePath(size_t file_num) const;
    void execCmd(const std::string& cmd);
    void doCompression(size_t file_num);
    bool flush(size_t start_pos);

    std::string filePath;
    size_t minRevnum;
    size_t curRevnum;
    std::atomic<size_t> maxLogFiles;
    std::ofstream fs;

    uint64_t maxLogFileSize;
    std::atomic<uint32_t> numCompJobs;

    // Log up to `curLogLevel`, default: 6.
    // Disable: -1.
    std::atomic<int> curLogLevel;

    // Display (print out on terminal) up to `curDispLevel`,
    // default: 4 (do not print debug and trace).
    // Disable: -1.
    std::atomic<int> curDispLevel;

    std::mutex displayLock;

    int tzGap;
    std::atomic<uint64_t> cursor;
    std::vector<LogElem> logs;
    std::mutex flushingLogs;
  };

  // Singleton class
  class SimpleLoggerMgr {
   public:
    struct CompElem;

    struct TimeInfo {
      TimeInfo(std::tm* src);
      TimeInfo(std::chrono::system_clock::time_point now);
      int year;
      int month;
      int day;
      int hour;
      int min;
      int sec;
      int msec;
      int usec;
    };

    struct RawStackInfo {
      RawStackInfo() : tidHash(0), kernelTid(0), crashOrigin(false) {}
      uint32_t tidHash;
      uint64_t kernelTid;
      std::vector<void*> stackPtrs;
      bool crashOrigin;
    };

    static SimpleLoggerMgr* init();
    static SimpleLoggerMgr* get();
    static SimpleLoggerMgr* getWithoutInit();
    static void destroy();
    static int getTzGap();
    static void handleSegFault(int sig);
    static void handleSegAbort(int sig);
    static void handleStackTrace(int sig, siginfo_t* info, void* secret);
    static void flushWorker();
    static void compressWorker();

    void logStackBacktrace(size_t timeout_ms = 60 * 1000);
    void flushCriticalInfo();
    void enableOnlyOneDisplayer();
    void flushAllLoggers() { flushAllLoggers(0, std::string()); }
    void flushAllLoggers(int level, const std::string& msg);
    void addLogger(SimpleLogger* logger);
    void removeLogger(SimpleLogger* logger);
    void addThread(uint64_t tid);
    void removeThread(uint64_t tid);
    void addCompElem(SimpleLoggerMgr::CompElem* elem);
    void sleepFlusher(size_t ms);
    void sleepCompressor(size_t ms);
    bool chkTermination() const;
    void setCriticalInfo(const std::string& info_str);
    void setCrashDumpPath(const std::string& path, bool origin_only);
    void setStackTraceOriginOnly(bool origin_only);

    /**
     * Set the flag regarding exiting on crash.
     * If flag is `true`, custom segfault handler will not invoke
     * original handler so that process will terminate without
     * generating core dump.
     * The flag is `false` by default.
     *
     * @param exit_on_crash New flag value.
     * @return void.
     */
    void setExitOnCrash(bool exit_on_crash);

    const std::string& getCriticalInfo() const;

    static std::mutex displayLock;

   private:
    // Copy is not allowed.
    SimpleLoggerMgr(const SimpleLoggerMgr&) = delete;
    SimpleLoggerMgr& operator=(const SimpleLoggerMgr&) = delete;

    static const size_t stackTraceBufferSize = 65536;

    // Singleton instance and lock.
    static std::atomic<SimpleLoggerMgr*> instance;
    static std::mutex instanceLock;

    SimpleLoggerMgr();
    ~SimpleLoggerMgr();

    void _flushStackTraceBuffer(size_t buffer_len, uint32_t tid_hash, uint64_t kernel_tid, bool crash_origin);
    void flushStackTraceBuffer(RawStackInfo& stack_info);
    void flushRawStack(RawStackInfo& stack_info);
    void addRawStackInfo(bool crash_origin = false);
    void logStackBackTraceOtherThreads();

    bool chkExitOnCrash();

    std::mutex loggersLock;
    std::unordered_set<SimpleLogger*> loggers;

    std::mutex activeThreadsLock;
    std::unordered_set<uint64_t> activeThreads;

    // Periodic log flushing thread.
    std::thread tFlush;

    // Old log file compression thread.
    std::thread tCompress;

    // List of files to be compressed.
    std::list<CompElem*> pendingCompElems;

    // Lock for `pendingCompFiles`.
    std::mutex pendingCompElemsLock;

    // Condition variable for BG flusher.
    std::condition_variable cvFlusher;
    std::mutex cvFlusherLock;

    // Condition variable for BG compressor.
    std::condition_variable cvCompressor;
    std::mutex cvCompressorLock;

    // Termination signal.
    std::atomic<bool> termination;

    // Original segfault handler.
    void (*oldSigSegvHandler)(int);

    // Original abort handler.
    void (*oldSigAbortHandler)(int);

    // Critical info that will be displayed on crash.
    std::string globalCriticalInfo;

    // Reserve some buffer for stack trace.
    char* stackTraceBuffer;

    // TID of thread where crash happens.
    std::atomic<uint64_t> crashOriginThread;

    std::string crashDumpPath;
    std::ofstream crashDumpFile;

    // If `true`, generate stack trace only for the origin thread.
    // Default: `true`.
    bool crashDumpOriginOnly;

    // If `true`, do not invoke original segfault handler
    // so that process just terminates.
    // Default: `false`.
    bool exitOnCrash;

    std::atomic<uint64_t> abortTimer;

    // Assume that only one thread is updating this.
    std::vector<RawStackInfo> crashDumpThreadStacks;
  };
}  // namespace app::consensus::_logger

namespace app::consensus::_logger {
  using namespace nuraft;

  /**
   * Example implementation of Raft logger, on top of SimpleLogger.
   */
  class logger_wrapper : public logger {
   public:
    logger_wrapper(const std::string& log_file, int log_level = 6) {
      my_log_ = new SimpleLogger(log_file, 1024, 32 * 1024 * 1024, 10);
      my_log_->setLogLevel(log_level);
      my_log_->setDispLevel(-1);
      my_log_->setCrashDumpPath("./", true);
      my_log_->start();
    }

    ~logger_wrapper() {
      destroy();
    }

    void destroy() {
      if (my_log_) {
        my_log_->flushAll();
        my_log_->stop();
        delete my_log_;
        my_log_ = nullptr;
      }
    }

    void put_details(int level, const char* source_file, const char* func_name, size_t line_number, const std::string& msg) {
      if (my_log_) {
        my_log_->put(level, source_file, func_name, line_number, "%s", msg.c_str());
      }
    }

    void set_level(int l) {
      if (!my_log_) return;

      if (l < 0) l = 1;
      if (l > 6) l = 6;
      my_log_->setLogLevel(l);
    }

    int get_level() {
      if (!my_log_) return 0;
      return my_log_->getLogLevel();
    }

    SimpleLogger* getLogger() const { return my_log_; }

   private:
    SimpleLogger* my_log_;
  };
}  // namespace app::consensus::_logger

/* TODO: MOVE TO utility.cc
   _   _ _____ ___ _     ___ _______   __
  | | | |_   _|_ _| |   |_ _|_   _\ \ / /
  | | | | | |  | || |    | |  | |  \ V /
  | |_| | | |  | || |___ | |  | |   | |
   \___/  |_| |___|_____|___| |_|   |_|
*/

namespace app::consensus::_TestSuite {

#define _CLM_DEFINED (1)

#define _CLM_D_GRAY "\033[1;30m"
#define _CLM_GREEN "\033[32m"
#define _CLM_B_GREEN "\033[1;32m"
#define _CLM_RED "\033[31m"
#define _CLM_B_RED "\033[1;31m"
#define _CLM_BROWN "\033[33m"
#define _CLM_B_BROWN "\033[1;33m"
#define _CLM_BLUE "\033[34m"
#define _CLM_B_BLUE "\033[1;34m"
#define _CLM_MAGENTA "\033[35m"
#define _CLM_B_MAGENTA "\033[1;35m"
#define _CLM_CYAN "\033[36m"
#define _CLM_B_GREY "\033[1;37m"
#define _CLM_END "\033[0m"

#define _CLM_WHITE_FG_RED_BG "\033[37;41m"

#define _CL_D_GRAY(str) _CLM_D_GRAY str _CLM_END
#define _CL_GREEN(str) _CLM_GREEN str _CLM_END
#define _CL_RED(str) _CLM_RED str _CLM_END
#define _CL_B_RED(str) _CLM_B_RED str _CLM_END
#define _CL_MAGENTA(str) _CLM_MAGENTA str _CLM_END
#define _CL_BROWN(str) _CLM_BROWN str _CLM_END
#define _CL_B_BROWN(str) _CLM_B_BROWN str _CLM_END
#define _CL_B_BLUE(str) _CLM_B_BLUE str _CLM_END
#define _CL_B_MAGENTA(str) _CLM_B_MAGENTA str _CLM_END
#define _CL_CYAN(str) _CLM_CYAN str _CLM_END
#define _CL_B_GRAY(str) _CLM_B_GREY str _CLM_END

#define _CL_WHITE_FG_RED_BG(str) _CLM_WHITE_FG_RED_BG str _CLM_END

#define __COUT_STACK_INFO__                                                           \
  std::endl                                                                           \
      << "        time: " << _CLM_D_GRAY << TestSuite::getTimeString() << _CLM_END    \
      << "\n"                                                                         \
      << "      thread: " << _CLM_BROWN << std::hex << std::setw(4)                   \
      << std::setfill('0')                                                            \
      << (std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xffff)          \
      << std::dec << _CLM_END << "\n"                                                 \
      << "          in: " << _CLM_CYAN << __func__ << "()" _CLM_END << "\n"           \
      << "          at: " << _CLM_GREEN << __FILE__ << _CLM_END ":" << _CLM_B_MAGENTA \
      << __LINE__ << _CLM_END << "\n"

// exp_value == value
#define CHK_EQ(exp_value, value)                                         \
  {                                                                      \
    auto _ev = (exp_value);                                              \
    decltype(_ev) _v = (decltype(_ev))(value);                           \
    if (_ev != _v) {                                                     \
      std::cout << __COUT_STACK_INFO__                                   \
                << "    value of: " _CLM_B_BLUE #value _CLM_END "\n"     \
                << "    expected: " _CLM_B_GREEN << _ev << _CLM_END "\n" \
                << "      actual: " _CLM_B_RED << _v << _CLM_END "\n";   \
      TestSuite::failHandler();                                          \
      return -1;                                                         \
    }                                                                    \
  }

// exp_value != value
#define CHK_NEQ(exp_value, value)                                            \
  {                                                                          \
    auto _ev = (exp_value);                                                  \
    decltype(_ev) _v = (decltype(_ev))(value);                               \
    if (_ev == _v) {                                                         \
      std::cout << __COUT_STACK_INFO__                                       \
                << "    value of: " _CLM_B_BLUE #value _CLM_END "\n"         \
                << "    expected: not " _CLM_B_GREEN << _ev << _CLM_END "\n" \
                << "      actual: " _CLM_B_RED << _v << _CLM_END "\n";       \
      TestSuite::failHandler();                                              \
      return -1;                                                             \
    }                                                                        \
  }

// value == true
#define CHK_OK(value)                                                     \
  if (!(value)) {                                                         \
    std::cout << __COUT_STACK_INFO__                                      \
              << "    value of: " _CLM_B_BLUE #value _CLM_END "\n"        \
              << "    expected: " _CLM_B_GREEN << "true" << _CLM_END "\n" \
              << "      actual: " _CLM_B_RED << "false" << _CLM_END "\n"; \
    TestSuite::failHandler();                                             \
    return -1;                                                            \
  }

#define CHK_TRUE(value) CHK_OK(value)

// value == false
#define CHK_NOT(value)                                                     \
  if (value) {                                                             \
    std::cout << __COUT_STACK_INFO__                                       \
              << "    value of: " _CLM_B_BLUE #value _CLM_END "\n"         \
              << "    expected: " _CLM_B_GREEN << "false" << _CLM_END "\n" \
              << "      actual: " _CLM_B_RED << "true" << _CLM_END "\n";   \
    TestSuite::failHandler();                                              \
    return -1;                                                             \
  }

#define CHK_FALSE(value) CHK_NOT(value)

// value == NULL
#define CHK_NULL(value)                                                      \
  {                                                                          \
    auto _v = (value);                                                       \
    if (_v) {                                                                \
      std::cout << __COUT_STACK_INFO__                                       \
                << "    value of: " _CLM_B_BLUE #value _CLM_END "\n"         \
                << "    expected: " _CLM_B_GREEN << "NULL" << _CLM_END "\n"; \
      printf("      actual: " _CLM_B_RED "%p" _CLM_END "\n", _v);            \
      TestSuite::failHandler();                                              \
      return -1;                                                             \
    }                                                                        \
  }

// value != NULL
#define CHK_NONNULL(value)                                                    \
  if (!(value)) {                                                             \
    std::cout << __COUT_STACK_INFO__                                          \
              << "    value of: " _CLM_B_BLUE #value _CLM_END "\n"            \
              << "    expected: " _CLM_B_GREEN << "non-NULL" << _CLM_END "\n" \
              << "      actual: " _CLM_B_RED << "NULL" << _CLM_END "\n";      \
    TestSuite::failHandler();                                                 \
    return -1;                                                                \
  }

// value == 0
#define CHK_Z(value)                                                     \
  {                                                                      \
    auto _v = (value);                                                   \
    if ((0) != _v) {                                                     \
      std::cout << __COUT_STACK_INFO__                                   \
                << "    value of: " _CLM_B_BLUE #value _CLM_END "\n"     \
                << "    expected: " _CLM_B_GREEN << "0" << _CLM_END "\n" \
                << "      actual: " _CLM_B_RED << _v << _CLM_END "\n";   \
      TestSuite::failHandler();                                          \
      return -1;                                                         \
    }                                                                    \
  }

// smaller < greater
#define CHK_SM(smaller, greater)                                          \
  {                                                                       \
    auto _sm = (smaller);                                                 \
    decltype(_sm) _gt = (decltype(_sm))(greater);                         \
    if (!(_sm < _gt)) {                                                   \
      std::cout << __COUT_STACK_INFO__ << "    expected: "                \
                << _CLM_B_BLUE #smaller " < " #greater _CLM_END "\n"      \
                << "    value of " << _CLM_B_GREEN #smaller _CLM_END ": " \
                << _CLM_B_RED << _sm << _CLM_END "\n"                     \
                << "    value of " << _CLM_B_GREEN #greater _CLM_END ": " \
                << _CLM_B_RED << _gt << _CLM_END "\n";                    \
      TestSuite::failHandler();                                           \
      return -1;                                                          \
    }                                                                     \
  }

// smaller <= greater
#define CHK_SMEQ(smaller, greater)                                        \
  {                                                                       \
    auto _sm = (smaller);                                                 \
    decltype(_sm) _gt = (decltype(_sm))(greater);                         \
    if (!(_sm <= _gt)) {                                                  \
      std::cout << __COUT_STACK_INFO__ << "    expected: "                \
                << _CLM_B_BLUE #smaller " <= " #greater _CLM_END "\n"     \
                << "    value of " << _CLM_B_GREEN #smaller _CLM_END ": " \
                << _CLM_B_RED << _sm << _CLM_END "\n"                     \
                << "    value of " << _CLM_B_GREEN #greater _CLM_END ": " \
                << _CLM_B_RED << _gt << _CLM_END "\n";                    \
      TestSuite::failHandler();                                           \
      return -1;                                                          \
    }                                                                     \
  }

// greater > smaller
#define CHK_GT(greater, smaller)                                          \
  {                                                                       \
    auto _sm = (smaller);                                                 \
    decltype(_sm) _gt = (decltype(_sm))(greater);                         \
    if (!(_gt > _sm)) {                                                   \
      std::cout << __COUT_STACK_INFO__ << "    expected: "                \
                << _CLM_B_BLUE #greater " > " #smaller _CLM_END "\n"      \
                << "    value of " << _CLM_B_GREEN #greater _CLM_END ": " \
                << _CLM_B_RED << _gt << _CLM_END "\n"                     \
                << "    value of " << _CLM_B_GREEN #smaller _CLM_END ": " \
                << _CLM_B_RED << _sm << _CLM_END "\n";                    \
      TestSuite::failHandler();                                           \
      return -1;                                                          \
    }                                                                     \
  }

// greater >= smaller
#define CHK_GTEQ(greater, smaller)                                        \
  {                                                                       \
    auto _sm = (smaller);                                                 \
    decltype(_sm) _gt = (decltype(_sm))(greater);                         \
    if (!(_gt >= _sm)) {                                                  \
      std::cout << __COUT_STACK_INFO__ << "    expected: "                \
                << _CLM_B_BLUE #greater " >= " #smaller _CLM_END "\n"     \
                << "    value of " << _CLM_B_GREEN #greater _CLM_END ": " \
                << _CLM_B_RED << _gt << _CLM_END "\n"                     \
                << "    value of " << _CLM_B_GREEN #smaller _CLM_END ": " \
                << _CLM_B_RED << _sm << _CLM_END "\n";                    \
      TestSuite::failHandler();                                           \
      return -1;                                                          \
    }                                                                     \
  }

  using test_func = std::function<int()>;

  class TestArgsBase;
  using test_func_args = std::function<int(TestArgsBase*)>;

  class TestSuite;
  class TestArgsBase {
   public:
    virtual ~TestArgsBase() {}
    void
    setCallback(std::string test_name, test_func_args func, TestSuite* test_instance) {
      testName = test_name;
      testFunction = func;
      testInstance = test_instance;
    }
    void testAll() { testAllInternal(0); }
    virtual void setParam(size_t param_no, size_t param_idx) = 0;
    virtual size_t getNumSteps(size_t param_no) = 0;
    virtual size_t getNumParams() = 0;
    virtual std::string toString() = 0;

   private:
    inline void testAllInternal(size_t depth);
    std::string testName;
    test_func_args testFunction;
    TestSuite* testInstance;
  };

  class TestArgsWrapper {
   public:
    TestArgsWrapper(TestArgsBase* _test_args)
        : test_args(_test_args) {}
    ~TestArgsWrapper() { delete test_args; }
    TestArgsBase* getArgs() const { return test_args; }
    operator TestArgsBase*() const { return getArgs(); }

   private:
    TestArgsBase* test_args;
  };

  enum class StepType { LINEAR,
                        EXPONENTIAL };

  template <typename T>
  class TestRange {
   public:
    TestRange()
        : type(RangeType::NONE), begin(), end(), step() {}

    // Constructor for given values
    TestRange(const std::vector<T>& _array)
        : type(RangeType::ARRAY), array(_array), begin(), end(), step() {}

    // Constructor for regular steps
    TestRange(T _begin, T _end, T _step, StepType _type)
        : begin(_begin), end(_end), step(_step) {
      if (_type == StepType::LINEAR) {
        type = RangeType::LINEAR;
      } else {
        type = RangeType::EXPONENTIAL;
      }
    }

    T getEntry(size_t idx) {
      if (type == RangeType::ARRAY) {
        return (T)(array[idx]);
      } else if (type == RangeType::LINEAR) {
        return (T)(begin + step * idx);
      } else if (type == RangeType::EXPONENTIAL) {
        ssize_t _begin = begin;
        ssize_t _step = step;
        ssize_t _ret = (ssize_t)(_begin * std::pow(_step, idx));
        return (T)(_ret);
      }

      return begin;
    }

    size_t getSteps() {
      if (type == RangeType::ARRAY) {
        return array.size();
      } else if (type == RangeType::LINEAR) {
        return ((end - begin) / step) + 1;
      } else if (type == RangeType::EXPONENTIAL) {
        ssize_t coe = ((ssize_t)end) / ((ssize_t)begin);
        double steps_double = (double)std::log(coe) / std::log(step);
        return (size_t)(steps_double + 1);
      }

      return 0;
    }

   private:
    enum class RangeType { NONE,
                           ARRAY,
                           LINEAR,
                           EXPONENTIAL };

    RangeType type;
    std::vector<T> array;
    T begin;
    T end;
    T step;
  };

  struct TestOptions {
    TestOptions()
        : printTestMessage(false), abortOnFailure(false), preserveTestFiles(false) {}
    bool printTestMessage;
    bool abortOnFailure;
    bool preserveTestFiles;
  };

  class TestSuite {
    friend TestArgsBase;

   private:
    static std::mutex& getResMsgLock() {
      static std::mutex res_msg_lock;
      return res_msg_lock;
    }
    static std::string& getResMsg() {
      static std::string res_msg;
      return res_msg;
    }
    static std::string& getInfoMsg() {
      thread_local std::string info_msg;
      return info_msg;
    }
    static std::string& getTestName() {
      static std::string test_name;
      return test_name;
    }
    static TestSuite*& getCurTest() {
      static TestSuite* cur_test;
      return cur_test;
    }

   public:
    static bool& globalMsgFlag() {
      static bool global_msg_flag = false;
      return global_msg_flag;
    }
    static std::string getCurrentTestName() { return getTestName(); }
    static bool isMsgAllowed() {
      TestSuite* cur_test = TestSuite::getCurTest();
      if (cur_test && (cur_test->options.printTestMessage || cur_test->displayMsg) && !cur_test->suppressMsg) {
        return true;
      }
      if (globalMsgFlag()) return true;
      return false;
    }

    static void setInfo(const char* format, ...) {
      thread_local char info_buf[4096];
      size_t len = 0;
      va_list args;
      va_start(args, format);
      len += vsnprintf(info_buf + len, 4096 - len, format, args);
      va_end(args);
      getInfoMsg() = info_buf;
    }
    static void clearInfo() { getInfoMsg().clear(); }

    static void failHandler() {
      if (!getInfoMsg().empty()) {
        std::cout << "        info: " << getInfoMsg() << std::endl;
      }
    }

    static void usage(int argc, char** argv) {
      printf("\n");
      printf("Usage: %s [-f <keyword>] [-r <parameter>] [-p]\n", argv[0]);
      printf("\n");
      printf("    -f, --filter\n");
      printf("        Run specific tests matching the given keyword.\n");
      printf("    -r, --range\n");
      printf("        Run TestRange-based tests using given parameter value.\n");
      printf("    -p, --preserve\n");
      printf("        Do not clean up test files.\n");
      printf("    --abort-on-failure\n");
      printf("        Immediately abort the test if failure happens.\n");
      printf("    --suppress-msg\n");
      printf("        Suppress test messages.\n");
      printf("    --display-msg\n");
      printf("        Display test messages.\n");
      printf("\n");
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

    static std::string countToString(uint64_t count) {
      std::stringstream ss;
      if (count < 1000) {
        ss << count;
      } else if (count < 1000000) {
        // K
        double tmp = static_cast<double>(count / 1000.0);
        ss << std::fixed << std::setprecision(1) << tmp << "K";
      } else if (count < (uint64_t)1000000000) {
        // M
        double tmp = static_cast<double>(count / 1000000.0);
        ss << std::fixed << std::setprecision(1) << tmp << "M";
      } else {
        // B
        double tmp = static_cast<double>(count / 1000000000.0);
        ss << std::fixed << std::setprecision(1) << tmp << "B";
      }
      return ss.str();
    }

    static std::string sizeToString(uint64_t size) {
      std::stringstream ss;
      if (size < 1024) {
        ss << size << " B";
      } else if (size < 1024 * 1024) {
        // K
        double tmp = static_cast<double>(size / 1024.0);
        ss << std::fixed << std::setprecision(1) << tmp << " KiB";
      } else if (size < (uint64_t)1024 * 1024 * 1024) {
        // M
        double tmp = static_cast<double>(size / 1024.0 / 1024.0);
        ss << std::fixed << std::setprecision(1) << tmp << " MiB";
      } else {
        // B
        double tmp = static_cast<double>(size / 1024.0 / 1024.0 / 1024.0);
        ss << std::fixed << std::setprecision(1) << tmp << " GiB";
      }
      return ss.str();
    }

   private:
    struct TimeInfo {
      TimeInfo(std::tm* src)
          : year(src->tm_year + 1900), month(src->tm_mon + 1), day(src->tm_mday), hour(src->tm_hour), min(src->tm_min), sec(src->tm_sec), msec(0), usec(0) {}
      TimeInfo(std::chrono::system_clock::time_point now) {
        std::time_t raw_time = std::chrono::system_clock::to_time_t(now);
        std::tm* lt_tm = std::localtime(&raw_time);
        year = lt_tm->tm_year + 1900;
        month = lt_tm->tm_mon + 1;
        day = lt_tm->tm_mday;
        hour = lt_tm->tm_hour;
        min = lt_tm->tm_min;
        sec = lt_tm->tm_sec;

        size_t us_epoch = std::chrono::duration_cast<std::chrono::microseconds>(
                              now.time_since_epoch())
                              .count();
        msec = (us_epoch / 1000) % 1000;
        usec = us_epoch % 1000;
      }
      int year;
      int month;
      int day;
      int hour;
      int min;
      int sec;
      int msec;
      int usec;
    };

   public:
    TestSuite(int argc = 0, char** argv = nullptr)
        : cntPass(0), cntFail(0), useGivenRange(false), preserveTestFiles(false), forceAbortOnFailure(false), suppressMsg(false), displayMsg(false), givenRange(0), startTimeGlobal(std::chrono::system_clock::now()) {
      for (int ii = 1; ii < argc; ++ii) {
        // Filter.
        if (ii < argc - 1 && (!strcmp(argv[ii], "-f") || !strcmp(argv[ii], "--filter"))) {
          filter = argv[++ii];
        }

        // Range.
        if (ii < argc - 1 && (!strcmp(argv[ii], "-r") || !strcmp(argv[ii], "--range"))) {
          givenRange = atoi(argv[++ii]);
          useGivenRange = true;
        }

        // Do not clean up test files after test.
        if (!strcmp(argv[ii], "-p") || !strcmp(argv[ii], "--preserve")) {
          preserveTestFiles = true;
        }

        // Force abort on failure.
        if (!strcmp(argv[ii], "--abort-on-failure")) {
          forceAbortOnFailure = true;
        }

        // Suppress test messages.
        if (!strcmp(argv[ii], "--suppress-msg")) {
          suppressMsg = true;
        }

        // Display test messages.
        if (!strcmp(argv[ii], "--display-msg") && !suppressMsg) {
          displayMsg = true;
        }

        // Help
        if (!strcmp(argv[ii], "-h") || !strcmp(argv[ii], "--help")) {
          usage(argc, argv);
          exit(0);
        }
      }
    }

    ~TestSuite() {
      std::chrono::time_point<std::chrono::system_clock> cur_time =
          std::chrono::system_clock::now();
      ;
      std::chrono::duration<double> elapsed = cur_time - startTimeGlobal;
      std::string time_str = usToString((uint64_t)(elapsed.count() * 1000000));

      printf(_CL_GREEN("%zu") " tests passed", cntPass);
      if (cntFail) {
        printf(", " _CL_RED("%zu") " tests failed", cntFail);
      }
      printf(" out of " _CL_CYAN("%zu") " (" _CL_BROWN("%s") ")\n", cntPass + cntFail, time_str.c_str());
    }

    // === Helper functions ====================================
    static std::string getTestFileName(const std::string& prefix) {
      TimeInfo lt(std::chrono::system_clock::now());
      (void)lt;

      char time_char[64];
      sprintf(time_char, "%04d%02d%02d_%02d%02d%02d", lt.year, lt.month, lt.day, lt.hour, lt.min, lt.sec);

      std::string ret = prefix;
      ret += "_";
      ret += time_char;
      return ret;
    }

    static std::string getTimeString() {
      TimeInfo lt(std::chrono::system_clock::now());
      char time_char[64];
      sprintf(time_char, "%04d-%02d-%02d %02d:%02d:%02d.%03d%03d", lt.year, lt.month, lt.day, lt.hour, lt.min, lt.sec, lt.msec, lt.usec);
      return time_char;
    }
    static std::string getTimeStringShort() {
      TimeInfo lt(std::chrono::system_clock::now());
      char time_char[64];
      sprintf(time_char, "%02d:%02d.%03d %03d", lt.min, lt.sec, lt.msec, lt.usec);
      return time_char;
    }
    static std::string getTimeStringPlain() {
      TimeInfo lt(std::chrono::system_clock::now());
      char time_char[64];
      sprintf(time_char, "%02d%02d_%02d%02d%02d", lt.month, lt.day, lt.hour, lt.min, lt.sec);
      return time_char;
    }

    static int mkdir(const std::string& path) {
      struct stat st;
      if (stat(path.c_str(), &st) != 0) {
        return ::mkdir(path.c_str(), 0755);
      }

      return 0;
    }
    static int copyfile(const std::string& src, const std::string& dst) {
      std::string cmd = "cp -R " + src + " " + dst;
      int rc = ::system(cmd.c_str());
      return rc;
    }
    static int remove(const std::string& path) {
      int rc = ::remove(path.c_str());
      return rc;
    }
    static bool exist(const std::string& path) {
      struct stat st;
      int result = stat(path.c_str(), &st);
      return (result == 0);
    }

    enum TestPosition {
      BEGINNING_OF_TEST = 0,
      MIDDLE_OF_TEST = 1,
      END_OF_TEST = 2,
    };
    static void clearTestFile(const std::string& prefix, TestPosition test_pos = MIDDLE_OF_TEST) {
      TestSuite*& cur_test = TestSuite::getCurTest();
      if (test_pos == END_OF_TEST && (cur_test->preserveTestFiles || cur_test->options.preserveTestFiles))
        return;

      int r;
      std::string command = "rm -rf ";
      command += prefix;
      command += "*";
      r = system(command.c_str());
      (void)r;
    }

    static void setResultMessage(const std::string& msg) { TestSuite::getResMsg() = msg; }

    static void appendResultMessage(const std::string& msg) {
      std::lock_guard<std::mutex> l(TestSuite::getResMsgLock());
      TestSuite::getResMsg() += msg;
    }

    static size_t _msg(const char* format, ...) {
      size_t cur_len = 0;
      TestSuite* cur_test = TestSuite::getCurTest();
      if ((cur_test && (cur_test->options.printTestMessage || cur_test->displayMsg) && !cur_test->suppressMsg) || globalMsgFlag()) {
        va_list args;
        va_start(args, format);
        cur_len += vprintf(format, args);
        va_end(args);
      }
      return cur_len;
    }
    static size_t _msgt(const char* format, ...) {
      size_t cur_len = 0;
      TestSuite* cur_test = TestSuite::getCurTest();
      if ((cur_test && (cur_test->options.printTestMessage || cur_test->displayMsg) && !cur_test->suppressMsg) || globalMsgFlag()) {
        std::cout << _CLM_D_GRAY << getTimeStringShort() << _CLM_END << "] ";
        va_list args;
        va_start(args, format);
        cur_len += vprintf(format, args);
        va_end(args);
      }
      return cur_len;
    }

    class Msg {
     public:
      Msg() {}

      template <typename T>
      inline Msg& operator<<(const T& data) {
        if (TestSuite::isMsgAllowed()) {
          std::cout << data;
        }
        return *this;
      }

      using MyCout = std::basic_ostream<char, std::char_traits<char>>;
      typedef MyCout& (*EndlFunc)(MyCout&);

      Msg& operator<<(EndlFunc func) {
        if (TestSuite::isMsgAllowed()) {
          func(std::cout);
        }
        return *this;
      }
    };

    static void sleep_us(size_t us, const std::string& msg = std::string()) {
      if (!msg.empty()) TestSuite::_msg("%s (%zu us)\n", msg.c_str(), us);
      std::this_thread::sleep_for(std::chrono::microseconds(us));
    }
    static void sleep_ms(size_t ms, const std::string& msg = std::string()) {
      if (!msg.empty()) TestSuite::_msg("%s (%zu ms)\n", msg.c_str(), ms);
      std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    static void sleep_sec(size_t sec, const std::string& msg = std::string()) {
      if (!msg.empty()) TestSuite::_msg("%s (%zu s)\n", msg.c_str(), sec);
      std::this_thread::sleep_for(std::chrono::seconds(sec));
    }
    static std::string lzStr(size_t digit, uint64_t num) {
      std::stringstream ss;
      ss << std::setw(digit) << std::setfill('0') << std::to_string(num);
      return ss.str();
    }
    static double calcThroughput(uint64_t ops, uint64_t elapsed_us) {
      return ops * 1000000.0 / elapsed_us;
    }
    static std::string throughputStr(uint64_t ops, uint64_t elapsed_us) {
      return countToString(ops * 1000000 / elapsed_us);
    }
    static std::string sizeThroughputStr(uint64_t size_byte, uint64_t elapsed_us) {
      return sizeToString(size_byte * 1000000 / elapsed_us);
    }

    // === Timer things ====================================
    class Timer {
     public:
      Timer()
          : duration_ms(0) {
        reset();
      }
      Timer(size_t _duration_ms)
          : duration_ms(_duration_ms) {
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
      void reset() { start = std::chrono::system_clock::now(); }
      void resetSec(size_t _duration_sec) {
        duration_ms = _duration_sec * 1000;
        reset();
      }
      void resetMs(size_t _duration_ms) {
        duration_ms = _duration_ms;
        reset();
      }

     private:
      std::chrono::time_point<std::chrono::system_clock> start;
      size_t duration_ms;
    };

    // === Workload generator things ====================================
    class WorkloadGenerator {
     public:
      WorkloadGenerator(double ops_per_sec = 0.0, uint64_t max_ops_per_batch = 0)
          : opsPerSec(ops_per_sec), maxOpsPerBatch(max_ops_per_batch), numOpsDone(0) {
        reset();
      }
      void reset() {
        start = std::chrono::system_clock::now();
        numOpsDone = 0;
      }
      size_t getNumOpsToDo() {
        if (opsPerSec <= 0) return 0;

        auto cur = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed = cur - start;

        double exp = opsPerSec * elapsed.count();
        if (numOpsDone < exp) {
          if (maxOpsPerBatch) {
            return std::min(maxOpsPerBatch, (uint64_t)exp - numOpsDone);
          }
          return (uint64_t)exp - numOpsDone;
        }
        return 0;
      }
      void addNumOpsDone(size_t num) { numOpsDone += num; }

     private:
      std::chrono::time_point<std::chrono::system_clock> start;
      double opsPerSec;
      uint64_t maxOpsPerBatch;
      uint64_t numOpsDone;
    };

    // === Progress things ==================================
    // Progress that knows the maximum value.
    class Progress {
     public:
      Progress(uint64_t _num, const std::string& _comment = std::string(), const std::string& _unit = std::string())
          : curValue(0), num(_num), timer(0), lastPrintTimeUs(timer.getTimeUs()), comment(_comment), unit(_unit) {}
      void update(uint64_t cur) {
        curValue = cur;
        uint64_t curTimeUs = timer.getTimeUs();
        if (curTimeUs - lastPrintTimeUs > 50000 || cur == 0 || curValue >= num) {
          // Print every 0.05 sec (20 Hz).
          lastPrintTimeUs = curTimeUs;
          std::string _comment = (comment.empty()) ? "" : comment + ": ";
          std::string _unit = (unit.empty()) ? "" : unit + " ";

          Msg mm;
          mm << "\r" << _comment << curValue << "/" << num << " " << _unit
             << std::fixed << std::setprecision(1) << "("
             << (double)curValue * 100 / num << "%)";
          fflush(stdout);
        }
        if (curValue >= num) {
          _msg("\n");
          fflush(stdout);
        }
      }
      void done() {
        if (curValue < num) update(num);
      }

     private:
      uint64_t curValue;
      uint64_t num;
      Timer timer;
      uint64_t lastPrintTimeUs;
      std::string comment;
      std::string unit;
    };

    // Progress that doesn't know the maximum value.
    class UnknownProgress {
     public:
      UnknownProgress(const std::string& _comment = std::string(), const std::string& _unit = std::string())
          : curValue(0), timer(0), lastPrintTimeUs(timer.getTimeUs()), comment(_comment), unit(_unit) {}
      void update(uint64_t cur) {
        curValue = cur;
        uint64_t curTimeUs = timer.getTimeUs();
        if (curTimeUs - lastPrintTimeUs > 50000 || cur == 0) {
          // Print every 0.05 sec (20 Hz).
          lastPrintTimeUs = curTimeUs;
          std::string _comment = (comment.empty()) ? "" : comment + ": ";
          std::string _unit = (unit.empty()) ? "" : unit + " ";

          _msg("\r%s%ld %s", _comment.c_str(), curValue, _unit.c_str());
          fflush(stdout);
        }
      }
      void done() {
        _msg("\n");
        fflush(stdout);
      }

     private:
      uint64_t curValue;
      Timer timer;
      uint64_t lastPrintTimeUs;
      std::string comment;
      std::string unit;
    };

    // === Displayer things ==================================
    class Displayer {
     public:
      Displayer(size_t num_raws, size_t num_cols)
          : numRaws(num_raws), numCols(num_cols), colWidth(num_cols, 20), context(num_raws, std::vector<std::string>(num_cols)) {}
      void init() {
        for (size_t ii = 0; ii < numRaws; ++ii)
          _msg("\n");
      }
      void setWidth(std::vector<size_t>& src) {
        size_t num_src = src.size();
        if (!num_src) return;

        for (size_t ii = 0; ii < num_src; ++ii) {
          colWidth[ii] = src[ii];
        }
        for (size_t ii = num_src; ii < numCols; ++ii) {
          colWidth[ii] = src[num_src - 1];
        }
      }
      void set(size_t raw_idx, size_t col_idx, const char* format, ...) {
        if (raw_idx >= numRaws || col_idx >= numCols) return;

        thread_local char info_buf[32];
        size_t len = 0;
        va_list args;
        va_start(args, format);
        len += vsnprintf(info_buf + len, 20 - len, format, args);
        va_end(args);
        context[raw_idx][col_idx] = info_buf;
      }
      void print() {
        _msg("\033[%zuA", numRaws);
        for (size_t ii = 0; ii < numRaws; ++ii) {
          std::stringstream ss;
          for (size_t jj = 0; jj < numCols; ++jj) {
            ss << std::setw(colWidth[jj]) << context[ii][jj];
          }
          _msg("\r%s\n", ss.str().c_str());
        }
      }

     private:
      size_t numRaws;
      size_t numCols;
      std::vector<size_t> colWidth;
      std::vector<std::vector<std::string>> context;
    };

    // === Gc things ====================================
    template <typename T, typename T2 = T>
    class GcVar {
     public:
      GcVar(T& _src, T2 _to)
          : src(_src), to(_to) {}
      ~GcVar() {
        // GC by value.
        src = to;
      }

     private:
      T& src;
      T2 to;
    };

    class GcFunc {
     public:
      GcFunc(std::function<void()> _func)
          : func(_func) {}
      ~GcFunc() {
        // GC by function.
        func();
      }

     private:
      std::function<void()> func;
    };

    // === Thread things ====================================
    struct ThreadArgs { /* Opaque. */
    };
    using ThreadFunc = std::function<int(ThreadArgs*)>;
    using ThreadExitHandler = std::function<void(ThreadArgs*)>;

   private:
    struct ThreadInternalArgs {
      ThreadInternalArgs()
          : userArgs(nullptr), func(nullptr), rc(0) {}
      ThreadArgs* userArgs;
      ThreadFunc func;
      int rc;
    };

   public:
    struct ThreadHolder {
      ThreadHolder()
          : tid(nullptr), handler(nullptr) {}
      ThreadHolder(std::thread* _tid, ThreadExitHandler _handler)
          : tid(_tid), handler(_handler) {}
      ThreadHolder(ThreadArgs* u_args, ThreadFunc t_func, ThreadExitHandler t_handler)
          : tid(nullptr), handler(nullptr) {
        spawn(u_args, t_func, t_handler);
      }

      ~ThreadHolder() { join(true); }

      void spawn(ThreadArgs* u_args, ThreadFunc t_func, ThreadExitHandler t_handler) {
        if (tid) return;
        handler = t_handler;
        args.userArgs = u_args;
        args.func = t_func;
        tid = new std::thread(spawnThread, &args);
      }

      void join(bool force = false) {
        if (!tid) return;
        if (tid->joinable()) {
          if (force) {
            // Force kill.
            handler(args.userArgs);
          }
          tid->join();
        }
        delete tid;
        tid = nullptr;
      }
      int getResult() const { return args.rc; }
      std::thread* tid;
      ThreadExitHandler handler;
      ThreadInternalArgs args;
    };

    // === doTest things ====================================

    // 1) Without parameter.
    void doTest(const std::string& test_name, test_func func) {
      if (!matchFilter(test_name)) return;

      readyTest(test_name);
      TestSuite::getResMsg() = "";
      TestSuite::getInfoMsg() = "";
      TestSuite::getCurTest() = this;
      int ret = func();
      reportTestResult(test_name, ret);
    }

    // 2) Ranged parameter.
    template <typename T, typename F>
    void doTest(std::string test_name, F func, TestRange<T> range) {
      if (!matchFilter(test_name)) return;

      size_t n = (useGivenRange) ? 1 : range.getSteps();
      size_t i;

      for (i = 0; i < n; ++i) {
        std::string actual_test_name = test_name;
        std::stringstream ss;

        T cur_arg = (useGivenRange) ? givenRange : range.getEntry(i);

        ss << cur_arg;
        actual_test_name += " (" + ss.str() + ")";
        readyTest(actual_test_name);

        TestSuite::getResMsg() = "";
        TestSuite::getInfoMsg() = "";
        TestSuite::getCurTest() = this;

        int ret = func(cur_arg);
        reportTestResult(actual_test_name, ret);
      }
    }

    // 3) Generic one-time parameters.
    template <typename T1, typename... T2, typename F>
    void doTest(const std::string& test_name, F func, T1 arg1, T2... args) {
      if (!matchFilter(test_name)) return;

      readyTest(test_name);
      TestSuite::getResMsg() = "";
      TestSuite::getInfoMsg() = "";
      TestSuite::getCurTest() = this;
      int ret = func(arg1, args...);
      reportTestResult(test_name, ret);
    }

    // 4) Multi composite parameters.
    template <typename F>
    void doTest(const std::string& test_name, F func, TestArgsWrapper& args_wrapper) {
      if (!matchFilter(test_name)) return;

      TestArgsBase* args = args_wrapper.getArgs();
      args->setCallback(test_name, func, this);
      args->testAll();
    }

    TestOptions options;

   private:
    void doTestCB(const std::string& test_name, test_func_args func, TestArgsBase* args) {
      readyTest(test_name);
      TestSuite::getResMsg() = "";
      TestSuite::getInfoMsg() = "";
      TestSuite::getCurTest() = this;
      int ret = func(args);
      reportTestResult(test_name, ret);
    }

    static void spawnThread(ThreadInternalArgs* args) {
      args->rc = args->func(args->userArgs);
    }

    bool matchFilter(const std::string& test_name) {
      if (!filter.empty() && test_name.find(filter) == std::string::npos) {
        // Doesn't match with the given filter.
        return false;
      }
      return true;
    }

    void readyTest(const std::string& test_name) {
      printf(
          "[ "
          "...."
          " ] %s\n",
          test_name.c_str());
      if ((options.printTestMessage || displayMsg) && !suppressMsg) {
        printf(_CL_D_GRAY("   === TEST MESSAGE (BEGIN) ===\n"));
      }
      fflush(stdout);

      getTestName() = test_name;
      startTimeLocal = std::chrono::system_clock::now();
    }

    void reportTestResult(const std::string& test_name, int result) {
      std::chrono::time_point<std::chrono::system_clock> cur_time =
          std::chrono::system_clock::now();
      ;
      std::chrono::duration<double> elapsed = cur_time - startTimeLocal;
      std::string time_str = usToString((uint64_t)(elapsed.count() * 1000000));

      char msg_buf[1024];
      std::string res_msg = TestSuite::getResMsg();
      sprintf(msg_buf, "%s (" _CL_BROWN("%s") ")%s%s", test_name.c_str(), time_str.c_str(), (res_msg.empty() ? "" : ": "), res_msg.c_str());

      if (result < 0) {
        printf("[ " _CL_RED("FAIL") " ] %s\n", msg_buf);
        cntFail++;
      } else {
        if ((options.printTestMessage || displayMsg) && !suppressMsg) {
          printf(_CL_D_GRAY("   === TEST MESSAGE (END) ===\n"));
        } else {
          // Move a line up.
          printf("\033[1A");
          // Clear current line.
          printf("\r");
          // And then overwrite.
        }
        printf("[ " _CL_GREEN("PASS") " ] %s\n", msg_buf);
        cntPass++;
      }

      if (result != 0 && (options.abortOnFailure || forceAbortOnFailure)) {
        abort();
      }
      getTestName().clear();
    }

    size_t cntPass;
    size_t cntFail;
    std::string filter;
    bool useGivenRange;
    bool preserveTestFiles;
    bool forceAbortOnFailure;
    bool suppressMsg;
    bool displayMsg;
    int64_t givenRange;
    // Start time of each test.
    std::chrono::time_point<std::chrono::system_clock> startTimeLocal;
    // Start time of the entire test suite.
    std::chrono::time_point<std::chrono::system_clock> startTimeGlobal;
  };

  // ===== Functor =====

  struct TestArgsSetParamFunctor {
    template <typename T>
    void operator()(T* t, TestRange<T>& r, size_t param_idx) const {
      *t = r.getEntry(param_idx);
    }
  };

  template <std::size_t I = 0, typename FuncT, typename... Tp>
  inline typename std::enable_if<I == sizeof...(Tp), void>::type TestArgsSetParamScan(
      int,
      std::tuple<Tp*...>&,
      std::tuple<TestRange<Tp>...>&,
      FuncT,
      size_t) {}

  template <std::size_t I = 0, typename FuncT, typename... Tp>
      inline typename std::enable_if < I<sizeof...(Tp), void>::type TestArgsSetParamScan(int index, std::tuple<Tp*...>& t, std::tuple<TestRange<Tp>...>& r, FuncT f, size_t param_idx) {
    if (index == 0) f(std::get<I>(t), std::get<I>(r), param_idx);
    TestArgsSetParamScan<I + 1, FuncT, Tp...>(index - 1, t, r, f, param_idx);
  }
  struct TestArgsGetNumStepsFunctor {
    template <typename T>
    void operator()(T* t, TestRange<T>& r, size_t& steps_ret) const {
      (void)t;
      steps_ret = r.getSteps();
    }
  };

  template <std::size_t I = 0, typename FuncT, typename... Tp>
  inline typename std::enable_if<I == sizeof...(Tp), void>::type TestArgsGetStepsScan(
      int,
      std::tuple<Tp*...>&,
      std::tuple<TestRange<Tp>...>&,
      FuncT,
      size_t) {}

  template <std::size_t I = 0, typename FuncT, typename... Tp>
      inline typename std::enable_if < I<sizeof...(Tp), void>::type TestArgsGetStepsScan(int index, std::tuple<Tp*...>& t, std::tuple<TestRange<Tp>...>& r, FuncT f, size_t& steps_ret) {
    if (index == 0) f(std::get<I>(t), std::get<I>(r), steps_ret);
    TestArgsGetStepsScan<I + 1, FuncT, Tp...>(index - 1, t, r, f, steps_ret);
  }

#define TEST_ARGS_CONTENTS()                                                         \
  void setParam(size_t param_no, size_t param_idx) {                                 \
    TestArgsSetParamScan(                                                            \
        param_no,                                                                    \
        args,                                                                        \
        ranges,                                                                      \
        TestArgsSetParamFunctor(),                                                   \
        param_idx);                                                                  \
  }                                                                                  \
  size_t getNumSteps(size_t param_no) {                                              \
    size_t ret = 0;                                                                  \
    TestArgsGetStepsScan(param_no, args, ranges, TestArgsGetNumStepsFunctor(), ret); \
    return ret;                                                                      \
  }                                                                                  \
  size_t getNumParams() { return std::tuple_size<decltype(args)>::value; }

  // ===== TestArgsBase =====

  void TestArgsBase::testAllInternal(size_t depth) {
    size_t i;
    size_t n_params = getNumParams();
    size_t n_steps = getNumSteps(depth);

    for (i = 0; i < n_steps; ++i) {
      setParam(depth, i);
      if (depth + 1 < n_params) {
        testAllInternal(depth + 1);
      } else {
        std::string test_name;
        std::string args_name = toString();
        if (!args_name.empty()) {
          test_name = testName + " (" + args_name + ")";
        }
        testInstance->doTestCB(test_name, testFunction, this);
      }
    }
  }

  // ===== Parameter macros =====

#define DEFINE_PARAMS_2(name, type1, param1, range1, type2, param2, range2)       \
  class name##_class : public TestArgsBase {                                      \
   public:                                                                        \
    name##_class() {                                                              \
      args = std::make_tuple(&param1, &param2);                                   \
      ranges = std::make_tuple(TestRange<type1> range1, TestRange<type2> range2); \
    }                                                                             \
    std::string toString() {                                                      \
      std::stringstream ss;                                                       \
      ss << param1 << ", " << param2;                                             \
      return ss.str();                                                            \
    }                                                                             \
    TEST_ARGS_CONTENTS()                                                          \
    type1 param1;                                                                 \
    type2 param2;                                                                 \
                                                                                  \
   private:                                                                       \
    std::tuple<type1*, type2*> args;                                              \
    std::tuple<TestRange<type1>, TestRange<type2>> ranges;                        \
  };

#define DEFINE_PARAMS_3(                                                                                   \
    name,                                                                                                  \
    type1,                                                                                                 \
    param1,                                                                                                \
    range1,                                                                                                \
    type2,                                                                                                 \
    param2,                                                                                                \
    range2,                                                                                                \
    type3,                                                                                                 \
    param3,                                                                                                \
    range3)                                                                                                \
  class name##_class : public TestArgsBase {                                                               \
   public:                                                                                                 \
    name##_class() {                                                                                       \
      args = std::make_tuple(&param1, &param2, &param3);                                                   \
      ranges = std::make_tuple(TestRange<type1> range1, TestRange<type2> range2, TestRange<type3> range3); \
    }                                                                                                      \
    std::string toString() {                                                                               \
      std::stringstream ss;                                                                                \
      ss << param1 << ", " << param2 << ", " << param3;                                                    \
      return ss.str();                                                                                     \
    }                                                                                                      \
    TEST_ARGS_CONTENTS()                                                                                   \
    type1 param1;                                                                                          \
    type2 param2;                                                                                          \
    type3 param3;                                                                                          \
                                                                                                           \
   private:                                                                                                \
    std::tuple<type1*, type2*, type3*> args;                                                               \
    std::tuple<TestRange<type1>, TestRange<type2>, TestRange<type3>> ranges;                               \
  };

#define SET_PARAMS(name) TestArgsWrapper name(new name##_class())

#define GET_PARAMS(name) name##_class* name = static_cast<name##_class*>(TEST_args_base__)

#define PARAM_BASE TestArgsBase* TEST_args_base__

#define TEST_SUITE_AUTO_PREFIX __func__

#define TEST_SUITE_PREPARE_PATH(path)                           \
  const std::string _ts_auto_prefiix_ = TEST_SUITE_AUTO_PREFIX; \
  TestSuite::clearTestFile(_ts_auto_prefiix_);                  \
  path = TestSuite::getTestFileName(_ts_auto_prefiix_);

#define TEST_SUITE_CLEANUP_PATH() \
  TestSuite::clearTestFile(_ts_auto_prefiix_, TestSuite::END_OF_TEST);

}  // namespace app::consensus::_TestSuite

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
