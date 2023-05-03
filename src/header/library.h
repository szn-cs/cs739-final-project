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
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <termcolor/termcolor.hpp>
#include <thread>
#include <vector>
#include <future>

#include <msd/channel.hpp>

#include "../../dependency/variadic_table/include/VariadicTable.h"  // https://github.com/friedmud/variadic_table
#include "interface.grpc.pb.h"

using namespace std;
using namespace grpc;
using namespace interface;
using grpc::Server, grpc::ServerBuilder, grpc::ServerContext, grpc::ServerReader, grpc::ServerWriter, grpc::Status;  // https://grpc.github.io/grpc/core/md_doc_statuscodes.html
using grpc::ClientAsyncResponseReader, grpc::CompletionQueue, grpc::ServerAsyncResponseWriter, grpc::ServerCompletionQueue;
using termcolor::reset, termcolor::yellow, termcolor::red, termcolor::blue, termcolor::cyan, termcolor::grey, termcolor::magenta, termcolor::green;
namespace fs = std::filesystem;
