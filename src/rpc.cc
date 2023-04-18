#include "./declaration.h"

namespace rpc {

  Status ConsensusRPC::propose(ServerContext* context, const consensus_interface::Request* request, consensus_interface::Response* response) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPC";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::incoming>(n);
    }

    // app::Consensus consensus = *app::Consensus::instance;

    if (context->IsCancelled()) {
      return Status(grpc::StatusCode::CANCELLED, "Deadline exceeded or Client cancelled, abandoning.");
    }

    string key = request->key();
    int round = request->round();
    string value = request->value();
    int p_server = request->pserver_id();

    if (app::Cluster::config->flag.debug) {
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << cyan << "Proposal is for key " << key << reset << std::endl;
    }

    app::Consensus::instance->Set_Log(key, round);  // add log entry when acceptor receives a proposal

    consensus_interface::LogEntry pax_log = app::Consensus::instance->Get_Log(key, round);

    // Check if the current proposal round is greater than the previously seen round for the given key

    // int largestRoundSoFar = INT_MIN; // initialize to the smallest possible int value
    // for (const auto& entry : pax_log[key]) {
    //   largestRoundSoFar = max(largestRoundSoFar, entry.first);
    // }

    // if (largestRoundSoFar > round) {
    //   response->set_status(Status_Types::FAILED);
    //   return Status::OK;
    // }

    if (pax_log.p_server_id() > p_server) {
      return Status(grpc::StatusCode::ABORTED, "Proposal is out of date.");
    }

    // // see if there exists an accepted value for the given key and round

    if (pax_log.a_server_id() > 0) {
      response->set_aserver_id(pax_log.a_server_id());
      response->set_op(pax_log.op());
      response->set_value(pax_log.accepted_value());

      return Status::OK;
    }

    // pair<string, int> already_seen_key = consensus.Find_Max_Proposal(key, round);

    // if (already_seen_key.first != "") {  // the value is not nil ? then propose the same value
    //   round = already_seen_key.second + 1;
    //   value = already_seen_key.first;
    // }

    // else , continue with the current one

    response->set_round(round);
    response->set_pserver_id(p_server);

    return Status::OK;
  }

  Status ConsensusRPC::accept(ServerContext* context, const consensus_interface::Request* request, consensus_interface::Response* response) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPC";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::incoming>(n);
    }

    if (context->IsCancelled()) {
      return Status(grpc::StatusCode::CANCELLED, "Deadline exceeded or Client cancelled, abandoning.");
    }

    // Extract the key and proposal no. from the request
    string key = request->key();
    int round = request->round();

    // Extract the proposal server ID, operation, and value from the request
    int pserver_id = request->pserver_id();
    int aserver_id = request->aserver_id();
    consensus_interface::Operation op = request->op();
    string value = request->value();

    if (app::Cluster::config->flag.debug) {
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << cyan << "Acceptance is for key " << key << " and value " << value << reset << std::endl;
    }

    consensus_interface::LogEntry pax_log = app::Consensus::instance->Get_Log(key, round);

    // if (pax_log.p_server_id() > pserver_id) {
    //   if(app::Cluster::config->flag.debug){
    //     std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset <<
    //       red << "Denied Acceptance. Log p_server_id is " << pax_log.p_server_id()
    //       << " while the new p_server_id is " << pserver_id << reset << std::endl;
    //   }
    //   return Status(grpc::StatusCode::ABORTED, "Accept request is out of date.");
    // }

    response->set_op(op);
    response->set_value(value);
    response->set_pserver_id(pserver_id);
    response->set_aserver_id(aserver_id);
    response->set_round(round);

    app::Consensus::instance->Set_Log(key, round, pserver_id, op, value);

    return Status::OK;
  }

  Status ConsensusRPC::success(ServerContext* context, const consensus_interface::Request* request, consensus_interface::Empty* response) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPC";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::incoming>(n);
    }

    if (context->IsCancelled()) {
      return Status(grpc::StatusCode::CANCELLED, "Deadline exceeded or Client cancelled, abandoning.");
    }

    std::string key = request->key();
    std::string value = request->value();
    int round = request->round();
    int pserver_id = request->pserver_id();
    consensus_interface::Operation op = request->op();

    app::Consensus::instance->Set_Log(key, round, pserver_id, op, value);

    if (app::Cluster::config->flag.debug) {
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << cyan << "Successful consensus, key " << key << " is now set to value " << value << reset << std::endl;
    }

    map<int, consensus_interface::LogEntry> pax_log = app::Consensus::instance->Get_Log(key);

    int largestRoundSoFar = INT_MIN;  // initialize to the smallest possible int value
    for (const auto& entry : pax_log) {
      largestRoundSoFar = max(largestRoundSoFar, entry.first);
    }

    if (largestRoundSoFar > round) {
      return Status(grpc::StatusCode::ABORTED, "Information is outdated.");
    }

    if (key == "leader") {
      app::Cluster::leader = value;
    } else {
      app::Database::instance->Set_KV(key, value);
    }

    return Status::OK;
  }

  Status ConsensusRPC::ping(ServerContext* context, const consensus_interface::Empty* request, consensus_interface::Empty* response) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPC";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::incoming>(n);
    }

    if (context->IsCancelled()) {
      return Status(grpc::StatusCode::CANCELLED, "Deadline exceeded or Client cancelled, abandoning.");
    }

    return Status::OK;
  }

  Status ConsensusRPC::db_address_request(ServerContext* context, const consensus_interface::Empty* request, consensus_interface::DbAddressResponse* response) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPC";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::incoming>(n);
    }

    if (context->IsCancelled()) {
      return Status(grpc::StatusCode::CANCELLED, "Deadline exceeded or Client cancelled, abandoning.");
    }

    // TODO: We may need this, or changes to the .ini file, to have variable port numbers for db
    return Status::OK;
  }

  Status ConsensusRPC::get_leader(ServerContext* context, const consensus_interface::Empty* request, consensus_interface::GetLeaderResponse* response) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPC";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::incoming>(n);
    }

    if (context->IsCancelled()) {
      return Status(grpc::StatusCode::CANCELLED, "Deadline exceeded or Client cancelled, abandoning.");
    }

    app::Consensus consensus = *app::Consensus::instance;

    std::string leader = consensus.GetLeader();
    response->set_leader(leader);
    return Status::OK;
  }

  Status ConsensusRPC::elect_leader(ServerContext* context, const consensus_interface::ElectLeaderRequest* request, consensus_interface::Empty* response) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPC";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::incoming>(n);
    }

    if (context->IsCancelled()) {
      return Status(grpc::StatusCode::CANCELLED, "Request timed out");
    }

    consensus_interface::Request r;
    r.set_value(request->value());
    r.set_key(request->key());
    r.set_op(consensus_interface::SET_LEADER);

    std::pair<Status, Response> resp = app::Consensus::instance->AttemptConsensus(r);

    if (resp.first.ok()) {
      app::Cluster::leader = resp.second.value();
      // TODO: Log entry
    }

    return resp.first;
  }

  Status ConsensusRPC::get_stats(ServerContext* context, const consensus_interface::Empty* request, consensus_interface::StatisticsResponse* response) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPC";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::incoming>(n);
    }

    if (context->IsCancelled()) {
      return Status(grpc::StatusCode::CANCELLED, "Request timed out");
    }

    // print stats
    app::Statistics::print<app::Statistics::rpc_type::incoming>();
    app::Statistics::print<app::Statistics::rpc_type::outgoing>();

    google::protobuf::Map<string, int>* incount_map = response->mutable_incount();
    incount_map->insert(app::Statistics::incount.begin(), app::Statistics::incount.end());
    google::protobuf::Map<string, int>* outcount_map = response->mutable_outcount();
    outcount_map->insert(app::Statistics::outcount.begin(), app::Statistics::outcount.end());

    return Status::OK;
  }
}  // namespace rpc

namespace rpc {

  Status DatabaseRPC::get(ServerContext* context, const database_interface::GetRequest* request, database_interface::GetResponse* response) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "DatabaseRPC";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::incoming>(n);
    }

    if (context->IsCancelled()) {
      return Status(grpc::StatusCode::CANCELLED, "Deadline exceeded or Client cancelled, abandoning.");
    }

    // Check if we are leader by asking consensus thread
    std::string addr = app::Cluster::leader;
    if (addr.empty()) {
      return Status(grpc::StatusCode::ABORTED, "No leader set");
    }

    if (addr == app::Cluster::config->getAddress<app::Service::Consensus>().toString()) {
      // We are leader, return local value
      // Pair to store <value, error_code>. Useful in determining whether a key simply has an empty value or if they key does not exist
      // in the database, allowing us to return an error in the latter case
      std::pair<string, int> resp = app::Database::instance->Get_KV(request->key());
      if (app::Cluster::config->flag.debug) {
        std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << cyan << "We are leader." << reset << std::endl;
      }

      if (resp.second != 0) {
        response->set_value("");
        response->set_error(1);
        if (app::Cluster::config->flag.debug) {
          std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << cyan << "There was no value tied to key " << request->key() << reset << std::endl;
        }
      } else {
        if (app::Cluster::config->flag.debug) {
          std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << cyan << "Returning value " << resp.first << reset << std::endl;
        }
        response->set_value(resp.first);
      }

    } else {
      // We are not leader, contact the db stub of the leader, corresponding to the result of asking the consensus thread for the leader's address

      if (app::Cluster::config->flag.debug) {
        std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << cyan << "Not leader, forwarding request to leader." << reset << std::endl;
      }

      grpc::ClientContext leader_context;
      auto deadline =
          std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
      leader_context.set_deadline(deadline);

      database_interface::GetRequest leader_request;
      database_interface::GetResponse leader_response;

      leader_request.set_key(request->key());

      Status leader_status = app::Cluster::memberList->at(app::Cluster::leader)->databaseEndpoint.stub->stub->get(&leader_context, leader_request, &leader_response);

      if (!leader_status.ok()) {
        // We must elect a new leader
        if (app::Cluster::config->flag.debug) {
          std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << cyan << "Leader unresponsive, finding new leader." << reset << std::endl;
        }

        Status election_status = app::Cluster::currentNode->consensusEndpoint.stub->trigger_election();
        if(!election_status.ok()){
          if (app::Cluster::config->flag.debug) {
            std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << cyan << "Election failed, likely due to too many nodes being down." << reset << std::endl;
          }
          return Status(grpc::StatusCode::ABORTED, "Could not find a suitable leader.");
        }
        grpc::ClientContext new_leader_context;
        auto deadline =
          std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
        new_leader_context.set_deadline(deadline);

        Status new_leader_status = app::Cluster::memberList->at(app::Cluster::leader)->databaseEndpoint.stub->stub->get(&new_leader_context, leader_request, &leader_response);

        if(!new_leader_status.ok()){
          return Status(grpc::StatusCode::ABORTED, "New leader was not able to get value.");
        }
        response->set_value(leader_response.value());
        response->set_error(leader_response.error());

      } else {
        if (app::Cluster::config->flag.debug) {
          std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << cyan << "Leader replied with value " << leader_response.value() << reset << std::endl;
        }
        response->set_value(leader_response.value());
        response->set_error(leader_response.error());
      }
    }

    return Status::OK;
  }

  Status DatabaseRPC::set(ServerContext* context, const database_interface::SetRequest* request, database_interface::Empty* response) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "DatabaseRPC";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::incoming>(n);
    }

    if (context->IsCancelled()) {
      return Status(grpc::StatusCode::CANCELLED, "Deadline exceeded or Client cancelled, abandoning.");
    }

    std::string addr = app::Cluster::leader;
    if (addr.empty()) {
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << red << "Error: No leader set" << reset << std::endl;
      return Status(grpc::StatusCode::ABORTED, "No leader set");
    }

    if (addr == app::Cluster::config->getAddress<app::Service::Consensus>().toString()) {
      // We are leader, trigger paxos algorithm
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << red
                << "We are leader, triggering paxos to set key " << request->key() << " to value " << request->value() << reset << std::endl;

      Request req;
      req.set_key(request->key());
      req.set_value(request->value());

      std::pair<Status, Response> resp = app::Consensus::instance->AttemptConsensus(req);

      if (resp.first.ok()) {
        if (app::Cluster::config->flag.debug) {
          std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << cyan << "Reached consensus: Key: " << request->key() << " Value: " << resp.second.value() << reset << std::endl;
        }
        // KV value is stored as a part of the inform() rpc
      }
      return resp.first;
    } else {
      // We are not the leader, forward request to leader
      if (app::Cluster::config->flag.debug) {
        std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << red << "Not leader, forwarding request to leader." << reset << std::endl;
      }

      grpc::ClientContext leader_context;
      auto deadline =
          std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
      leader_context.set_deadline(deadline);

      database_interface::SetRequest leader_request;
      database_interface::Empty leader_response;

      leader_request.set_key(request->key());
      leader_request.set_value(request->value());

      Status leader_status = app::Cluster::memberList->at(app::Cluster::leader)->databaseEndpoint.stub->stub->set(&leader_context, leader_request, &leader_response);

      if (!leader_status.ok()) {
        if (app::Cluster::config->flag.debug) {
          std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << cyan << "Leader unresponsive, finding new leader." << reset << std::endl;
        }

        Status election_status = app::Cluster::currentNode->consensusEndpoint.stub->trigger_election();
        if(!election_status.ok()){
          if (app::Cluster::config->flag.debug) {
            std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << cyan << "Election failed, likely due to too many nodes being down." << reset << std::endl;
          }
          return Status(grpc::StatusCode::ABORTED, "Could not find a suitable leader.");
        }
        grpc::ClientContext new_leader_context;
        auto deadline =
          std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
        new_leader_context.set_deadline(deadline);

        Status new_leader_status = app::Cluster::memberList->at(app::Cluster::leader)->databaseEndpoint.stub->stub->set(&new_leader_context, leader_request, &leader_response);

        if(!new_leader_status.ok()){
          return Status(grpc::StatusCode::ABORTED, "New leader was not able to get value.");
        }
      }
    }

    return Status::OK;
  }

  /**
   * @brief This method is just for testing, returns the full db key->value map
  */
  Status DatabaseRPC::get_db(grpc::ServerContext* context, const database_interface::Empty* request, database_interface::FullDBResponse* response) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "DatabaseRPC";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::incoming>(n);
    }
    //response.(app::Database::instance->Get_DB());
    std::map<string, string> kv = app::Database::instance->Get_DB();
    *response->mutable_db() = google::protobuf::Map<std::string, std::string>(kv.begin(), kv.end());
    return Status::OK;
  }
}  // namespace rpc

namespace rpc::call {
  /* Database RPC wrappers ------------------------------------------------------------- */

  google::protobuf::Map<string, string>
  DatabaseRPCWrapperCall::get_db() {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "DatabaseRPCWrapperCall";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::outgoing>(n);
    }

    grpc::ClientContext context;

    database_interface::Empty request;
    database_interface::FullDBResponse response;

    grpc::Status status = this->stub->get_db(&context, request, &response);

    return response.db();
  }

  std::string DatabaseRPCWrapperCall::get(const std::string& s, bool deadline) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "DatabaseRPCWrapperCall";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::outgoing>(n);
    }

    grpc::ClientContext context;
    if (deadline) {
      auto deadline =
          std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
      context.set_deadline(deadline);
    }

    database_interface::GetRequest request;
    database_interface::GetResponse response;

    request.set_key(s);

    grpc::Status status = this->stub->get(&context, request, &response);

    // if (status.error_code() == grpc::StatusCode::UNAVAILABLE ||
    //     status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
    //   // Elect new leader
    //   // Resend request to new leader
    // }

    return (status.ok()) ? response.value() : "RPC failed !";
  }

  Status DatabaseRPCWrapperCall::set(const std::string& key, const std::string& value, bool deadline) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "DatabaseRPCWrapperCall";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::outgoing>(n);
    }

    grpc::ClientContext context;

    if (deadline) {
      auto deadline =
          std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
      context.set_deadline(deadline);
    }
    database_interface::SetRequest request;
    database_interface::Empty response;

    request.set_key(key);
    request.set_value(value);

    grpc::Status status = this->stub->set(&context, request, &response);
    //std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << status.error_code() << " " << status.error_message() << reset << std::endl;

    // if (status.error_code() == grpc::StatusCode::UNAVAILABLE ||
    //     status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
    //   // Elect new leader
    //   // Resend request to new leader
    // }

    return status;
  }
}  // namespace rpc::call

namespace rpc::call {
  /* Consensus RPC wrappers ------------------------------------------------------------- */

  std::pair<Status, Response> ConsensusRPCWrapperCall::propose(const Request request) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPCWrapperCall";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::outgoing>(n);
    }

    ClientContext context;

    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
    context.set_deadline(deadline);
    Response response;

    grpc::Status status = this->stub->propose(&context, request, &response);

    std::pair<Status, Response> res;
    res.first = status;
    res.second = response;
    return res;
  }

  std::pair<Status, Response> ConsensusRPCWrapperCall::accept(const Request request) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPCWrapperCall";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::outgoing>(n);
    }

    ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
    context.set_deadline(deadline);
    Response response;

    grpc::Status status = this->stub->accept(&context, request, &response);

    std::pair<Status, Response> res;
    res.first = status;
    res.second = response;
    return res;
  }

  std::pair<Status, std::string> ConsensusRPCWrapperCall::db_address_request() {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPCWrapperCall";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::outgoing>(n);
    }

    ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
    context.set_deadline(deadline);

    consensus_interface::Empty request;
    consensus_interface::DbAddressResponse response;

    grpc::Status status = this->stub->db_address_request(&context, request, &response);

    std::pair<Status, std::string> a;
    a.first = status;
    a.second = response.addr();

    return a;
  }

  std::tuple<Status, std::map<std::string, int>, std::map<std::string, int>> ConsensusRPCWrapperCall::get_stats() {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPCWrapperCall";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::outgoing>(n);
    }

    ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
    context.set_deadline(deadline);

    consensus_interface::Empty request;
    consensus_interface::StatisticsResponse response;

    grpc::Status status = this->stub->get_stats(&context, request, &response);

    const google::protobuf::Map<std::string, int32_t> grpc_map_in = response.incount();
    const google::protobuf::Map<std::string, int32_t> grpc_map_out = response.outcount();

    std::map<string, int> standard_map_in(grpc_map_in.begin(), grpc_map_in.end());
    std::map<string, int> standard_map_out(grpc_map_out.begin(), grpc_map_out.end());

    std::tuple<Status, std::map<std::string, int>, std::map<std::string, int>> t = std::make_tuple(status, standard_map_in, standard_map_out);
    return t;
  }

  Status ConsensusRPCWrapperCall::success(const Request request) {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPCWrapperCall";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::outgoing>(n);
    }

    grpc::ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
    context.set_deadline(deadline);

    consensus_interface::Empty response;

    grpc::Status status = this->stub->success(&context, request, &response);

    return status;
  }

  Status ConsensusRPCWrapperCall::ping() {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPCWrapperCall";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::outgoing>(n);
    }

    grpc::ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(500);
    context.set_deadline(deadline);

    consensus_interface::Empty request;
    consensus_interface::Empty response;

    grpc::Status status = this->stub->ping(&context, request, &response);

    return status;
  }

  std::pair<Status, std::string> ConsensusRPCWrapperCall::get_leader() {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPCWrapperCall";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::outgoing>(n);
    }

    grpc::ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(app::Cluster::config->flag.timeout);
    context.set_deadline(deadline);

    consensus_interface::Empty request;
    consensus_interface::GetLeaderResponse response;

    grpc::Status status = this->stub->get_leader(&context, request, &response);

    std::pair<Status, std::string> res;
    res.first = status;
    res.second = response.leader();
    std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << "ConsensusRPCWrapperCall::get_leader returned " << response.leader() << reset << std::endl;
    return res;
  }

  Status ConsensusRPCWrapperCall::trigger_election() {
    if (app::Cluster::config->flag.debug) {
      const std::string className = "ConsensusRPCWrapperCall";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
      app::Statistics::increment<app::Statistics::rpc_type::outgoing>(n);
    }

    grpc::ClientContext context;
    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(5000);
    context.set_deadline(deadline);

    consensus_interface::ElectLeaderRequest request;
    consensus_interface::Empty response;

    request.set_key("leader");
    request.set_value(app::Cluster::config->getAddress<app::Service::Consensus>().toString());

    grpc::Status status = this->stub->elect_leader(&context, request, &response);

    return status;
  }

}  // namespace rpc::call
