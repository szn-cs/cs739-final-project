#!/bin/bash
# all command must be run on `Cloudlab.us` with `sudo su -`
# on an isntance of UBUNTU 20.04 with Temporary Filesystem Size of 40 GB mounted to /root
# copy over binaries from ./target/release

test() {
  ./target/app --help
  ./target/app --mode=user --help

  # terminal 1
  {
    ./target/app --mode node
  }
  # terminal 2
  {
    ./target/app --mode user
  }
}

test_example() {
  source ./script/setenv.sh
  # SERVER_ADDRESS=c220g5-110912.wisc.cloudlab.us:50051

  ./target/app -g --port_consensus 8000 &
  ./target/app -g --port_consensus 8001 &
  ./target/app -g --port_consensus 8002 &
  ./target/app -g --port_consensus 8003 &
  ./target/app -g --port_consensus 8004 --flag.leader &

  # ./server $SERVER -serverAddress=$SERVER_ADDRESS >/dev/null 2>&1 &

  # Stop all background jobs
  # kill $(jobs -p)

  # Bring background to foreground
  # jobs
  # fg
  # bg
  # fg %1

  # kill all processes hanging from a closed terminal
  kill $(ps aux | grep '[./]target/app' | awk '{print $2}')
}

test_rpc() {
  ./target/app -g --port_consensus 8000 --port_database 9000 &
  ./target/app -g --port_consensus 8001 --port_database 9001 &
  ./target/app -g --port_consensus 8002 --port_database 9002 &
  ./target/app -g --port_consensus 8003 --port_database 9003 &
  ./target/app -g --port_consensus 8004 --port_database 9004 --flag.leader &

  ./target/app --mode user --command set --key k1 --value v1 --target 127.0.1.1:8002
  ./target/app --mode user --command set --key k2 --value v2 --target 127.0.1.1:8004
  ./target/app --mode user --command get --key k1 --target 127.0.1.1:8000
}

test_leader_functionality() {
  ./target/app -g --port_consensus 8000 --port_database 9000 --flag.leader &

  ./target/app --mode user --command test_leader

  fuser -k 8000/tcp
  fuser -k 9000/tcp
}

test_consistency_no_failure() {
  CONFIG=15_node_cluster.ini
  NUMBER=15

  for i in {0..$((${NUMBER} - 1))}; do
    ones=$(($i % 10))
    tens=$((${i} / 10 % 10))
    port_suffix="${tens}${ones}"

    if [[ $port_suffix == "00" ]]; then
      # ./target/app --config ${CONFIG} --port_consensus 80${port_suffix} --port_database 90${port_suffix} --flag.leader &
      ./target/app -g --config ${CONFIG} --port_consensus 80${port_suffix} --port_database 90${port_suffix} &
    else
      ./target/app --config ${CONFIG} --port_consensus 80${port_suffix} --port_database 90${port_suffix} &
    fi
  done

  #### separate stage

  ./target/app -g -m user --command test_1000_random_ops --config ${CONFIG}
  ./target/app -g -m user --command test_count --config ${CONFIG}

}

test_benchmark() {
  # setup different cluster sizes

  FILE=15_node.csv
  FOLDER=benchmark_set_leader
  ./target/benchmark --benchmark_out=./results/${FOLDER}/${FILE} --benchmark_out_format=csv
}

#  (source ./script/run.sh && terminate_process)
terminate_process() {
  # cleanup
  (kill $(ps aux | grep '[./]target/app' | awk '{print $2}') >/dev/null 2>&1)
  kill $(jobs -p) >/dev/null 2>&1
}
