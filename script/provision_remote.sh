#!/bin/bash

remote_setup() {
  REMOTE=sq089ahy@c220g5-111332.wisc.cloudlab.us

  # setup project permission running as root
  ssh $REMOTE
  {
    sudo su -
    if ! uname -a | grep -q "microsoft"; then
      chmod -R 777 .
    else
      echo "NOT REMOTE !"
    fi
  }

  # copy over workload files over and run tests
  source ./script/setenv.sh
  scp -rC ./script ./target/release/* $REMOTE:$PROJECT

  # provision remote machine
  ssh $REMOTE
  {
    sudo su -
    # (source ./<script_filename>.sh && <function_name>)
    (source ./script/provision_remote.sh && provision)
  }

}

provision() {
  # must be root $`sudo su -`

  # Disable ASLR https://linux-audit.com/linux-aslr-and-kernelrandomize_va_space-setting/
  echo 0 >/proc/sys/kernel/randomize_va_space

  ##### INSTALL dependencies
  pushd .

  # install dependencies

  popd
}

# run with $` (source ./script/provision_remote.sh && multiple_remote_upload) `
multiple_remote_upload() {
  source ./script/setenv.sh
  USERNAME=sq089ahy

  # Declare an array of string with type
  declare -a REMOTES=(
    "c220g2-010625"
    "c220g2-010624"
    # "c220g2-011121"
    # "c220g2-010623"
    # "c220g1-031120"
    # "c220g2-011126"
  )

  # Iterate the string array using for loop
  for i in ${REMOTES[@]}; do
    scp -rC ./script ./target/release/* $USERNAME@$i.wisc.cloudlab.us:$PROJECT
  done
}
