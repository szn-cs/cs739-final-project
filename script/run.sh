#!/bin/bash
# all command must be run on `Cloudlab.us` with `sudo su -`
# on an isntance of UBUNTU 20.04 with Temporary Filesystem Size of 40 GB mounted to /root
# copy over binaries from ./target/release

usage_example1() {
  ./target/app --help
  ./target/test --help
  # terminals for cluster nodes:
  {
    ./target/app -g --port 8001 --consensus.endpoint localhost:9001 --consensus.server-id 9001 --config ./3_node_cluster.ini
    # equivalent to:
    ./target/app -g --port 8001 --consensus.endpoint localhost:9001 --consensus.server-id 9001 --cluster.address localhost:9001 --cluster.address localhost:9002
  }
  {
    ./target/app -g --port 8002 --consensus.endpoint localhost:9002 --consensus.server-id 9002 
  }
  {
    ./target/app -g --port 8003 --consensus.endpoint localhost:9003 --consensus.server-id 9003 
  }

  # terminal for user/client side
  {
    ./target/test -g --mode interactive --config 3_chubby.ini
    # or
    ./target/test -g --mode interactive --cluster.address 8000
    # or
    ./target/test -g --mode test --command test_maintain_session --config 3_chubby.ini
    # or 
    ./target/test -g -m test -c test_start_session -a 127.0.1.1:8001 -a 127.0.1.1:8002 -a 127.0.1.1:8003
    # or
    ./target/test -g --mode benchmark
  }
}

usage_example2() {
  source ./script/setenv.sh
  # SERVER_ADDRESS=c220g5-110912.wisc.cloudlab.us:50051

  ./target/app -g --port 8000 &
  ./target/app -g --port 8001 &
  ./target/app -g --port 8002 &
  ./target/app -g --port 8003 &
  ./target/app -g --port 8004 &

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
  ./target/app -g --port 8000 &
  ./target/app -g --port 8000 &
  ./target/app -g --port 8001 &
  ./target/app -g --port 8002 &
  ./target/app -g --port 8003 &
  ./target/app -g --port 8004 --flag.leader &

  ./target/test --command set --key k1 --value v1 --target 127.0.1.1:8002
  ./target/test --command set --key k2 --value v2 --target 127.0.1.1:8004
  ./target/test --command get --key k1 --target 127.0.1.1:8000
}

test_leader_functionality() {
  ./target/app -g --port 8000 --flag.leader &

  ./target/test --command test_1

  fuser -k 8000/tcp
}

test_consistency_no_failure() {
  CONFIG=5_node_cluster.ini
  NUMBER=5

  for i in {0..$((${NUMBER} - 1))}; do
    ones=$(($i % 10))
    tens=$((${i} / 10 % 10))
    port_suffix="${tens}${ones}"

    if [[ $port_suffix == "00" ]]; then
      ./target/app -g --config ${CONFIG} --port 80${port_suffix} --flag.leader &
    else
      ./target/app -g --config ${CONFIG} --port 80${port_suffix} &
    fi
  done

  #### separate stage

  ./target/test -g --command test_1 --config ${CONFIG}
}

test_benchmark() {
  # setup different cluster sizes

  FILE=15_node.csv
  # FOLDER=benchmark_set_leader
  # ./target/test --mode benchmark --benchmark_out=./results/${FOLDER}/${FILE} --benchmark_out_format=csv
  ./target/test --mode benchmark --benchmark_out=./results/${FILE} --benchmark_out_format=csv
}

#  (source ./script/run.sh && terminate_process)
terminate_process() {
  # cleanup
  (kill $(ps aux | grep '[./]target/app' | awk '{print $2}') >/dev/null 2>&1)
  kill $(jobs -p) >/dev/null 2>&1
}
