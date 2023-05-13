#pragma once

#include "library.h"
#include "rpc.h"
#include "struct.h"
#include "utility.h"

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

      string base_directory = utility::concatenatePath(fs::absolute(app::State::config->directory), std::to_string(app::State::stuff.server_id_));

      string mapped_path = utility::concatenatePath(base_directory, payload.path_);
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

      cur_value_ = std::make_shared<std::string>(payload.path_);

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
