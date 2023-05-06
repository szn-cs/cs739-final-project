#include "./common.h"

// interactive command prompt for testing the client
namespace interactive {

  bool do_cmd(const std::vector<std::string>& tokens) {  // TODO: call functionality form Chubby server/client
    if (!tokens.size()) return true;

    const std::string& cmd = tokens[0];

    if (cmd == "q" || cmd == "exit") {
      return false;
    } else if (cmd[0] == '+' || cmd[0] == '-' || cmd[0] == '*' || cmd[0] == '/') {
      // do something with Chubby specific commands
    } else if (cmd == "add") {
      // connect to server
    } else if (cmd == "st" || cmd == "stat") {
      // print information about session, cluster, consensus, handles, locks, etc.
    } else if (cmd == "ls" || cmd == "list") {
      // list Chubby servers
    } else if (cmd == "h" || cmd == "help") {
      help();
    }

    return true;
  }

  void help() {  // TODO: replace with Chubby instructions
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

}  // namespace interactive