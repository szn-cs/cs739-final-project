#include "./common.h"

// interactive command prompt for testing the client
namespace interactive {
  using namespace app::consensus;

  bool do_cmd(const std::vector<std::string>& tokens) {
    if (!tokens.size()) return true;

    const std::string& cmd = tokens[0];

    // call functionality from Chubby server/client
    if (cmd == "q" || cmd == "exit") {
      return false;
    } else if (cmd == "start_session") {
      grpc::Status r1 = app::client::start_session();
      if (!r1.ok()) {
        cout << red << "UNABLE TO START SESSION: " << r1.error_message() << reset << endl;
      }
    } else if (cmd == "open_lock") {
      bool r = app::client::open_lock(tokens[1]);
      if (r) {
        cout << "Lock created" << endl;
      } else {
        cout << red << "Failed to open lock, ending test for server" << reset << endl;
      }
    } else if (cmd == "acquire_lock") {
      grpc::Status status;

      if(tokens[1] == "exclusive")
        status = app::client::acquire_lock(tokens[2], LockStatus::EXCLUSIVE);
      else if (tokens[1] == "shared")
        status = app::client::acquire_lock(tokens[2], LockStatus::SHARED);
      else 
        return false; 

      if (status.ok()) {
        cout << "Correctly acquired lock" << endl;
      } else {
        cout << red << "unable to acquire lock" << reset << endl;
      }

    } else if (cmd == "delete") {
      bool r = app::client::delete_lock(tokens[1]);

      if (!r) {
        cout << green << "Correctly failed lock, this was expected to fail." << endl;
      } else {
        cout << red << "Destroyed lock even though we don't own it" << reset << endl;
      }

    } else if (cmd == "write") {
      grpc::Status status = app::client::write(tokens[1], tokens[2]);
      if (status.ok()) {
        cout << green << "Correctly wrote 'hello' to the server" << reset << endl;
      } else {
        cout << red << "Was not able to write even though exclusive lock." << reset << endl;
      }
    } else if (cmd == "read") {
      std::pair<grpc::Status, std::string> res = app::client::read(tokens[1]);
      if (res.first.ok()) {
        cout << green << "Correctly read: '" << res.second << "' from server." << reset << endl;
      } else {
        cout << red << "Was not able to read from server." << reset << endl;
      }

    } else if (cmd == "release_lock") {
      grpc::Status status = app::client::release_lock(tokens[1]);
      if (status.ok()) {
        cout << "Correctly released lock" << endl;
      } else {
        cout << red << "Was not able to release lock" << reset << endl;
      }
    } else if (cmd == "close_session") {
      app::client::close_session();
      cout << cyan << "There should be no errors on this close since we had a session." << reset << endl;
    } else if (cmd == "st" || cmd == "stat") {
      // print information about session, handles, locks, state machine files etc.
      print_status();
      server_list();
    } else if (cmd == "ls" || cmd == "list") {
      // list Chubby servers & cluster
    } else if (cmd == "h" || cmd == "help") {
      help();
    }

    return true;
  }

  void help() {  // TODO: replace with Chubby instructions
    std::cout << grey << "use only relative paths for the files" << reset
              << "\nstart_session\n"
              << "\n"
              << "open_lock <path>\n"
              << "\n"
              << "acquire_lock <exclusive/shared> <path>\n"
              << "\n"
              << "delete <path>\n"
              << "\n"
              << "write <path> <content>\n"
              << "\n"
              << "read <path>\n"
              << "\n"
              << "release_lock <path>\n"
              << "\n"
              << "close_session\n"
              << "\n"

              << "get current server status: st (or stat)\n"
              << "\n"
              << "get the list of members: ls (or list)\n"
              << "\n"
              << "show help (or h)\n"
              << "\n";
  }

}  // namespace interactive