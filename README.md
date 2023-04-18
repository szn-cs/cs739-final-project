# Distributed Replicated Database using Paxos [cs739-p2-replicated-database]

- An implementation of a replicated in-memory KV-store using Multi-Paxos consensus algorithm. 

# Architecture Design

The logic of the code is divided into database & consensus portions. Each encompasses the algorithm implementations of the KV-Store and the consensus funcitons, respectively. In addition to the RPC functionality of the database endpoint exposed to the user and the consensus endpoint used to communicate amongst nodes.

![architecture](./documentation/Design%20Architecture.v4.jpg)

## Code details:
static member datastructures are instrantiated once and used to manage the node's state (similar to how  singleton pattern works).  
- **Consensus class**: holds the consensus functionality and handles the command logs datastructure. 
- **Database class**: implements an in-memory key-value store. 
- **ConsensusRPC**: for the consensus endpoint, defines the typical RPC implementations for Paxos, such as propose, accept, and success. In addition to RPC functions used for testing purposes.
- **DatabaseRPC**: defines the user-exposed endpoint for interactive with the replicated database service. Accepts requests such as get & set (alternatively used for delete operation). In addition to debugging RPC function, such as `get_db` to retrieve a database snapshot.
- **RPCWrapperCall functions**: these intend to abstract the calls to the gRPC (which include setting up protobuf structures) and provide a clean programming interface without dealing with the generated stubs directly.

_Note: as the deadline for the assignment approached, the modularity boundaries were violated, thus the code requires a round of refactoring to adhere to the original intention of the above modules._

# Build process & Setup instrucitons: 
For simplicity, the project generates a single binary file which can run serving the role of a cluster **node** or it can be run in **user** mode for testing the distributed service. All required repository setup scripts are included under `./script/*.sh`. 

- _Tools used in the project:_ `CMake` build tool, `VCpkg` package manger for C++, Git submodules to install alternative dependencies.
- gRPC library & it's dependencies are installed locally using the package manager. (initial installation may take 15 mins). 
- External libraries used include: `variadic table` (submodule), `Boost` library (such as programs_options component), `Termcolor`, `Google/Benchmark`, etc.

## How to use: 

- execute `./script/provision-local.sh` to setup repo and install dependencies
- run script `build.sh` → binary files in `./target`
- check `run.sh` for running the program

_to run funcitons in script files (without copy pasting): `$ (source ./script/<scriptname>.sh && <functioname>)`_


# Algorithm Details: 

- [Our Paxos algorithm approach](./documentation/paxos-consensus.md)

## Resources & Research papers: 
- EPaxos 
  - <https://lamport.azurewebsites.net/pubs/paxos-simple.pdf>
  - <https://www.usenix.org/system/files/nsdi21-tollman.pdf>
  - [Piazza] Proving that Paxos implementation works correctly: <https://harmony.cs.cornell.edu/docs/textbook/paxos/>
  - [Piazza] List of tests for distributed system: <https://asatarin.github.io/testing-distributed-systems/>
  - A good summary of the protocol with some psuedo code: <https://martinfowler.com/articles/patterns-of-distributed-systems/paxos.html/>
  - [example C++] <https://github.com/jiahanzhu/PaxosKVStore>
  - [example Go] <https://github.com/kkdai/paxos> <https://github.com/xiang90/paxos> <https://github.com/kr/paxos> 
  - Jepsen consistency testing for distributed system (e.g. check linearizability)
  - <https://github.com/dgryski/awesome-consensus>
  - [example] <https://www.youtube.com/watch?v=odZ2znr7D1o>


# Demonstration of consistency: 
![demonstration](./documentation/demostration-consistency.gif)
