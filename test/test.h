#pragma once

#include "benchmark/benchmark.h"
#include "common.h"

typedef void (*TestFnPtr_t)(std::shared_ptr<utility::parse::Config>, boost::program_options::variables_map&);
