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
    grpc::Status r1 = app::client::start_session();
    std::pair<grpc::Status, int64_t> r2 = app::client::info::master->endpoint.keep_alive(app::client::info::session_id, chrono::system_clock::now() + chrono::milliseconds(6000));
    auto [status, v] = r2;

    if (status.ok()) {
      cout << "Value returned: " << v << endl;
    } else {
      cout << red << "Failed RPC" << reset << endl;
    }
  }

  void test_maintain_session(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    grpc::Status r1 = app::client::start_session();
    // Idrk how to test for this without just blocking and allowing for a few rounds of keep_alives to be exchanged
    std::this_thread::sleep_for(chrono::seconds(60));
  }

  void test_create(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    grpc::Status r1 = app::client::start_session();
    if (!r1.ok()) {
      cout << red << "UNABLE TO START SESSION: " << r1.error_message() << reset << endl;
    }

    bool r = app::client::open_lock("./test");
    if (r) {
      cout << "Lock created" << endl;
    } else {
      cout << red << "Failed to open lock, ending test." << reset << endl;
      return;
    }

    r = app::client::open_lock("./test");
    if (!r) {
      cout << "Second attempt correctly refused." << endl;
    } else {
      cout << red << "Accepted creation of existing lock" << reset << endl;
    }
  }

  void test_delete(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    grpc::Status r1 = app::client::start_session();
    if (!r1.ok()) {
      cout << red << "UNABLE TO START SESSION: " << r1.error_message() << reset << endl;
    }

    bool r = app::client::open_lock("./test");
    if (r) {
      cout << "Lock created" << endl;
    } else {
      cout << red << "Failed to open lock, ending test for server" << reset << endl;
      return;
    }

    /* NOTE: Can only happen when holding the lock. */
    // r = app::client::acquire_lock("./test");
    r = app::client::delete_lock("./test");
    if (r) {
      cout << "Correctly deleted lock" << endl;
    } else {
      cout << red << "unable to delete lock" << reset << endl;
      return;
    }

    r = app::client::open_lock("./test");
    if (r) {
      cout << "Lock created" << endl;
    } else {
      cout << red << "Failed to open lock, ending test for server" << reset << endl;
    }
  }

  void test_acquire(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    grpc::Status r1 = app::client::start_session();
    if (!r1.ok()) {
      cout << red << "UNABLE TO START SESSION: " << r1.error_message() << reset << endl;
    }

    bool r = app::client::open_lock("./test");
    if (r) {
      cout << "Lock created" << endl;
    } else {
      cout << red << "Failed to open lock, ending test for server" << reset << endl;
      return;
    }

    r = app::client::delete_lock("./test");
    if (!r) {
      cout << green << "Correctly failed lock, this was expected to fail." << endl;
    } else {
      cout << red << "Destroyed lock even though we don't own it" << reset << endl;
      return;
    }

    grpc::Status status = app::client::acquire_lock("./test", LockStatus::EXCLUSIVE);

    if (status.ok()) {
      cout << "Correctly acquired lock" << endl;
    } else {
      cout << red << "unable to delete lock" << reset << endl;
      return;
    }

    r = app::client::delete_lock("./test");
    if (r) {
      cout << "Correctly destroyed lock." << endl;
    } else {
      cout << red << "Unable to destroy lock even though we own it" << reset << endl;
    }
  }

  void test_2_clients(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    // Start normal session to get master info
    grpc::Status r1 = app::client::start_session();
    if (!r1.ok()) {
      cout << red << "UNABLE TO START SESSION: " << r1.error_message() << reset << endl;
    }

    grpc::Status r2 = app::client::info::master->endpoint.init_session("sess2");
    if (!r2.ok()) {
      cout << red << "UNABLE TO START SESSION 2: " << r2.error_message() << reset << endl;
    }
  }

  void test_2_clients_create(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    // Start normal session to get master info
    grpc::Status r1 = app::client::start_session();
    if (!r1.ok()) {
      cout << red << "UNABLE TO START SESSION: " << r1.error_message() << reset << endl;
    }

    grpc::Status r2 = app::client::info::master->endpoint.init_session("sess2");
    if (!r2.ok()) {
      cout << red << "UNABLE TO START SESSION 2: " << r2.error_message() << reset << endl;
    }

    // Extend session
    chrono::system_clock::time_point deadline = app::client::info::lease_start + app::client::info::lease_length;
    auto r3 = app::client::info::master->endpoint.keep_alive("sess2", deadline);

    //Client 1 opens lock
    bool r = app::client::open_lock("./test");
    if (r) {
      cout << "Lock created" << endl;
    } else {
      cout << red << "Failed to open lock, ending test for server" << reset << endl;
      return;
    }

    // Client 2 opens lock
    grpc::Status r4 = app::client::info::master->endpoint.open_lock("sess2", "./test");
    if (!r4.ok()) {
      cout << "Second attempt correctly refused." << endl;
    } else {
      cout << red << "Accepted creation of existing lock" << reset << endl;
    }
  }

  void test_release(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    grpc::Status r1 = app::client::start_session();
    if (!r1.ok()) {
      cout << red << "UNABLE TO START SESSION: " << r1.error_message() << reset << endl;
    }

    bool r = app::client::open_lock("./test");

    if (r) {
      cout << "Lock created" << endl;
    } else {
      cout << red << "Failed to open lock, ending test for server" << reset << endl;
      return;
    }

    r = app::client::delete_lock("./test");

    if (!r) {
      cout << green << "Correctly failed lock, this was expected to fail." << endl;
    } else {
      cout << red << "Destroyed lock even though we don't own it" << reset << endl;
      return;
    }

    grpc::Status status = app::client::acquire_lock("./test", LockStatus::EXCLUSIVE);

    if (status.ok()) {
      cout << "Correctly acquired lock" << endl;
    } else {
      cout << red << "unable to delete lock" << reset << endl;
      return;
    }

    status = app::client::release_lock("./test");
    if (status.ok()) {
      cout << "Correctly released lock" << endl;
    } else {
      cout << red << "Was not able to release lock" << reset << endl;
      return;
    }

    r = app::client::delete_lock("./test");
    if (r) {
      cout << green << "Correctly was unable to destroy lock." << endl;
    } else {
      cout << red << "Destroyed lock even though we released" << reset << endl;
    }
  }

  void test_read_exclusive(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    grpc::Status r1 = app::client::start_session();
    if (!r1.ok()) {
      cout << red << "UNABLE TO START SESSION: " << r1.error_message() << reset << endl;
    }

    bool r = app::client::open_lock("./test");

    if (r) {
      cout << "Lock created" << endl;
    } else {
      cout << red << "Failed to open lock, ending test for server" << reset << endl;
      return;
    }

    grpc::Status status = app::client::acquire_lock("./test", LockStatus::EXCLUSIVE);

    if (status.ok()) {
      cout << "Correctly acquired lock" << endl;
    } else {
      cout << red << "unable to delete lock" << reset << endl;
      return;
    }

    std::pair<grpc::Status, std::string> res = app::client::read("./test");
    if (res.first.ok()) {
      cout << "Correctly read file (it is blank)" << endl;
    } else {
      cout << red << "Was not able to read file" << reset << endl;
      return;
    }
  }

  void test_read_shared(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    grpc::Status r1 = app::client::start_session();
    if (!r1.ok()) {
      cout << red << "UNABLE TO START SESSION: " << r1.error_message() << reset << endl;
    }

    bool r = app::client::open_lock("./test");

    if (r) {
      cout << "Lock created" << endl;
    } else {
      cout << red << "Failed to open lock, ending test for server" << reset << endl;
      return;
    }

    grpc::Status status = app::client::acquire_lock("./test", LockStatus::SHARED);

    if (status.ok()) {
      cout << "Correctly acquired lock" << endl;
    } else {
      cout << red << "unable to delete lock" << reset << endl;
      return;
    }

    std::pair<grpc::Status, std::string> res = app::client::read("./test");
    if (res.first.ok()) {
      cout << "Correctly read file (it is blank)" << endl;
    } else {
      cout << red << "Was not able to read file" << reset << endl;
      return;
    }
  }

  void test_write_exclusive(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    grpc::Status r1 = app::client::start_session();
    if (!r1.ok()) {
      cout << red << "UNABLE TO START SESSION: " << r1.error_message() << reset << endl;
    }

    bool r = app::client::open_lock("./test");

    if (r) {
      cout << "Lock created" << endl;
    } else {
      cout << red << "Failed to open lock, ending test for server" << reset << endl;
      return;
    }

    grpc::Status status = app::client::acquire_lock("./test", LockStatus::EXCLUSIVE);

    if (status.ok()) {
      cout << "Correctly acquired lock" << endl;
    } else {
      cout << red << "unable to delete lock" << reset << endl;
      return;
    }

    status = app::client::write("./test", "Hello world");
    if (status.ok()) {
      cout << "Correctly wrote to file" << endl;
    } else {
      cout << red << "Was not able to write to file" << reset << endl;
      return;
    }
  }

  void test_write_shared(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    grpc::Status r1 = app::client::start_session();
    if (!r1.ok()) {
      cout << red << "UNABLE TO START SESSION: " << r1.error_message() << reset << endl;
    }

    bool r = app::client::open_lock("./test");

    if (r) {
      cout << "Lock created" << endl;
    } else {
      cout << red << "Failed to open lock, ending test for server" << reset << endl;
      return;
    }

    grpc::Status status = app::client::acquire_lock("./test", LockStatus::SHARED);

    if (status.ok()) {
      cout << "Correctly acquired lock" << endl;
    } else {
      cout << red << "unable to delete lock" << reset << endl;
      return;
    }

    status = app::client::write("./test", "Hello world");
    if (!status.ok()) {
      cout << green << "Correctly blocked the write" << reset << endl;
    } else {
      cout << red << "Was not able to stop the write even though shared lock." << reset << endl;
      return;
    }
  }

  void test_rw(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    grpc::Status r1 = app::client::start_session();
    if (!r1.ok()) {
      cout << red << "UNABLE TO START SESSION: " << r1.error_message() << reset << endl;
    }

    bool r = app::client::open_lock("./test");

    if (r) {
      cout << "Lock created" << endl;
    } else {
      cout << red << "Failed to open lock, ending test for server" << reset << endl;
      return;
    }

    grpc::Status status = app::client::acquire_lock("./test", LockStatus::EXCLUSIVE);

    if (status.ok()) {
      cout << "Correctly acquired lock" << endl;
    } else {
      cout << red << "unable to delete lock" << reset << endl;
      return;
    }

    status = app::client::write("./test", "hello");
    if (status.ok()) {
      cout << green << "Correctly wrote 'hello' to the server" << reset << endl;
    } else {
      cout << red << "Was not able to write even though exclusive lock." << reset << endl;
      return;
    }

    std::pair<grpc::Status, std::string> res = app::client::read("./test");
    if (res.first.ok()) {
      cout << green << "Correctly read: '" << res.second << "' from server." << reset << endl;
    } else {
      cout << red << "Was not able to read from server." << reset << endl;
      return;
    }
  }

  void close_session(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    app::client::close_session();
    cout << cyan << "You should have seen some form of error since no session exists yet." << reset << endl;

    grpc::Status r1 = app::client::start_session();
    if (!r1.ok()) {
      cout << red << "UNABLE TO START SESSION: " << r1.error_message() << reset << endl;
      return;
    }

    bool r = app::client::open_lock("./test");

    if (r) {
      cout << "Lock created" << endl;
    } else {
      cout << red << "Failed to open lock, ending test for server" << reset << endl;
      return;
    }

    grpc::Status status = app::client::acquire_lock("./test", LockStatus::EXCLUSIVE);

    if (status.ok()) {
      cout << "Correctly acquired lock" << endl;
    } else {
      cout << red << "unable to delete lock" << reset << endl;
      return;
    }

    app::client::close_session();
    cout << cyan << "There should be no errors on this close since we had a session." << reset << endl;

    r1 = app::client::start_session();
    if (!r1.ok()) {
      cout << red << "UNABLE TO START SESSION AFTER CLOSING PREVIOUSLY: " << r1.error_message() << reset << endl;
      return;
    }

    status = app::client::acquire_lock("./test", LockStatus::EXCLUSIVE);

    if (status.ok()) {
      cout << "Correctly acquired lock" << endl;
    } else {
      cout << red << "unable to delete lock" << reset << endl;
      return;
    }
  }

}  // namespace test
