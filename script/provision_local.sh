#!/bin/bash

function fix_vcpkg() {
  WARNING='\033[93m'
  BOLD='\033[1m'
  RESET="\e[0m"
  echo "FIX: fix_vcpkg called"
  # FIX issue with versioning in VCpkg with baseline reference in vcpkg.json
  VCPKG=./dependency/vcpkg

  rm -r $VCPKG
  pushd ./dependency
  git clone https://github.com/Microsoft/vcpkg.git
  popd

  # git restore dependency/vcpkg --recurse-submodules

  pushd $VCPKG
  echo "$BOLD$WARNING IMPORTANT: make sure 'vcpkg.json' has 'builtin-baseline' equal to:"
  git fetch
  git rev-parse HEAD
  echo $RESET
  popd
}

source ./script/setenv.sh
# Provision local developemnt - develop locally and send binaries to remote for testing

## setup & configure repository
workspaceFolder=$PWD
chmod +x ${workspaceFolder}/script/*

# download corresponding submodules
git restore dependency/vcpkg --recurse-submodules
git submodule update --init --remote
git submodule update --init --recursive

## provision system dependencies
DEPENDENCIES=("build-essential" "autoconf" "libtool" "pkg-config" "gcc" "cmake")

sudo apt -y update && sudo apt -y upgrade
for i in ${DEPENDENCIES}; do
  echo "EXECUTE: \`sudo apt install -y $i"
  sudo apt install -y $i
done
sudo apt update -y && sudo apt -y upgrade
sudo apt autoremove

## install vcpkg package manager and dependencies https://github.com/grpc/grpc/tree/master/src/cpp#install-using-vcpkg-package

# fix_vcpkg

pushd ./dependency/vcpkg
./bootstrap-vcpkg.sh -disableMetrics && ./vcpkg integrate install # >./CMake-script-for-vcpkg.txt
popd
# read vcpkg.json from root directory and install dependencies (vcpkg manifest mode)
./dependency/vcpkg/vcpkg install --debug

function cmake() {
  # install latest CMake
  # https://askubuntu.com/questions/355565/how-do-i-install-the-latest-version-of-cmake-from-the-command-line
}

function fedora() {
  ## Fedora gcc installation
  # dnf groupinstall 'Development Tools'
  # yum install gcc-c++
  # yum install perl-IPC-Cmd
  # solve issue: https://github.com/microsoft/vcpkg/issues/12061
  # fedora installation https://techviewleo.com/install-vcpkg-c-library-manager-on-linux-macos-windows/
  # yum install -y perl-CPAN
}

function mac() {
  su -

  brew update
  brew upgrade
  brew info gcc
  brew install gcc
  brew cleanup

  port install cmake
}
