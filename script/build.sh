#!/bin/bash

# run using $` (source ./script/build.sh && build) `
build() {
  source ./script/setenv.sh

  # create make files &
  # build through `cmake`  or use `make -w -C ./target/config/`
  cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S . -B ./target/config
  # # NOTE: don't use `` it breaks the build !
  cmake --build ./target/config --parallel # --verbose
  # ## move binaries from nested builds
  # mkdir -p ./target/
  # # copy binaries
  # cp ./target/config/app ./target/
  # cp ./target/config/test ./target/
  # cp ./config/*.ini ./target/
}

build_NuRaft_dependency() {
  ### NOTE: NOT needed anymore as the cmake called from within the main cmake of the project

  PKG=./dependency/NuRaft

  pushd $PKG
  {
    ./prepare.sh # or `git submodule update --init`
    mkdir build
    pushd build
    {
      cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../
      make
    }
    popd
  }
  popd

  # test

  test() {
    # [optional] test functionality of static built library
    #            (use that to verify no external factors cause issues)
    pushd $PKG/build
    ./runtests.sh
    popd
  }
}

build_optimized() {
  echo "requires commenting out the appropriate lines in 'CMakeLists_app.cmake'"
}

## clean
clean() {
  cmake --build ./target/config --target clean
}

install_package() {
  VCPKG=./dependency/vcpkg
  ./${VCPKG}/vcpkg install
  # ./${VCPKG}/vcpkg install ${package_name}

  ################################################

  # versioning -----------------------------------
  # https://learn.microsoft.com/en-us/vcpkg/users/examples/modify-baseline-to-pin-old-boost
  # resolve versioning https://www.appsloveworld.com/cplus/100/197/cmake-new-boost-version-may-have-incorrect-or-missing-dependencies-and-imported
  # get builtin-baseline for vcpkg.json
  # https://learn.microsoft.com/en-us/vcpkg/users/examples/versioning.getting-started#builtin-baseline
  # (cd ${VCPKG} && git rev-parse HEAD)
}
