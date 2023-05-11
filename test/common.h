#pragma once

#include "../include/common.h"
#include "benchmark/benchmark.h"

typedef void (*TestFnPtr_t)(std::shared_ptr<utility::parse::Config>, boost::program_options::variables_map&);

namespace benchmark {
  void run(rpc::Endpoint& c, const size_t size);
  static void function(benchmark::State& state);
}  // namespace benchmark

namespace test {

  void test_start_session(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables);
  void test_single_keep_alive(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables);
  void test_maintain_session(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables);
  void test_create(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables);
  void test_delete(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables);
  void test_acquire(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables);
  void test_2_clients(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables);
  void test_2_clients_create(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables);

}  // namespace test

namespace interactive {
  bool do_cmd(const std::vector<std::string>& tokens);
  void help();

}  // namespace interactive