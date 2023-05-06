#include "./common.h"

// interactive command prompt for testing the client
namespace interactive {

  bool do_cmd(const std::vector<std::string>& tokens) {
    using namespace consensus;

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

}  // namespace interactive