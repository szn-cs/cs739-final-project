#pragma once

#include <bits/stdc++.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <math.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio.hpp>
#include <boost/current_function.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <msd/channel.hpp>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <termcolor/termcolor.hpp>
#include <thread>
#include <vector>

#include "VariadicTable.h"  // https://github.com/friedmud/variadic_table
#include "interface.grpc.pb.h"
#include "libnuraft/nuraft.hxx"

using namespace std;
using namespace grpc;
using namespace interface;
using grpc::ClientAsyncResponseReader, grpc::CompletionQueue, grpc::ServerAsyncResponseWriter, grpc::ServerCompletionQueue;
using grpc::Server, grpc::ServerBuilder, grpc::ServerContext, grpc::ServerReader, grpc::ServerWriter, grpc::Status;  // https://grpc.github.io/grpc/core/md_doc_statuscodes.html
using termcolor::reset, termcolor::yellow, termcolor::red, termcolor::blue, termcolor::cyan, termcolor::grey, termcolor::magenta, termcolor::green;
namespace fs = std::filesystem;

// alias for shared pointer type
template <typename T>
using ptr = std::shared_ptr<T>;

// alias for creating shared pointer
template <typename T, typename... TArgs>
inline ptr<T> ptr_new(TArgs&&... args) {
  return std::make_shared<T>(std::forward<TArgs>(args)...);
}
