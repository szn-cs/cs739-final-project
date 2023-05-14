#include "./common.h"

/** 
 * initialize google/benchmark main with custom code
 * 
 * Alternatively run the benchmark:
 * BENCHMARK_MAIN(); // google/benchmark main macro
*/
// This test is not fully automated and required manual setup of cluster beforehand.
namespace benchmark {
  /*
  void run_session(benchmark::State& state) {

    // run on measurment
    benchmark::DoNotOptimize(app::client::close_session());
    benchmark::ClobberMemory();
  }

  static void session(benchmark::State& state) {
    state.PauseTiming();
    cout << termcolor::grey << utility::getClockTime() << termcolor::reset << endl;
    std::string address = "127.0.1.1:8000";  // can be leader / follower depending on initial setup (not automated)
    state.ResumeTiming();

    // Perform setup here
    for (auto _ : state) {
      grpc::Status t;
      state.PauseTiming();
      t = app::client::start_session();
      if (!t.ok()) {
        cout << red << "UNABLE TO START SESSION: " << t.error_message() << reset << endl;
      }
      state.ResumeTiming();

      // This code gets timed
      run_session(state);
    }
  }

  // Register the function as a benchmark
  // BENCHMARK(benchmark_function)->RangeMultiplier(2)->Range(1 << 10, 1 << 20);
  // BENCHMARK(benchmark_function)->Arg(1);  // ->Arg(200000)->Arg(400000);
  BENCHMARK(benchmark::session)->Iterations(1); // ->Iterations(pow(10, 4));  // ->Arg(200000)->Arg(400000)
  */
}

namespace benchmark {

  void run_acquire(benchmark::State& state) {
            grpc::Status status;


    benchmark::DoNotOptimize(
      status = app::client::acquire_lock("./test", LockStatus::EXCLUSIVE)
    );
    benchmark::ClobberMemory();

    if (status.ok()) {
      cout << "Correctly acquired lock" << endl;
    } else {
      cout << red << "unable to acquire lock" << reset << endl;
      return;
    }
  }

  static void acquire(benchmark::State& state) {
    for (auto _ : state) {
      grpc::Status t;

      state.PauseTiming();
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

      state.ResumeTiming();

      run_acquire(state);

    }
  }
  // BENCHMARK(benchmark::acquire)->Iterations(1); // ->Iterations(pow(10, 4));  // ->Arg(200000)->Arg(400000)


}  // namespace benchmark



namespace benchmark {

  void run_write(benchmark::State& state) {
    grpc::Status status;
    benchmark::DoNotOptimize(
      status = app::client::write("./test", "Hello world")
    );
    benchmark::ClobberMemory();

    if (status.ok()) {
      cout << "Correctly wrote to file" << endl;
    } else {
      cout << red << "Was not able to write to file" << reset << endl;
      return;
    }
  }

  static void write(benchmark::State& state) {
    for (auto _ : state) {
      grpc::Status t;

      state.PauseTiming();

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

      state.ResumeTiming();

      run_write(state);

    }
  }
  BENCHMARK(benchmark::write)->Iterations(1); // ->Iterations(pow(10, 4));  // ->Arg(200000)->Arg(400000)


}  // namespace benchmark

