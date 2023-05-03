#pragma once

#include "../src/header/common.h"
#include "benchmark/benchmark.h"

typedef void (*TestFnPtr_t)(std::shared_ptr<utility::parse::Config>, boost::program_options::variables_map&);
