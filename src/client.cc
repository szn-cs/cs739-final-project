#include "common.h"

namespace app::client {
  std::string info::session_id;
  chrono::system_clock::time_point info::lease_start;
  chrono::milliseconds info::lease_length;
  std::shared_ptr<map<std::string, LockStatus>> info::locks;
  bool info::jeopardy;
  bool info::expired;
  std::shared_ptr<Node> info::master;

  grpc::Status start_session() {
    using namespace std::chrono;

    // Session ID is simply the client's ip:port, acting as a simple uniquifier
    info::session_id = app::State::config->getAddress<app::Service::NODE>().toString();
    info::locks = std::make_shared<std::map<std::string, LockStatus>>();
    info::jeopardy = false;
    info::expired = false;
    info::lease_length = milliseconds(utility::DEFAULT_LEASE_DURATION);
    info::master = nullptr;

    // Lease start is in seconds since epoch
    info::lease_start = system_clock::now();

    // Try to establish session with all nodes in cluster
    for (const auto& [key, node] : *(State::memberList)) {
      grpc::Status status = node->endpoint.init_session(info::session_id);

      if (!status.ok()) {  // If server is down or replies with grpc::StatusCode::ABORTED
        if (State::config->flag.debug) {
          cout << grey << "Node " << key << " replied with an error." << reset << endl;
        }
        continue;
      }

      // Session established with server
      info::master = node;
      break;
    }

    if (info::master == nullptr) {
      if (State::config->flag.debug) {
        cout << red << "Could not establish a session with any server." << reset << endl;
      }
      return Status::CANCELLED;
    }

    // Thread to handle maintaining the session (i.e. issuing keep_alives and such)
    std::thread t(maintain_session);
    t.detach();

    return Status::OK;
  }

  void close_session() {
    if (State::config->flag.debug) {
      cout << yellow << "Ending session." << reset << endl;
    }

    if (info::session_id.empty()) {
      if (State::config->flag.debug) {
        cout << red << "No session to end." << reset << endl;
      }
      return;
    }

    grpc::Status status = info::master->endpoint.close_session(info::session_id);

    // Debugging
    if (status.ok()) {
      if (State::config->flag.debug) {
        cout << green << "Successfully closed session." << reset << endl;
      }
    } else {
      if (State::config->flag.debug) {
        cout << red << "Failed to close session." << reset << endl;
      }
    }
  }

  void maintain_session() {
    if (State::config->flag.debug) {
      cout << yellow << "Beginning session maitenence." << reset << endl;
    }

    // Shouldn't ever happen
    if (info::master == nullptr) {
      return;
    }

    auto stopJeopardy = info::lease_start + info::lease_length + chrono::milliseconds(utility::JEAPARDY_DURATION);

    while (true) {
      // Wait until response or the deadline is reached
      chrono::system_clock::time_point deadline = info::lease_start + info::lease_length;
      std::pair<grpc::Status, int64_t> r = info::master->endpoint.keep_alive(info::session_id, deadline);
      auto [status, new_lease_length] = r;

      // If we successfully heard back from server before deadline
      if (status.ok()) {
        if (State::config->flag.debug) {
          cout << grey << "keep_alive response received. Lease extended." << reset << endl;
        }
        info::lease_length = chrono::milliseconds(new_lease_length);
      } else {
        if (State::config->flag.debug) {
          cout << red << "Entering jeopardy." << reset << endl;
        }
        info::jeopardy = true;

        // Send keep alives which include the keys we believe we own to every server, keep doing so
        // until we hit jeopardy duration
        bool looping = true;
        while (looping) {
          for (const auto& [key, node] : *(State::memberList)) {
            std::pair<grpc::Status, int64_t> res = node->endpoint.keep_alive(info::session_id, *(info::locks));

            if (!res.first.ok()) {  // If server is down or replies with grpc::StatusCode::ABORTED
              if (State::config->flag.debug) {
                cout << grey << "Node " << key << " replied with an error." << reset << endl;
              }
              continue;
            }

            // Master was found, check if session is still valid
            if (res.second < 0) {
              // Indication that lease is expired, master didn't extend it
              info::expired = true;
              return;
            }

            // Update session info
            info::master = node;
            info::lease_start = chrono::system_clock::now();
            info::lease_length = chrono::milliseconds(res.second);
            info::jeopardy = false;

            // Stop looping
            break;
            looping = false;
          }

          // See if we have exceeded jeopardy duration
          if (chrono::system_clock::now() > stopJeopardy) {
            info::expired = true;
            return;
          }

          std::this_thread::sleep_for(chrono::seconds(1));
        }
        return;
      }
    }
  }

  bool open_lock(std::string file_path) {
    if (info::jeopardy) {
      // Here we would wait for either timeout, or for the session to be reestablished
      if (State::config->flag.debug) {
        cout << grey << "We are in jeopardy." << reset << endl;
      }
      auto stopJeopardy = info::lease_start + info::lease_length + chrono::milliseconds(utility::JEAPARDY_DURATION);
      std::this_thread::sleep_until(stopJeopardy);
      if (info::jeopardy || info::expired) {
        cout << grey << "Session expired." << reset << endl;
        return false;
      }
    }

    grpc::Status status = info::master->endpoint.open_lock(info::session_id, file_path);

    if (status.ok()) {
      if (State::config->flag.debug) {
        cout << green << "Successfully opened lock." << reset << endl;
      }
      return true;
    } else {
      if (State::config->flag.debug) {
        cout << red << "Failed to open lock." << reset << endl;
      }
      return false;
    }
  }

  bool delete_lock(std::string file_path) {
    if (info::jeopardy) {
      // Here we would wait for either timeout, or for the session to be reestablished
      if (State::config->flag.debug) {
        cout << grey << "We are in jeopardy." << reset << endl;
      }
      auto stopJeopardy = info::lease_start + info::lease_length + chrono::milliseconds(utility::JEAPARDY_DURATION);
      std::this_thread::sleep_until(stopJeopardy);
      if (info::jeopardy || info::expired) {
        cout << grey << "Session expired." << reset << endl;
        return false;
      }
    }

    grpc::Status status = info::master->endpoint.delete_lock(info::session_id, file_path);

    if (status.ok()) {
      if (State::config->flag.debug) {
        cout << green << "Successfully deleted lock." << reset << endl;
      }
      return true;
    } else {
      if (State::config->flag.debug) {
        cout << red << "Failed to delete lock." << reset << endl;
      }
      return false;
    }
  }

  grpc::Status acquire_lock(std::string file_path, LockStatus mode) {
    if (info::jeopardy) {
      // Here we would wait for either timeout, or for the session to be reestablished
      if (State::config->flag.debug) {
        cout << grey << "We are in jeopardy." << reset << endl;
      }
      auto stopJeopardy = info::lease_start + info::lease_length + chrono::milliseconds(utility::JEAPARDY_DURATION);
      std::this_thread::sleep_until(stopJeopardy);
      if (info::jeopardy || info::expired) {
        cout << grey << "Session expired." << reset << endl;
        return grpc::Status(StatusCode::ABORTED, "Session expired.");
      }
    }

    grpc::Status status = info::master->endpoint.acquire_lock(info::session_id, file_path, mode);

    // Debugging
    if (status.ok()) {
      if (State::config->flag.debug) {
        cout << green << "Successfully acquired lock." << reset << endl;
      }
      info::locks->insert(std::make_pair(file_path, mode));
    } else {
      if (State::config->flag.debug) {
        cout << red << "Client api unable to acquire lock." << reset << endl;
      }
    }

    return status;
  }

  grpc::Status release_lock(std::string file_path) {
    if (info::jeopardy) {
      // Here we would wait for either timeout, or for the session to be reestablished
      if (State::config->flag.debug) {
        cout << grey << "We are in jeopardy." << reset << endl;
      }
      auto stopJeopardy = info::lease_start + info::lease_length + chrono::milliseconds(utility::JEAPARDY_DURATION);
      std::this_thread::sleep_until(stopJeopardy);
      if (info::jeopardy || info::expired) {
        cout << grey << "Session expired." << reset << endl;
        return grpc::Status(StatusCode::ABORTED, "Session expired.");
      }
    }

    // Check if we hold the lock client side
    if (info::locks->find(file_path) == info::locks->end()) {
      if (State::config->flag.debug) {
        cout << red << "We don't own lock we are trying to release." << reset << endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Client doesn't have the lock you are trying to release.");
    }

    grpc::Status status = info::master->endpoint.release_lock(info::session_id, file_path);

    // Debugging
    if (status.ok()) {
      if (State::config->flag.debug) {
        cout << green << "Successfully released lock." << reset << endl;
      }
      info::locks->erase(file_path);
    } else {
      if (State::config->flag.debug) {
        cout << red << "Client api unable to release lock." << reset << endl;
      }
    }

    return status;
  }

  std::pair<grpc::Status, std::string> read(std::string file_path) {
    if (info::jeopardy) {
      // Here we would wait for either timeout, or for the session to be reestablished
      if (State::config->flag.debug) {
        cout << grey << "We are in jeopardy." << reset << endl;
      }
      auto stopJeopardy = info::lease_start + info::lease_length + chrono::milliseconds(utility::JEAPARDY_DURATION);
      std::this_thread::sleep_until(stopJeopardy);
      if (info::jeopardy || info::expired) {
        cout << grey << "Session expired." << reset << endl;

        grpc::Status s = grpc::Status(StatusCode::ABORTED, "Session expired.");
        std::pair<grpc::Status, std::string> res = std::make_pair(s, "");
        return res;
      }
    }

    // Check if we hold the lock client side
    if (info::locks->find(file_path) == info::locks->end()) {
      if (State::config->flag.debug) {
        cout << red << "We don't own lock we are trying to read from." << reset << endl;
      }
      grpc::Status s = grpc::Status(StatusCode::ABORTED, "Client doesn't have the lock you are trying to read from.");
      std::pair<grpc::Status, std::string> res = std::make_pair(s, "");

      return res;
    }

    std::pair<grpc::Status, std::string> res = info::master->endpoint.read(info::session_id, file_path);

    // Debugging
    if (res.first.ok()) {
      if (State::config->flag.debug) {
        cout << green << "Successfully read from file." << reset << endl;
      }
      info::locks->erase(file_path);
    } else {
      if (State::config->flag.debug) {
        cout << red << "Client api unable to read from file." << reset << endl;
      }
    }

    return res;
  }

  grpc::Status write(std::string file_path, std::string content) {
    if (info::jeopardy) {
      // Here we would wait for either timeout, or for the session to be reestablished
      if (State::config->flag.debug) {
        cout << grey << "We are in jeopardy." << reset << endl;
      }
      auto stopJeopardy = info::lease_start + info::lease_length + chrono::milliseconds(utility::JEAPARDY_DURATION);
      std::this_thread::sleep_until(stopJeopardy);
      if (info::jeopardy || info::expired) {
        cout << grey << "Session expired." << reset << endl;
        return grpc::Status(StatusCode::ABORTED, "Session expired.");
      }
    }

    // Check if we hold the lock client side
    if (info::locks->find(file_path) == info::locks->end()) {
      if (State::config->flag.debug) {
        cout << red << "We don't own lock we are trying to write to." << reset << endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Client doesn't have the lock you are trying to write to.");
    }

    grpc::Status status = info::master->endpoint.write(info::session_id, file_path, content);

    // Debugging
    if (status.ok()) {
      if (State::config->flag.debug) {
        cout << green << "Successfully write to file." << reset << endl;
      }
    } else {
      if (State::config->flag.debug) {
        cout << red << "Client api unable to write to file." << reset << endl;
      }
    }

    return status;
  }

}  // namespace app::client
