#include "./declaration.h"

namespace app {

  template class Endpoint<rpc::call::ConsensusRPCWrapperCall>;  // explicit initiation - prevent linker errors for separate dec & def of template
  template class Endpoint<rpc::call::DatabaseRPCWrapperCall>;   // explicit initiation - prevent linker errors for separate dec & def of template

  std::shared_ptr<std::map<std::string, std::shared_ptr<Node>>> Cluster::memberList = nullptr;
  std::shared_ptr<utility::parse::Config> Cluster::config = nullptr;
  std::shared_ptr<Node> Cluster::currentNode = nullptr;
  std::string Cluster::leader;
  pthread_mutex_t Cluster::leader_mutex;
  std::map<string, int> Statistics::incount;
  std::map<string, int> Statistics::outcount;

  void initializeStaticInstance(std::vector<std::string> addressList, std::shared_ptr<utility::parse::Config> config) {
    Cluster::config = config;

    Cluster::memberList = std::make_shared<std::map<std::string, std::shared_ptr<Node>>>();

    // Transform each config into a address via make_address, inserting each object into the vector.
    std::vector<utility::parse::Address> l;
    std::transform(addressList.begin(), addressList.end(), std::back_inserter(l), utility::parse::make_address);
    for (utility::parse::Address a : l) {
      // NOTE: Statically assuming db port will be 1000 above its consensus port
      std::string db_addr = a.address + ":" + std::to_string(a.port + 1000);
      // if(Cluster::config->flag.debug){
      //   std::cout << "consensus address: " << a.toString() << " database address: " << db_addr << reset << std::endl;
      // }

      Cluster::memberList->insert(std::make_pair(a.toString(), std::make_shared<Node>(a.toString(), db_addr)));
    }

    // current node's details
    utility::parse::Address addressConsensus = Cluster::config->flag.local_ubuntu ?  // If local ubuntu
                                                   utility::parse::make_address("127.0.1.1:" + std::to_string(Cluster::config->port_consensus))
                                                                                  :                         // Ip is 127.0.1.1
                                                   Cluster::config->getAddress<app::Service::Consensus>();  // Ip could be something else, found through getAddress()

    utility::parse::Address addressDatabase = Cluster::config->flag.local_ubuntu ?  // If local ubuntu
                                                  utility::parse::make_address("127.0.1.1:" + std::to_string(Cluster::config->port_database))
                                                                                 :                        // Ip is 127.0.1.1
                                                  Cluster::config->getAddress<app::Service::Database>();  // Ip could be something else, found through getAddress()

    // addressConsensus = Cluster::config->getAddress<app::Service::Consensus>();  // addresses as IDs follow the consensus port
    // addressDatabase = Cluster::config->getAddress<app::Service::Database>();    // addresses as IDs follow the consensus port

    auto iterator = Cluster::memberList->find(addressConsensus.toString());
    if (iterator == Cluster::memberList->end()) {  // not found
      Cluster::currentNode = std::make_shared<Node>(addressConsensus, addressDatabase);
      Cluster::memberList->insert(std::make_pair(addressConsensus.toString(), Cluster::currentNode));
    } else {
      Cluster::currentNode = iterator->second;
    }

    Cluster::leader = "";

    pthread_mutex_init(&(Cluster::leader_mutex), NULL);

    // instance initialization taken care of in the class definition.
    // Consensus::instance = std::make_shared<Consensus>();
    // Database::instance = std::make_shared<Database>();

    if (Cluster::config->flag.debug)
      std::cout << "There are " << Cluster::memberList->size() << " replicas." << reset << std::endl;
  }

}  // namespace app

namespace app {

  // initialized in class instead.
  // std::shared_ptr<Consensus> Consensus::instance = nullptr;

  void Consensus::coordinate() {
    Status status = Status::OK;
    std::set<std::string> leaders;
    std::set<std::string> live_replicas;

    if (Cluster::config->flag.leader) {
      // Can use self to indicate if this replica is a leader, an address otherwise
      Cluster::leader = Cluster::config->getAddress<app::Service::Consensus>().toString();

      if (Cluster::config->flag.debug)
        std::cout << termcolor::blue << "I am the leader, address " << Cluster::config->getAddress<app::Service::Consensus>().toString() << reset << endl;
    } else {
      std::mt19937_64 eng{std::random_device{}()};    //  seed randomly
      std::uniform_int_distribution<> dist{1, 5000};  // 1 ms to 5 seconds
      std::this_thread::sleep_for(std::chrono::milliseconds{dist(eng)});

      // Must send get_coordinator requests to other stubs
      // Because this is a call not going explicitly to leader, need to track which
      // nodes are live
      if (Cluster::config->flag.debug)
        std::cout << termcolor::blue << "I am not the leader" << reset << endl;

      for (const auto& [key, node] : *(Cluster::memberList)) {
        if (Cluster::config->flag.debug) {
          std::cout << termcolor::blue << "get_leader() request to " << key << reset << endl;
        }
        std::pair<Status, std::string> res = node->consensusEndpoint.stub->get_leader();
        if (res.first.ok()) {  // Replica is up
          live_replicas.insert(key);
          if (!res.second.empty())  // Replica knows the current leader
            leaders.insert(res.second);
        }
      }

      // leaders.size() will be 1 or 0, unless consensus issues
      // If 0, trigger an election
      // Check if leader is alive, if not return a non ok status to trigger an election
      if (leaders.size() == 1 && live_replicas.find(*leaders.begin()) != live_replicas.end()) {
        if (Cluster::config->flag.debug)
          std::cout << termcolor::blue << "Valid leader returned." << reset << endl;

        pthread_mutex_lock(&(Cluster::leader_mutex));
        Cluster::leader = *leaders.begin();
        pthread_mutex_unlock(&(Cluster::leader_mutex));
      } else if (Cluster::config->flag.election) {
        // Send an election request to ourself
        if (Cluster::config->flag.debug)
          std::cout << termcolor::blue << "No valid leader returned by any server, starting election. " << leaders.size() << " leaders were suggested." << reset << endl;

        Status election_status = Cluster::currentNode->consensusEndpoint.stub->trigger_election();
        if (!election_status.ok())
          status = Status(grpc::StatusCode::ABORTED, "we could not establish a leader, not enough nodes.");
      }

      if (!status.ok()) {
        std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << red
                  << "Unable to initialize node because " << status.error_message() << reset << std::endl;
        return;
      }
    }

    // Recover the db from the leader, unless we are leader
    if (Cluster::leader.empty()) {
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << red
                << "Somehow no leader is set." << reset << std::endl;
      return;
    }

    if (Cluster::leader != Cluster::config->getAddress<app::Service::Consensus>().toString()) {
      google::protobuf::Map<string, string> leader_db = Cluster::memberList->at(Cluster::leader)->databaseEndpoint.stub->get_db();
      if (leader_db.empty()) {
        std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << red
                  << "Recovery map sent by leader is empty. Please determine if this is the expected behavior." << reset << std::endl;
      }
      for (const auto& [key, value] : leader_db) {
        app::Database::instance->Set_KV(key, value);
      }

      if (Cluster::config->flag.debug) {
        std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow
                  << "Recovery succeeded." << reset << std::endl;
      }
    }

    return;
  }

  void Consensus::broadcastPeriodicPing() {
    // sleep(5);
    // for (auto& [key, node] : Cluster::memberList) {
    // node.consensusEndpoint.stub->ping("message");
    // }
  }

  // Methods for adding to log at different points during paxos algorithm
  // when Acceptor receives a proposal for particular key and round
  void Consensus::Set_Log(const string& key, int round) {
    pthread_mutex_lock(&log_mutex);
    pax_log[key][round];
    pthread_mutex_unlock(&log_mutex);
  }

  // update promise id
  void Consensus::Set_Log(const string& key, int round, int p_server) {
    pthread_mutex_lock(&log_mutex);
    pax_log[key][round].set_p_server_id(p_server);
    pthread_mutex_unlock(&log_mutex);
  }

  // Update the acceptance info for the given key and round.
  void Consensus::Set_Log(const string& key, int round, int a_server, consensus_interface::Operation op, const string& value) {
    pthread_mutex_lock(&log_mutex);
    pax_log[key][round].set_a_server_id(a_server);
    pax_log[key][round].set_op(op);
    pax_log[key][round].set_accepted_value(value);
    pthread_mutex_unlock(&log_mutex);
  }

  map<string, map<int, consensus_interface::LogEntry>> Consensus::Get_Log() {
    //pthread_mutex_lock(&log_mutex);
    return pax_log;
    //pthread_mutex_unlock(&log_mutex);
  }

  map<int, consensus_interface::LogEntry> Consensus::Get_Log(const string& key) {
    //pthread_mutex_lock(&log_mutex);
    return pax_log[key];
    //pthread_mutex_unlock(&log_mutex);
  }

  consensus_interface::LogEntry Consensus::Get_Log(const string& key, int round) {
    //pthread_mutex_lock(&log_mutex);
    return pax_log[key][round];
    //pthread_mutex_unlock(&log_mutex);
  }

  // Find the highest round number seen for the given key
  pair<string, int> Consensus::Find_Max_Proposal(const string& key, int round) {
    map<string, map<int, consensus_interface::LogEntry>> pax_log = Consensus::Get_Log();

    //pthread_mutex_lock(&log_mutex);
    int max_proposal = round;
    for (const auto& entry : pax_log[key]) {
      if (entry.first > max_proposal) {
        max_proposal = entry.first;
      }
    }
    //pthread_mutex_unlock(&log_mutex);

    if (max_proposal == round)
      return make_pair("", 0);

    return make_pair(pax_log[key][max_proposal].accepted_value(), max_proposal);
  }

  string Consensus::readFromDisk(string path) {
    std::ifstream file(path);
    std::string value;
    std::getline(file, value);

    return value;
  }

  void Consensus::writeToDisk(string path, string value) {
    ofstream file2(path, std::ios::trunc);  // open the file for writing, truncate existing content
    file2 << value;                         // write the new content to the file
    file2.close();
  }

  std::string Consensus::GetLeader() {
    //pthread_mutex_lock(&Cluster::leader_mutex);
    //std::string leader = Cluster::leader;
    //pthread_mutex_unlock(&Cluster::leader_mutex);

    return Cluster::leader;
    ;
  }

  // std::string Consensus::SetLeader() {

  //   // Threadsafe read of leader address
  //   //pthread_mutex_lock(&Cluster::leader_mutex);
  //   //std::string leader = Cluster::leader;
  //   //pthread_mutex_unlock(&Cluster::leader_mutex);

  //   return Cluster::leader;
  // }

  std::pair<Status, Response> Consensus::AttemptConsensus(consensus_interface::Request r) {
    std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset
              << yellow << "Consensus::AttemptConsensus for key " << r.key() << " value: " << r.value() << reset << std::endl;

    int num_replicas = Cluster::memberList->size();

    std::string key = r.key();
    std::string value = r.value();

    std::pair<Status, Response> default_response;
    default_response.first = Status(grpc::StatusCode::ABORTED, "Not enough live servers for quorum.");

    // Get current round
    map<string, map<int, consensus_interface::LogEntry>> pax_log = Consensus::Get_Log();
    int round = pax_log[key].size();

    // Set proposal id to 1, first proposal we are making from this replica
    int propose_id = 1;

    // Initialize variables to hold info about if there is already an accepted value
    int num_accepted_proposals = 0;
    int accepted_id = 0;
    std::string accepted_value;
    consensus_interface::Operation accepted_op = consensus_interface::Operation::NOT_SET;

    std::vector<std::shared_ptr<Node>> live_nodes;

    // TODO: We may be able to implement the below logic to decrease latency
    // Only need to do the ping and propose rounds if this is an election. Otherwise, because there is only one proposer,
    // we can skip those steps and just send accept requests, and if there is a quorum, that will suffice

    // Ping servers, figure out who's alive

    for (const auto& [key, node] : *(Cluster::memberList)) {
      Status res = node->consensusEndpoint.stub->ping();
      if (res.ok()) {
        live_nodes.push_back(node);
      }
    }

    // Do we have enough for quorum?
    int num_live_acceptors = live_nodes.size();
    if (num_live_acceptors <= std::ceil(num_replicas / 2.0) - 1) {
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << cyan
                << "Ping state: Not enough live servers for quorum, could only find " << num_live_acceptors
                << " replicas (including myself) out of " << num_replicas << " (including myself)." << reset << std::endl;
      return default_response;
    }

    // 1. Entering the proposal stage

    // Preparing the request
    Request prepare_request;
    prepare_request.set_key(key);
    prepare_request.set_round(round);
    prepare_request.set_value(value);
    prepare_request.set_pserver_id(propose_id);

    // Sending the request to all nodes, tracking if a value has already been accepted for our key

    for (const auto& node : live_nodes) {
      std::pair<Status, Response> response = node->consensusEndpoint.stub->propose(prepare_request);
      if (response.first.ok()) {
        num_accepted_proposals++;
        if (response.second.aserver_id() > accepted_id) {
          accepted_id = response.second.aserver_id();
          accepted_value = response.second.value();
          accepted_op = response.second.op();
        }
      }
    }

    if (Cluster::config->flag.debug) {
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow
                << "Proposal phase done. " << num_accepted_proposals << " accept, "
                << num_live_acceptors - num_accepted_proposals << " reject." << reset << std::endl;
    }

    // Check if we still have a quorum
    if (num_accepted_proposals <= std::ceil(num_replicas / 2.0) - 1) {
      // A node must have died between our first ping and here
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << red << "Propose state: Not enough live servers for quorum." << reset << std::endl;
      return default_response;
    }

    // 2. Entering the accept stage

    int num_final_acceptances = 0;

    // Constructing the new Request
    Request accept_request;
    accept_request.set_key(key);
    accept_request.set_round(round);
    accept_request.set_pserver_id(propose_id);
    if (accepted_id > 0) {
      accept_request.set_op(accepted_op);
      accept_request.set_value(accepted_value);
      accept_request.set_aserver_id(accepted_id);
    } else {
      accept_request.set_op(r.op());
      accept_request.set_value(r.value());
    }

    // For each node, send an accept request
    Response acceptance;
    for (const auto& node : live_nodes) {
      std::pair<Status, Response> response = node->consensusEndpoint.stub->accept(accept_request);
      if (response.first.ok()) {
        num_final_acceptances++;
        acceptance = response.second;
      } else {
        if (Cluster::config->flag.debug) {
          std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << red << "Node" << key << " denied our request, error code " << response.first.error_code()
                    << " with message " << response.first.error_message() << reset << std::endl;
        }
      }
    }

    if (Cluster::config->flag.debug) {
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow
                << "Acceptance phase done. " << num_final_acceptances << " accept, "
                << live_nodes.size() - num_final_acceptances << " reject." << reset << std::endl;
    }

    // Check that we still have quorum
    if (num_final_acceptances <= std::ceil(num_replicas / 2.0) - 1) {
      // A node must have died between our first ping and here
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << red << "Accept state: Not enough live servers for quorum." << reset << std::endl;
      return default_response;
    }

    // 3: Entering the success stage, informing other nodes of the new commit

    // Setting up the request
    Request success_request;
    success_request.set_key(key);
    success_request.set_value(acceptance.value());
    success_request.set_op(acceptance.op());
    success_request.set_pserver_id(acceptance.pserver_id());
    success_request.set_aserver_id(acceptance.aserver_id());
    success_request.set_round(acceptance.round());

    // Loop through the nodes, informing them of the committed change to the db
    for (const auto& [key, node] : *(Cluster::memberList)) {
      Status response = node->consensusEndpoint.stub->success(success_request);
      // It doesn't matter if this doesn't go through, the only time it wouldn't is if a server is down, which
      // would mean they have to go through recovery anyway.
    }

    if (Cluster::config->flag.debug) {
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow
                << "Success phase done. Accepted value: " << acceptance.value() << reset << std::endl;
    }

    std::pair<Status, Response> resp;
    resp.first = Status::OK;
    resp.second = acceptance;

    return resp;
  }

}  // namespace app

namespace app {

  // initialized in class instead
  // std::shared_ptr<Database> Database::instance = nullptr;

  /**
   * Internal database KV methods
   * - not accessible by users only quorum participants/nodes.
   */
  std::pair<std::string, int> Database::Get_KV(const string& key) {
    // Returning a pair to indicate if the element does not exist vs if its simply an empty string in the db
    std::pair<std::string, int> res;
    // int ret = pthread_mutex_lock(&data_mutex);
    // if (ret != 0) {
    //   if (ret == EINVAL) {
    //     pthread_mutex_init(&data_mutex, NULL);
    //   }
    // }
    auto i = kv_store.find(key);
    if (i == kv_store.end()) {
      res.first = "";
      res.second = 1;
      return res;
    }
    // pthread_mutex_unlock(&data_mutex);
    res.first = i->second;
    res.second = 0;
    return res;
  }

  void Database::Set_KV(const string& key, const string& value) {
    int ret = pthread_mutex_lock(&data_mutex);
    if (ret != 0) {
      if (ret == EINVAL) {
        pthread_mutex_init(&data_mutex, NULL);
      }
    }
    kv_store[key] = value;
    pthread_mutex_unlock(&data_mutex);
  }

  void Database::Delete_KV(const string& key) {
    int ret = pthread_mutex_lock(&data_mutex);
    if (ret != 0) {
      if (ret == EINVAL) {
        pthread_mutex_init(&data_mutex, NULL);
      }
    }
    kv_store.erase(key);
    pthread_mutex_unlock(&data_mutex);
  }

  map<string, string> Database::Get_DB() {
    return kv_store;
  }

}  // namespace app