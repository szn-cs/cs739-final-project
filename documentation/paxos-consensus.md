# Multi-Paxos: distributes a log with enteries of single-decree instances. System picks a series of values. 
- Leader election process: depose to server with highest ID from heatbeats received. If 2xT time passed, then act as leader. 
- Log distribution process
- DB atomic updates

## config params: 
- concurrency limit α
- Heartbeat every T milliseconds; T must be << round trip of message 

## persisted: 
- **proposer**: 
  - maxRound
  - prepared: i.e. encountered noMoreAccepted msg
  - next index of entry to use for client request
- **Acceptor**: 
  - last log index
  - min proposal
  - log[{accepted proposal, accepted value}]
  - first unchosen index: earliest non ∞ entry

## API interface: 
- **Heartbeat RPC** or use prepare with empty data. 
- **Prepare RPC**:  (log index, proposal #) → current enry accepted{proposal #, value}
- **Accept RPC**:  (log index, proporal{#, value}, first unchosen index in proposer) → minimum proposal, first unchosen index in acceptor
- **Success RPC**:  (index of corresponding acceptor, value) → new first unchosen index in acceptor

## Algorithm: 
### Prepare RPC
- Client uses unique id for each command (ensuring exactly once semantics, no duplicate exec.)
1. choose ealiest log entry empty (not chosen or accepted); 
  - proposal number = largest seen maxRound+1 concatenated with server ID; 
2. PrepareRPC(current log index, proposal # for entire log); 
3. Reject olrder proposal (block off entire log)
  - if highest proposal accepted minProposal < proposal #: then minProposal = proposal #; 
4. Return accepted for current entry & noMoreAccepted  (in case no proposal accepted beyond entry) 
5. if majority, replace/abandon own value with highest accepted proposal from reponses; 
  - if no majority, then no need for more prepares to server; 
### Accept RPC
1. broadcast accept RPC; 
2. proposal # >= minProposal: minProposal = proposal{#, value}; 
3. set all indecies < first unchosen index of proposer and have the same proposal # of proposer to ∞; 
4. return ... 
5. majority & response.minProposal <= Proposal: proposal value chosen (set to ∞); 
6. retry RPC until all acceptors respond (partial replication if crashes); no-op to qcuickly complete operation;
### Success RPC
- Run in background, allows acceptor to be caught up to speed; Repeat as necessary;
### Code Architecture
**We should hash out whether this design makes sense**
- Consensus and Database run as two separate threads
- Database receives requests from the client
- Database forwards requests to the coordinator
  - If there is no coordinator, Database initializes an election
  - If Database is the coordinator, it just responds to the client
  - If another Database (server) is the coordinator, that server will return success
- Consensus thread handles all inter-server rpc communication
  - Consensus stores a list of rpc stubs, one for each replica
  - Will not return values, will only return information about a Database request
    - For get() requests, will return a value
    - For set() requests, will return the accepted value
    - For election() requests, will return the server id of the new coordinator
    - For recovery() requests, will return most recent log and database snapshot
- May require communicating back with the Database thread, for instance, if a replica is a leader and needs to get a value for another replica that issues a get request
