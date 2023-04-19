#include "./declaration.h"

namespace app {

  std::shared_ptr<utility::parse::Config> Cluster::config = nullptr;

  void initializeStaticInstance(std::shared_ptr<utility::parse::Config> config) {
    Cluster::config = config;

    if (config->flag.debug)
      cout << termcolor::grey << "Using config file at: " << config->config << termcolor::reset << endl;
  }

}  // namespace app
