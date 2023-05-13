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
      bool r = app::client::open_lock("./test");
      if (r) {
        cout << "Lock created" << endl;
      } else {
        cout << red << "Failed to open lock, ending test for server" << reset << endl;
      }
    } else if (cmd == "acquire_lock") {
      grpc::Status status = app::client::acquire_lock("./test", LockStatus::EXCLUSIVE);

      if (status.ok()) {
        cout << "Correctly acquired lock" << endl;
      } else {
        cout << red << "unable to delete lock" << reset << endl;
      }
    } else if (cmd == "write") {
      grpc::Status status = app::client::write("./test", "hello");
      if (status.ok()) {
        cout << green << "Correctly wrote 'hello' to the server" << reset << endl;
      } else {
        cout << red << "Was not able to write even though exclusive lock." << reset << endl;
      }
    } else if (cmd == "read") {
      std::pair<grpc::Status, std::string> res = app::client::read("./test");
      if (res.first.ok()) {
        cout << green << "Correctly read: '" << res.second << "' from server." << reset << endl;
      } else {
        cout << red << "Was not able to read from server." << reset << endl;
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
    std::cout << "\nstart_session\n"
              << "\n"
              << "open_lock <path>\n"
              << "\n"
              << "write <path> <content>\n"
              << "\n"
              << "read <path>\n"
              << "\n"
              << "close_session\n"
              << "\n"
              << "get current server status: st (or stat)\n"
              << "\n"
              << "get the list of members: ls (or list)\n"
              << "\n";
  }

}  // namespace interactive