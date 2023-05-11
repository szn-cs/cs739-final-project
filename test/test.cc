#include "./common.h"

// unit/integration tests: asynchronous (non-interactive) tests
namespace test {

  void test_start_session(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    grpc::Status r = app::client::start_session();

    if (r.ok()) {
      cout << green << "Test passed for this configuration." << reset << endl;
    } else {
      cout << red << "Failed to start a session with any nodes." << reset << endl;
      return;
    }
  }

  void test_single_keep_alive(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    for (const auto& [key, node] : *(app::State::memberList)) {
      cout << key << endl;

      grpc::Status r1 = app::client::start_session();
      std::pair<grpc::Status, int64_t> r2 = node->endpoint.keep_alive(app::client::info::session_id, chrono::system_clock::now() + chrono::milliseconds(6000));
      auto [status, v] = r2;

      if (status.ok()) {
        cout << "Value returned: " << v << endl;
      } else {
        cout << red << "Failed RPC" << reset << endl;
      }
    }
  }

  void test_maintain_session(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    grpc::Status r1 = app::client::start_session();
    // Idrk how to test for this without just blocking and allowing for a few rounds of keep_alives to be exchanged
    std::this_thread::sleep_for(chrono::seconds(60));
  }

  void test_create(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    grpc::Status r1 = app::client::start_session();
    if(!r1.ok()){
      cout << red << "UNABLE TO START SESSION: " << r1.error_message() << reset << endl;
    }

    for (const auto& [key, node] : *(app::State::memberList)) {
      cout << key << endl;
      bool r = app::client::open_lock("/test");

      if (r){
        cout << "Lock created" << endl;
      }else{
        cout << red << "Failed to open lock, ending test for server " << key << reset << endl;
        break;
      }

      r = app::client::open_lock("/test");

      if(!r){
        cout << "Second attempt correctly refused." << endl;
      }else{
        cout << red << "Accepted creation of existing lock" << reset << endl;
      }


    }
  }

  void test_delete(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables){
    grpc::Status r1 = app::client::start_session();
    if(!r1.ok()){
      cout << red << "UNABLE TO START SESSION: " << r1.error_message() << reset << endl;
    }

    for (const auto& [key, node] : *(app::State::memberList)) {
      cout << key << endl;
      bool r = app::client::open_lock("/test");

      if (r){
        cout << "Lock created" << endl;
      }else{
        cout << red << "Failed to open lock, ending test for server " << key << reset << endl;
        break;
      }

      /* NOTE: Can only happen when holding the lock. */
      // r = app::client::acquire_lock("/test");
      r = app::client::delete_lock("/test");

      if(r){
        cout << "Correctly deleted lock" << endl;
      }else{
        cout << red << "unable to delete lock" << reset << endl;
        return;
      }

      r = app::client::open_lock("/test");

      if (r){
        cout << "Lock created" << endl;
      }else{
        cout << red << "Failed to open lock, ending test for server " << key << reset << endl;
        break;
      }
    }
  }

}  // namespace test
