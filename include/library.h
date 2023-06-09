#pragma once

#include <assert.h>
#include <bits/stdc++.h>
#include <cxxabi.h>
#include <dirent.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio.hpp>
#include <boost/current_function.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <msd/channel.hpp>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <termcolor/termcolor.hpp>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "VariadicTable.h"  // https://github.com/friedmud/variadic_table
#include "interface.grpc.pb.h"
#include "libnuraft/event_awaiter.hxx"
#include "libnuraft/internal_timer.hxx"
#include "libnuraft/log_store.hxx"
#include "libnuraft/nuraft.hxx"

using namespace std;
using namespace grpc;
using namespace interface;

using grpc::ClientAsyncResponseReader, grpc::CompletionQueue, grpc::ServerAsyncResponseWriter, grpc::ServerCompletionQueue;
using grpc::Server, grpc::ServerBuilder, grpc::ServerContext, grpc::ServerReader, grpc::ServerWriter, grpc::Status;  // https://grpc.github.io/grpc/core/md_doc_statuscodes.html

// colors at https://github.com/ikalnytskyi/termcolor
using termcolor::on_grey,
    termcolor::reset,
    termcolor::on_red,
    termcolor::on_green,
    termcolor::on_yellow,
    termcolor::on_blue,
    termcolor::on_magenta,
    termcolor::on_cyan,
    termcolor::on_white,
    termcolor::on_bright_grey,
    termcolor::on_bright_red,
    termcolor::on_bright_green,
    termcolor::on_bright_yellow,
    termcolor::on_bright_blue,
    termcolor::on_bright_magenta,
    termcolor::on_bright_cyan,
    termcolor::on_bright_white,
    termcolor::yellow,
    termcolor::red,
    termcolor::blue,
    termcolor::cyan,
    termcolor::grey,
    termcolor::magenta,
    termcolor::green,
    termcolor::bright_grey,
    termcolor::bright_red,
    termcolor::bright_green,
    termcolor::bright_yellow,
    termcolor::bright_blue,
    termcolor::bright_magenta,
    termcolor::bright_cyan,
    termcolor::bright_white,
    termcolor::dark,
    termcolor::bold,
    termcolor::italic,
    termcolor::underline,
    termcolor::blink,
    termcolor::reverse,
    termcolor::concealed,
    termcolor::crossed;

namespace fs = std::filesystem;
