## High level overview of chubby

**Server (replicas)**
* Paxos (or some other form of consensus) to decide a master within a cell (5 replicas)
* Only master can read/write, other replicas merely copy updates from master
* Read requests are served from master immediately
* Write requests are propogated using distributed consensus (Paxos)
* If master fails, new master is elected

**Client library**
* Sends master location requests to any replica to find master
* Directs all requests to master
* Once master is found, can communicate similarly to Unix filesystem API
    * We can use `ls/local/...` for everything since we will only be using one chubby cell in our implementation
    * These should be actual unix files/directories, located in the server's home directory
    * No moving of files between directories, no links, and no path-dependent permissions

## Data Structures

```
struct lock{
    string file_path;
    int_64 lease_expiration;
    bool status;
    // Do we want to use sequencer approach or 
}
```

## RPCs
* `locate_master`: Client sends RPC to a replica requesting info on the master. The replica will respond with the current master's ip and port.
    * The client typically would then ping the master to see if its alive. If not, the client will reach out to any other replica to trigger an election
    * If the master is alive, the client spins up a thread to send KeepAlive RPCs to the master and start its session

* `keep_alive`: Client periodically pings the master to alert it of its status
    * Client sends these to master, and sends another one as soon as it receives a response from the master
    * The master will have a thread for each client which handles these keep alives and issues responses
    * Time out, so that if there is no keep_alive communication for, say, 1 second, the master considers the client dead and frees its locks, deletes its file descriptors, etc.


* `request_reader_lock`: Client sends RPC to master requesting a shared lock for a node (filepath). Master replies LOCKED or UNLOCKED
    * **PARAMS** (filepath, ???)
    * Client treats the response as advisory, can modify the file and go against the lock status if it chooses to do so
    * Server checks if a writer lock exists. If so not, it replies UNLOCKED
    * If the lock is locked, server checks the current time vs the lock's lease expiration. If the lease expired, the server unlocks the lock and replies UNLOCKED to client. Otherwise, the lease hasn't expired and the server replies LOCKED
    * 

## Election process
1. All the entities that want to become a master, try to open a file in write mode.
2. Only one of those get the write mode access and others fail.
3. The one with write access, then writes its identity to the file
4. All the others get the file modification event and know about the the current master now.
5. Primary uses either a sequencer or a lock-delay based mechanism to ensure that out-of-order messages don’t cause inconsistent access and services can confirm if the sequencer for the current master is valid or not.