#ifndef raft_h
#define raft_h

#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>
#include <ctime>
#include <algorithm>
#include <thread>
#include <stdarg.h>

#include "rpc.h"
#include "raft_storage.h"
#include "raft_protocol.h"
#include "raft_state_machine.h"
template<typename state_machine, typename command>
class raft {

static_assert(std::is_base_of<raft_state_machine, state_machine>(), "state_machine must inherit from raft_state_machine");
static_assert(std::is_base_of<raft_command, command>(), "command must inherit from raft_command");


friend class thread_pool;

#define RAFT_LOG(fmt, args...) \
    do { \
        auto now = \
        std::chrono::duration_cast<std::chrono::milliseconds>(\
            std::chrono::system_clock::now().time_since_epoch()\
        ).count();\
        printf("[%ld][%s:%d][node %d term %d] " fmt "\n", now, __FILE__, __LINE__, my_id, current_term, ##args); \
    } while(0);

public:
    raft(
        rpcs* rpc_server,
        std::vector<rpcc*> rpc_clients,
        int idx, 
        raft_storage<command>* storage,
        state_machine* state    
    );
    ~raft();

    // start the raft node.
    // Please make sure all of the rpc request handlers have been registered before this method.
    void start();

    // stop the raft node. 
    // Please make sure all of the background threads are joined in this method.
    // Notice: you should check whether is server should be stopped by calling is_stopped(). 
    //         Once it returns true, you should break all of your long-running loops in the background threads.
    void stop();

    // send a new command to the raft nodes.
    // This method returns true if this raft node is the leader that successfully appends the log.
    // If this node is not the leader, returns false. 
    bool new_command(command cmd, int &term, int &index);

    // returns whether this node is the leader, you should also set the current term;
    bool is_leader(int &term);

    // save a snapshot of the state machine and compact the log.
    bool save_snapshot();

    enum {NOT_VOTE_YET=0,REJECT_AND_BE=1,REJECT_NOT_BE=2,VOTE_AND_BE=3,FAIL_CONNECT=4};
    //constant defined
    // const int NOT_VOTE_YET=0;
    // const int REJECT_AND_BE=1;
    // const int REJECT_NOT_BE=2;
    // const int VOTE_AND_BE=3;
    // const int FAIL_CONNECT=4;
    //FOR APPEND_ENTRIES_REPLY
    enum {HEART_BEAT_SUCCESS=0,APPEND_FAIL_AND_BE_FOLLOWER=1,APPEND_FAIL_AND_ASK_LOGS=2,FINISH_COPY=3,FINISH_COMMIT=5,RESTART_SEND=6,FINISH_RESTART=7};
    // const int HEART_BEAT_SUCCESS=0;
    // const int APPEND_FAIL_AND_BE_FOLLOWER=1;
    // const int APPEND_FAIL_AND_ASK_LOGS=2;
    // const int FINISH_COPY=3;
    // const int FINISH_COMMIT=5;
    //APPEND_ENTRIES_TYPE
    enum {HEART_BEAT_MISSION=0,COPY_MISSION=1,COMMIT_MISSION=2,RESTART_MISSION=3};
    // const int HEART_BEAT_MISSION=0;
    // const int COPY_MISSION=1;
    // const int COMMIT_MISSION=2;
    //COMMIT_HELPER  STATE
    enum {DO_NOTHING=0,COPY_STATE=1,COMMIT_STATE=2};
    // const int DO_NOTHING=0;
    // const int COPY_STATE=1; //should ask the follower to copy
    // const int COMMIT_STATE=2; //should ask the follower to commit
    //append_entries_args
    struct CommitHelper{
        bool lastjob_finished=true;
        int wait_round=0;
        int state=0;
        append_entries_reply reply;
    };
    struct RestartHelper{
        bool shouldSend=false;
        int  begin_index=1;
    };
private:
    std::mutex mtx;                     // A big lock to protect the whole data structure
    request_vote_reply *voteResult;  //to store the vote result,0 means not vote,1 means agree ,2 means not agree,3 means fail to connect
    append_entries_reply* pingResult;                    //to store the ping result
    CommitHelper* commit_helpers;
    RestartHelper* restart_helpers;
    ThrPool* thread_pool;
    raft_storage<command>* storage;              // To persist the raft log
    state_machine* state;  // The state machine that applies the raft log, e.g. a kv store

    rpcs* rpc_server;               // RPC server to recieve and handle the RPC requests
    std::vector<rpcc*> rpc_clients; // RPC clients of all raft nodes including this node
    int my_id;                     // The index of this node in rpc_clients, start from 0
    int totle_num;                 //the num of server
    bool restart=true; 
    int pingRound=0;
    int getPingInterVal(int round){
        if(this->can_recive_command){
            if(this->current_term==1){
                if(round>=9) return 40;
                else{
                    return 93-pingRound*4;
                }
            }
            else{
                return 40;
            }
        }
        else return 20;
    };
    std::atomic_bool stopped;

    enum raft_role {
        follower,
        candidate,
        leader
    };
    raft_role role;
    int current_term;
    //needless all the logs should be load into storage
    // std::vector<log_entry<command>> logs;   //store the log that havent't be commited yet
    
    std::thread* background_election;
    std::thread* background_ping;
    std::thread* background_commit;
    std::thread* background_apply;

    // Your code here:
    clock_t last_received_RPC_time;
    clock_t timeInterVal;
    clock_t timeChunk;
    clock_t baseTime;
    int commitIndex;  
    int lastApplied;
    int last_log_index; //for snapshot and safety
    int last_log_term;
    int leaderId;
    int votefor=-1;
    bool can_recive_command=true;

    //DEBUGLOCK tool
    void lockWithLog(){
        RAFT_LOG("LOCK");
        this->mtx.lock();
    }
    void unLockWithLog(){
        RAFT_LOG("UNLOCK");
        this->mtx.unlock();
    }
private:
    // RPC handlers
    int request_vote(request_vote_args arg, request_vote_reply& reply);

    int append_entries(append_entries_args<command> arg, append_entries_reply& reply);

    int install_snapshot(install_snapshot_args arg, install_snapshot_reply& reply);

    // RPC helpers
    void send_request_vote(int target, request_vote_args arg);
    void handle_request_vote_reply(int target, const request_vote_args& arg, const request_vote_reply& reply);

    void send_append_entries(int target, append_entries_args<command> arg);
    void handle_append_entries_reply(int target, const append_entries_args<command>& arg, const append_entries_reply& reply);

    void send_install_snapshot(int target, install_snapshot_args arg);
    void handle_install_snapshot_reply(int target, const install_snapshot_args& arg, const install_snapshot_reply& reply);
    //get random num from 0-1
    double getRandom(){
        return ((double)rand())/RAND_MAX;
    }
private:
    bool is_stopped();
    int num_nodes() {return rpc_clients.size();}

    // background workers    
    void run_background_ping();
    void run_background_election();
    void run_background_commit();
    void run_background_apply();

    // Your code here:
    void resetVoteResult(){
        for(int i=0;i<totle_num;i++){
            if(i==this->my_id) this->voteResult[i].voteGranted=VOTE_AND_BE;
            else this->voteResult[i].voteGranted=NOT_VOTE_YET;
            // this->voteResult[i].term=this->current_term;
        }
    }
    void resetPingResult(){
        for(int i=0;i<totle_num;i++){
            this->pingResult[i].return_state=HEART_BEAT_SUCCESS;
        }
    }
};

template<typename state_machine, typename command>
raft<state_machine, command>::raft(rpcs* server, std::vector<rpcc*> clients, int idx, raft_storage<command> *storage, state_machine *state) :
    storage(storage),
    state(state),   
    rpc_server(server),
    rpc_clients(clients),
    my_id(idx),
    stopped(false),
    role(follower),
    current_term(0),
    background_election(nullptr),
    background_ping(nullptr),
    background_commit(nullptr),
    background_apply(nullptr)
{
    thread_pool = new ThrPool(32);

        // Register the rpcs.
    rpc_server->reg(raft_rpc_opcodes::op_request_vote, this, &raft::request_vote);
    rpc_server->reg(raft_rpc_opcodes::op_append_entries, this, &raft::append_entries);
    rpc_server->reg(raft_rpc_opcodes::op_install_snapshot, this, &raft::install_snapshot);

    // Your code here: 
    // Do the initialization
    //initial time
    this->commitIndex=0;
    this->lastApplied=0;
    this->last_log_index=0;
    this->last_log_term=0;
    totle_num = clients.size();
    this->timeChunk=200/totle_num;
    this->baseTime=110;
    this->timeInterVal=baseTime+idx*timeChunk;
    this->voteResult=new request_vote_reply[totle_num];
    this->pingResult=new append_entries_reply[totle_num];
    this->commit_helpers=new CommitHelper[totle_num];
    this->restart_helpers=new RestartHelper[totle_num];
    for(int i=0;i<totle_num;i++){
        if(i==idx) voteResult[i].voteGranted=VOTE_AND_BE; //always vote for myself
        else voteResult[i].voteGranted=NOT_VOTE_YET;
        // pingResult[i].if_success=true;
        pingResult[i].return_state=HEART_BEAT_SUCCESS;
    }
    this->storage->readAll(this->current_term,this->votefor,this->commitIndex,this->lastApplied,this->last_log_index,this->last_log_term);
    RAFT_LOG("initial raft current_term:%d,votefor:%d,commitIndex:%d,lastAppiled:%d,last_log_index%d,last_log_term%d",
    this->current_term,this->votefor,this->commitIndex,this->lastApplied,this->last_log_index,this->last_log_term);
    //add the applied log to the state machine
    if(this->lastApplied!=0){
        bool haveSnapshot=false;
        int snapshotIndex;
        int snapshotTerm;
        std::vector<char> mySnapshot=this->storage->readSnapshot(haveSnapshot,snapshotIndex,snapshotTerm);
        if(!haveSnapshot){
            std::vector<log_entry<command>> appliedLogs=this->storage->readLogs(1,this->lastApplied);
            for(auto it:appliedLogs){
            state->apply_log(it.c);
            }
        }else{
            state->apply_snapshot(mySnapshot);
            std::vector<log_entry<command>> appliedLogs=this->storage->readLogs(snapshotIndex+1,this->lastApplied);
            for(auto it:appliedLogs){
            state->apply_log(it.c);
            }
        }
    }
}

template<typename state_machine, typename command>
raft<state_machine, command>::~raft() {
    if (background_ping) {
        delete background_ping;
    }
    if (background_election) {
        delete background_election;
    }
    if (background_commit) {
        delete background_commit;
    }
    if (background_apply) {
        delete background_apply;
    }
    delete thread_pool;
    if(pingResult){
        delete pingResult;
    }    
    if(voteResult){
        delete voteResult;
    }
    if(commit_helpers){
        delete commit_helpers;
    }
    if(restart_helpers){
        delete restart_helpers;
    }
}

/******************************************************************

                        Public Interfaces

*******************************************************************/

template<typename state_machine, typename command>
void raft<state_machine, command>::stop() {
    stopped.store(true);
    background_ping->join();
    background_election->join();
    background_commit->join();
    background_apply->join();
    thread_pool->destroy();
}

template<typename state_machine, typename command>
bool raft<state_machine, command>::is_stopped() {
    return stopped.load();
}

template<typename state_machine, typename command>
bool raft<state_machine, command>::is_leader(int &term) {
    term = current_term;
    return role == leader;
}

template<typename state_machine, typename command>
void raft<state_machine, command>::start() {
    // Your code here:
    
    RAFT_LOG("start");
    this->last_received_RPC_time = clock();
    srand((unsigned)time(NULL));
    this->background_election = new std::thread(&raft::run_background_election, this);
    this->background_ping = new std::thread(&raft::run_background_ping, this);
    this->background_commit = new std::thread(&raft::run_background_commit, this);
    this->background_apply = new std::thread(&raft::run_background_apply, this);
}

template<typename state_machine, typename command>
bool raft<state_machine, command>::new_command(command cmd, int &term, int &index) {
    // Your code here:
    this->mtx.lock();
    raft_role current_role=this->role;
    if(current_role!=leader) {
        this->mtx.unlock();
        return false;
    }
    if(!can_recive_command){
        this->mtx.unlock();
        return false;
    }
    // if(this->logs.empty()){
    //     //all previous is commited
    //     index=this->commitIndex+1;
    // }else{
    //     index=this->logs[logs.size()-1].index+1;
    // }
    term=this->current_term;
    index=this->last_log_index+1;
    log_entry<command> newLog(index,term,cmd);
    // this->logs.push_back(newLog);
    this->storage->writeLog(newLog);
    //log_entry<command> ttt=this->storage->readLog(index);
    this->last_log_index++;
    this->last_log_term=term;    
    this->storage->writeLastIndexAndLastTerm(last_log_index,last_log_term);
    RAFT_LOG("new command add to vector");
    RAFT_LOG("the cmd value input=%d",cmd.value);
    // RAFT_LOG("the cmd value read immediately :%d",ttt.c.value);
    this->mtx.unlock();

    return true;
}

template<typename state_machine, typename command>
bool raft<state_machine, command>::save_snapshot() {
    // Your code here:
    //split the data into snapshot and logs
    this->mtx.lock();
    while(this->commitIndex!=this->lastApplied){
        //release the lock and wait for applied
        this->mtx.unlock();
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
         this->mtx.lock();
    }
    //begin snapshort
    RAFT_LOG("begin snapshot");
    std::vector<char> phote=state->snapshot();
    this->storage->makeSnapshot(this->commitIndex,phote);
    RAFT_LOG("end snapshot");
    this->mtx.unlock();
    return true;
}



/******************************************************************

                         RPC Related

*******************************************************************/
template<typename state_machine, typename command>
int raft<state_machine, command>::request_vote(request_vote_args args, request_vote_reply& reply) {
    // Your code here:
    int candidateId=args.candidateId;
    int term=args.my_term;
    int come_last_index= args.last_log_index;
    int come_last_term= args.last_log_term;
    #ifdef DEBUGLOCK
        RAFT_LOG("LOCK");
        this->mtx.lock();
    #else
    mtx.lock();
    #endif
    if(this->current_term>term){
        reply.term=this->current_term;
        reply.voteGranted=REJECT_NOT_BE;
    }else if(this->current_term==term){
        if(this->votefor!=-1){
        reply.voteGranted=REJECT_AND_BE;
        }
        else{
            //safety handler
        bool flag=true;
        //can still be cadidate
        if(this->last_log_term>come_last_term){
            flag=false;
            reply.voteGranted=REJECT_AND_BE;
        }else if(this->last_log_term==come_last_term){
            if(this->last_log_index>come_last_index){
                flag = false;
                reply.voteGranted=REJECT_AND_BE;
            }
        }
        if(flag){
            reply.voteGranted=VOTE_AND_BE;
            this->current_term=term;
            this->storage->writeCurrentTerm(this->current_term);
            this->votefor=candidateId;
            this->storage->writeVotedFor(this->votefor);
            }
        }
    }
    else {
        //safety handler
        bool flag=true;
        this->current_term=term;
        if(this->role==leader){
            this->role=follower;
            this->last_received_RPC_time=clock();
            this->timeInterVal=baseTime+my_id*timeChunk+this->getRandom()*timeChunk;
        }
        this->storage->writeCurrentTerm(this->current_term);
        this->votefor=-1;
        this->storage->writeVotedFor(this->votefor);
        //can still be cadidate
        if(this->last_log_term>come_last_term){
            flag=false;
            reply.voteGranted=REJECT_AND_BE;
        }else if(this->last_log_term==come_last_term){
            if(this->last_log_index>come_last_index){
                flag = false;
                reply.voteGranted=REJECT_AND_BE;
            }
        }
        if(flag){
            reply.voteGranted=VOTE_AND_BE;
            this->current_term=term;
            this->votefor=candidateId;
            this->storage->writeCurrentTerm(this->current_term);
            this->storage->writeVotedFor(this->votefor);
            }
    }
    #ifdef DEBUGLOCK
        RAFT_LOG("UNLOCK");
        this->mtx.unlock();
    #else
    mtx.unlock();
    #endif
    return 0;
}


template<typename state_machine, typename command>
void raft<state_machine, command>::handle_request_vote_reply(int target, const request_vote_args& arg, const request_vote_reply& reply) {
    // Your code here:
    if(this->role!=candidate){
        return;
    }
    RAFT_LOG("elect reply -tergat:%d ,reply:%d",target,reply.voteGranted);
    this->voteResult[target]=reply;
}


template<typename state_machine, typename command>
int raft<state_machine, command>::append_entries(append_entries_args<command> arg, append_entries_reply& reply) {
    //RAFT_LOG("recive arg:term=%d,learder=%d,type=%d,",arg.term,arg.leaderId,arg.type);
    int term = arg.term;
    int Come_leaderId = arg.leaderId;
    int prev_log_index = arg.prevLogIndex;
    int prev_log_term = arg.prevLogTerm;
    int leader_commitIndex = arg.commitIndex;
    int logNums=arg.logNums;
    #ifdef DEBUGLOCK
        RAFT_LOG("LOCK");
        this->mtx.lock();
    #else
    mtx.lock();
    #endif
    switch (arg.type)
    {
    case HEART_BEAT_MISSION:   
        //RAFT_LOG("recieve heart");
        this->last_received_RPC_time=clock();
        this->timeInterVal=baseTime+my_id*timeChunk+this->getRandom()*timeChunk;
        if(this->role==leader){
            this->role=follower;
        }
        if(this->current_term>term){  // the leader is old, should  weed out the leader
            reply.term=this->current_term;
            reply.return_state=APPEND_FAIL_AND_BE_FOLLOWER;
        }else if(!this->restart){
            this->leaderId=Come_leaderId;
            if(term>this->current_term){
            this->current_term=term;
            this->votefor=-1;
            this->storage->writeCurrentTerm(this->current_term);
            this->storage->writeVotedFor(this->votefor);
            }
            reply.return_state=HEART_BEAT_SUCCESS;
        }else{
            //first restart, should ask for log
            reply.return_state=RESTART_SEND;
            reply.ask_begin_index=this->commitIndex+1;
            this->restart=false;
        }
        break;
    case COPY_MISSION:
        //find the prevLogIndex
        RAFT_LOG("recive copy mission,copy num is%d, prev_log_index is%d, prev_log_term is %d",logNums,arg.prevLogIndex,arg.prevLogTerm);
        // if(arg.term<this->current_term){
        //     RAFT_LOG("err copy mission do not come from leader");
        //     //reply.return_state=APPEND_FAIL_AND_BE_FOLLOWER;
        //     RAFT_LOG("the reply state is%d",reply.return_state);
        //     break;
        // }
        this->last_received_RPC_time=clock();
        this->timeInterVal=baseTime+my_id*timeChunk+this->getRandom()*timeChunk;
        if(this->role==leader&&arg.term>this->current_term){
            this->role=follower;
        }
        for(int i=0;i<logNums;i++){
            RAFT_LOG("the value recive is %d",arg.logs[i].c.value);
        }
        if(prev_log_index==0){
            //definately don't have snapshot
            //the leader don't have any previous log more,can copy directly    
            if(logNums>200){
                RAFT_LOG("ERROR,THE LOG TOOMUCH");
            }
            for(int i=0;i<logNums;i++){
                log_entry<command> cmd=arg.logs[i];
                this->storage->writeLog(cmd);
                if(i>0){
                    if(arg.logs[i].index<arg.logs[i-1].index){
                        RAFT_LOG("ERR THE LATER INDEX LESS THAN FRONT");
                    }
                }
            }
            if(logNums>0){
                log_entry<command> last=arg.logs[logNums-1];
                this->last_log_index=last.index;
                this->last_log_term=last.term;
                this->storage->writeLastIndexAndLastTerm(this->last_log_index,this->last_log_term);
            }
            RAFT_LOG("finish copy");
            reply.return_state=FINISH_COPY;
        }
        else{
            if(!arg.haveSnapshot){
            //check if the prev same
                if(this->last_log_index<prev_log_index){
                //do not have the prev_log
                    RAFT_LOG("do not have the log with index%d ,term %d,ask more log begin at %d",prev_log_index,prev_log_term,this->last_log_index);
                    reply.return_state=APPEND_FAIL_AND_ASK_LOGS;
                    reply.ask_begin_index=this->last_log_index;
                }
                else{
                    //have prev_log, should get the prev_log
                    int compareTerm;
                    if(this->storage->isExist(prev_log_index)){
                        log_entry<command> compare_log = this->storage->readLog(prev_log_index);
                        compareTerm = compare_log.term;
                    }else{
                        int MySnapshotIndex;
                        this->storage->readSnapShotIndexAndTerm(MySnapshotIndex,compareTerm);
                        if(MySnapshotIndex!=prev_log_index){
                            //snapshort inconsistent,should ask for snapshort
                            compareTerm=-1;
                        }
                    }
                    RAFT_LOG("checkout index %d,local term is%d ,come term is%d",prev_log_index,compareTerm,prev_log_term);
                    if(compareTerm==prev_log_term){
                        //check success,delete the behind file
                        RAFT_LOG("check success")
                        // for(int i=prev_log_index+1;i<=this->last_log_index;i++){
                        //     this->storage->deleteLog(i);
                        // }
                        //add the logs
                        for(int i=0;i<logNums;i++){
                            this->storage->writeLog(arg.logs[i]);
                        }
                        //update the last index and last term
                        if(logNums>0){
                        this->last_log_index=arg.logs[logNums-1].index;
                        this->last_log_term=arg.logs[logNums-1].term;
                        this->storage->writeLastIndexAndLastTerm(this->last_log_index,this->last_log_term);
                        }
                        RAFT_LOG("finish copy");
                        //modify the reply
                        reply.return_state=FINISH_COPY;

                    }
                    else{
                        //check fail,need more front log
                        reply.return_state=APPEND_FAIL_AND_ASK_LOGS;
                        // reply.ask_begin_index=prev_log_index-1;
                        if(prev_log_index>5){
                            reply.ask_begin_index=prev_log_index-5;
                        }else{
                            reply.ask_begin_index=prev_log_index-1;
                        }
                    }
                }
            }
            else{
                //have snapshot sent
                //first apply the snapshot
                std::vector<char> recivedSnapshot=arg.Snapshot;
                this->commitIndex=arg.prevLogIndex;
                this->lastApplied=arg.prevLogIndex;
                this->last_log_index=arg.prevLogIndex;
                this->last_log_term=arg.prevLogTerm;
                RAFT_LOG("come snapshot size:%d,prevLogIndex:%d,prevLogTerm:%d",recivedSnapshot.size(),arg.prevLogIndex,arg.prevLogTerm)
                state->apply_snapshot(recivedSnapshot);
                this->storage->applyComeSnapshot(arg.prevLogIndex,arg.prevLogTerm,recivedSnapshot);
                //copy the remain logs
                for(int i=0;i<logNums;i++){
                    this->storage->writeLog(arg.logs[i]);
                }
                    //update the last index and last term
                if(logNums>0){
                    this->last_log_index=arg.logs[logNums-1].index;
                    this->last_log_term=arg.logs[logNums-1].term;
                    this->storage->writeLastIndexAndLastTerm(this->last_log_index,this->last_log_term);
                }
                RAFT_LOG("finish copy");
                //modify the reply
                reply.return_state=FINISH_COPY;
                
            }
        }
        break;
    case COMMIT_MISSION:
        //should know the copy mission is finished
        //commit all the log in vector and change the commit index
        // for(auto it:this->logs){
        //     this->storage->writeLog(it);
        //     this->commitIndex=it.index > this->commitIndex ? it.index:this->commitIndex;
        // }
        // this->logs.clear();
        RAFT_LOG("recive commit mission");
        this->last_received_RPC_time=clock();
        this->timeInterVal=baseTime+my_id*timeChunk+this->getRandom()*timeChunk;
        if(this->role==leader){
            this->role=follower;
        }
        this->commitIndex = leader_commitIndex > this->last_log_index? this->last_log_index:leader_commitIndex;
        this->storage->writeCommitIndex(this->commitIndex);
        reply.return_state=FINISH_COMMIT;
        break;
    case RESTART_MISSION:
    //copy and commit
        this->last_received_RPC_time=clock();
        this->timeInterVal=baseTime+my_id*timeChunk+this->getRandom()*timeChunk;
        RAFT_LOG("recive restart mission,logNums:%d",logNums);
        if(!arg.haveSnapshot){
            for(int i=0;i<logNums;i++){
                RAFT_LOG("restart log index:%d,value%d",arg.logs[i].index,arg.logs[i].c.value);
                log_entry<command> cmd=arg.logs[i];
                this->storage->writeLog(cmd);
                if(i>0){
                    if(arg.logs[i].index<arg.logs[i-1].index){
                        RAFT_LOG("ERR THE LATER INDEX LESS THAN FRONT");
                    }
                }
            }
            if(logNums>0){
                log_entry<command> last=arg.logs[logNums-1];
                this->last_log_index=last.index;
                this->last_log_term=last.term;
                this->storage->writeLastIndexAndLastTerm(this->last_log_index,this->last_log_term);
            }
            this->commitIndex=this->last_log_index;
            this->storage->writeCommitIndex(this->commitIndex); 
            reply.return_state=FINISH_RESTART;   
        }else{
            //have snapshot
            std::vector<char> recivedSnapshot=arg.Snapshot;
            this->commitIndex=arg.prevLogIndex;
            this->lastApplied=arg.prevLogIndex;
            this->last_log_index=arg.prevLogIndex;
            this->last_log_term=arg.prevLogTerm;
            state->apply_snapshot(recivedSnapshot);
            this->storage->applyComeSnapshot(arg.prevLogIndex,arg.prevLogTerm,recivedSnapshot);
            for(int i=0;i<logNums;i++){
                this->storage->writeLog(arg.logs[i]);
            }
            if(logNums>0){
                this->last_log_index=arg.logs[logNums-1].index;
                this->last_log_term=arg.logs[logNums-1].term;
                this->storage->writeLastIndexAndLastTerm(this->last_log_index,this->last_log_term);
            }
            this->commitIndex=this->last_log_index;
            this->storage->writeCommitIndex(this->commitIndex); 
            reply.return_state=FINISH_RESTART; 
        }
        break;
    default:
        break;
    }
 
    #ifdef DEBUGLOCK
        RAFT_LOG("UNLOCK");
        this->mtx.unlock();
    #else
    mtx.unlock();
    #endif
    return 0;
}

template<typename state_machine, typename command>
void raft<state_machine, command>::handle_append_entries_reply(int target, const append_entries_args<command>& arg, const append_entries_reply& reply) {
    // Your code here:
    CommitHelper helper;
    RestartHelper rhelper;
    switch (reply.return_state)
    {
    case HEART_BEAT_SUCCESS:
        //ping reply
        this->pingResult[target] = reply;
        break;
    case APPEND_FAIL_AND_BE_FOLLOWER:
        //ping reply
        this->pingResult[target] = reply;
        break;
    case APPEND_FAIL_AND_ASK_LOGS:
        helper.state=COPY_STATE;
        helper.lastjob_finished=true;
        helper.wait_round=0;
        helper.reply=reply;
        this->commit_helpers[target]=helper;
        break;
    case FINISH_COPY:
        helper.state=COMMIT_STATE; //finish copy
        helper.lastjob_finished=true;
        helper.wait_round=0;
        helper.reply=reply;
        this->commit_helpers[target]=helper;
        break;
    case FINISH_COMMIT:
        helper.state=DO_NOTHING;
        helper.lastjob_finished=true;
        helper.wait_round=0;
        helper.reply=reply;
        this->commit_helpers[target]=helper; 
        break;
    case RESTART_SEND:
        rhelper.shouldSend=true;
        rhelper.begin_index=reply.ask_begin_index;
        this->restart_helpers[target]=rhelper; 
        break;
    case FINISH_RESTART:
        break;      
    default:
        break;
    }
    return;
}


template<typename state_machine, typename command>
int raft<state_machine, command>::install_snapshot(install_snapshot_args args, install_snapshot_reply& reply) {
    // Your code here:
    
    return 0;
}


template<typename state_machine, typename command>
void raft<state_machine, command>::handle_install_snapshot_reply(int target, const install_snapshot_args& arg, const install_snapshot_reply& reply) {
    // Your code here:
    return;
}

template<typename state_machine, typename command>
void raft<state_machine, command>::send_request_vote(int target, request_vote_args arg) {
    //RAFT_LOG("RPC CALL ELECT");
    request_vote_reply reply;
    if (rpc_clients[target]->call(raft_rpc_opcodes::op_request_vote, arg, reply) == 0) {    
        handle_request_vote_reply(target, arg, reply);
    } else {
        // RPC fails
        // if(this->role!=candidate){
        //     return;
        // }
        reply.voteGranted=FAIL_CONNECT;
        this->voteResult[target]=reply;
    }
} 

template<typename state_machine, typename command>
void raft<state_machine, command>::send_append_entries(int target, append_entries_args<command> arg) {
    //RAFT_LOG("CALL RPC SEND APPEND ENTRIES");
    append_entries_reply reply;
    if (rpc_clients[target]->call(raft_rpc_opcodes::op_append_entries, arg, reply) == 0) {
        handle_append_entries_reply(target, arg, reply);
    } else {
        // RPC fails,not handle yet
        if(arg.type==HEART_BEAT_MISSION){
            reply.return_state=FAIL_CONNECT;
            this->pingResult[target]=reply;
            // this->pingResult[target].return_state=FAIL_CONNECT;
        }
        else{
            // reply.return_state=FAIL_CONNECT;
            // this->commit_helpers[target]=reply;
            RAFT_LOG("mission didn't answer");
             this->commit_helpers[target].state=FAIL_CONNECT;
        }
        
    }
}

template<typename state_machine, typename command>
void raft<state_machine, command>::send_install_snapshot(int target, install_snapshot_args arg) {
    install_snapshot_reply reply;
    if (rpc_clients[target]->call(raft_rpc_opcodes::op_install_snapshot, arg, reply) == 0) {
        handle_install_snapshot_reply(target, arg, reply);
    } else {
        // RPC fails
    }
}

/******************************************************************

                        Background Workers

*******************************************************************/

template<typename state_machine, typename command>
void raft<state_machine, command>::run_background_election() {
    // Check the liveness of the leader.
    // Work for followers and candidates.

    // Hints: You should record the time you received the last RPC.
    //        And in this function, you can compare the current time with it.
    //        For example:
    //        if (current_time - last_received_RPC_time > timeout) start_election();
    //        Actually, the timeout should be different between the follower (e.g. 300-500ms) and the candidate (e.g. 1s).
    while (true) {
        if (is_stopped()) return;
        // Your code here:
        //based on the role to decide what to do
        // int current_time=clock();
        // int last_time=0
        // last_time = this->last_received_RPC_time;
        mtx.lock();
        int current_time;
        int last_time;
        int agree_num=1;  //the agree num
        bool vote_finished=true;
        request_vote_args vote_args;      
        raft_role current_role=this->role;
        last_time = this->last_received_RPC_time;
        int myID=this->my_id;
        switch (current_role)
        {
        case leader:
        //donothing, needless to election
            break;
        case follower:
            current_time=clock();
            //RAFT_LOG("DEBUG,role:%d ,last_time:%d,current_time:%d ,judge",int(current_role),last_time,current_time);
            if((current_time-last_time)*1000/CLOCKS_PER_SEC>this->timeInterVal){
                //should elect
                this->resetVoteResult(); 
                this->role=candidate;
                vote_args.candidateId=this->my_id;
                vote_args.my_term=++this->current_term;
                this->storage->writeCurrentTerm(this->current_term);
                vote_args.last_log_index=this->last_log_index;
                vote_args.last_log_term=this->last_log_term;
                RAFT_LOG("begin select,cadidate id:%d ,term:%d , last_log_index:%d ,last_log_term:%d",vote_args.candidateId,vote_args.my_term,vote_args.last_log_index,vote_args.last_log_term);
                this->timeInterVal=baseTime+my_id*timeChunk+this->getRandom()*timeChunk;
                //use the thread pool to request
                this->mtx.unlock();
                for(int i=0;i<totle_num;i++){
                    if(i==myID) continue;
                    this->thread_pool->addObjJob(this,&raft::send_request_vote,i,vote_args);
                }
                  std::this_thread::sleep_for(std::chrono::milliseconds(20));
                  continue;
            }
            break;
        case candidate:
            for(int i=0;i<totle_num;i++){
                //judge first
                if(2*agree_num>totle_num){
                    //can be leader now
                    this->role=leader;
                    RAFT_LOG("turn to leader");
                    this->restart=false;
                    this->resetVoteResult();
                    break;
                }
                if(i==this->my_id) continue;
                if(voteResult[i].voteGranted==NOT_VOTE_YET){
                    vote_finished=false;
                    continue;
                }
                if(voteResult[i].voteGranted==REJECT_AND_BE){
                    continue;
                }
                if(voteResult[i].voteGranted==REJECT_NOT_BE){
                    this->role=follower;
                    this->last_received_RPC_time=clock(); //make sure it won't elect soon
                    this->current_term=voteResult[i].term;
                    this->timeInterVal=baseTime+my_id*timeChunk+this->getRandom()*timeChunk;
                    this->votefor=-1;
                    this->storage->writeCurrentTerm(this->current_term);
                    this->storage->writeVotedFor(this->votefor);
                    this->resetVoteResult();
                    break;
                }
                if(voteResult[i].voteGranted==VOTE_AND_BE){
                    agree_num++;
                    continue;
                }
                if(voteResult[i].voteGranted==FAIL_CONNECT){
                    continue;
                }
            }
            if(this->role==candidate){
                if(vote_finished){
                    if(2*agree_num>totle_num){
                        this->role=leader;
                        this->resetVoteResult();
                    }else{
                        this->role=follower;
                        this->last_received_RPC_time=clock(); //make sure it won't elect soon
                        this->timeInterVal=baseTime+my_id*timeChunk+this->getRandom()*timeChunk;
                        this->resetVoteResult();
                    }
                }
            }
            break;
        default:
            break;
        }
        mtx.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }    
    

    return;
}

template<typename state_machine, typename command>
void raft<state_machine, command>::run_background_commit() {
    // Send logs/snapshots to the follower.
    // Only work for the leader.

    // Hints: You should check the leader's last log index and the follower's next log index.        
    append_entries_args<command> args;
    while (true) {
        if (is_stopped()) return;
        // Your code here:
        this->mtx.lock();
        //RAFT_LOG("get commit lock");
        raft_role current_role=this->role;
        if(current_role!=leader||(this->role==leader&&!can_recive_command)){
             mtx.unlock();
             std::this_thread::sleep_for(std::chrono::milliseconds(20));
             continue;
        }
        else{
            if(this->last_log_index==this->commitIndex){
                //check if there are restart worker that need to send data
                std::map<int,append_entries_args<command>> restartArgs;
                for(int i=0;i<totle_num;i++){
                    if(this->restart_helpers[i].shouldSend){
                        //only send once
                        this->restart_helpers[i].shouldSend=false;
                        RAFT_LOG("restart,num:%d,begin_index:%d,my commitIndex:%d",i,restart_helpers[i].begin_index,this->commitIndex);
                        append_entries_args<command> reArg;
                        if(restart_helpers[i].begin_index>this->commitIndex){
                            continue;
                        }
                        reArg.type=RESTART_MISSION;
                        if(this->storage->isExist(restart_helpers[i].begin_index)){
                            std::vector<log_entry<command>> restartLogs=this->storage->readLogs(restart_helpers[i].begin_index,this->commitIndex);
                            reArg.logNums=restartLogs.size();
                            RAFT_LOG(" restartLogs size=%d", restartLogs.size());
                            reArg.logs=restartLogs;
                            reArg.term=this->current_term;
                            // for(int index=0;index<restartLogs.size();index++){
                            //     reArg.logs[index]=restartLogs[index];
                            // }
                            restartArgs.insert(std::pair<int,append_entries_args<command>>(i,reArg));
                        }else{
                            //in snapshort
                            bool suc;
                            int snapshortIndex;
                            int snapshortTerm;
                            std::vector<char> leaderSnapshot=this->storage->readSnapshot(suc,snapshortIndex,snapshortTerm);
                            reArg.haveSnapshot=true;
                            reArg.Snapshot=leaderSnapshot;
                            reArg.prevLogIndex=snapshortIndex;
                            reArg.prevLogTerm=snapshortTerm;
                            reArg.term=this->current_term;
                            std::vector<log_entry<command>> restartLogs=this->storage->readLogs(snapshortIndex+1,this->commitIndex);
                            reArg.logNums=restartLogs.size();
                            RAFT_LOG(" restartLogs size=%d", restartLogs.size());
                            reArg.logs=restartLogs;
                        //     for(int index=0;index<restartLogs.size();index++){
                        //     reArg.logs[index]=restartLogs[index];
                        //    }
                           restartArgs.insert(std::pair<int,append_entries_args<command>>(i,reArg));
                        }
                    }
                }
                if(!restartArgs.empty()){
                    RAFT_LOG("begin restart log send");
                    //send mission
                    mtx.unlock();
                    for(auto item:restartArgs){
                         this->thread_pool->addObjJob(this,&raft::send_append_entries,item.first,item.second);
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    continue;
                }
                mtx.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }
            else{
                //have log to copy and commit,start the copy process
                RAFT_LOG("begin new commit work");
                args.term=this->current_term;
                args.type=COPY_MISSION;
                args.leaderId=this->my_id;
                args.commitIndex=this->commitIndex;
                int write_end_index=this->last_log_index;
                int now_commitIndex=this->commitIndex;
                // if((write_end_index-now_commitIndex)>200){
                //     write_end_index=now_commitIndex+200; //in one time, the transform log nums should <=200
                // }
                int myId=this->my_id;
                //form the now_logs vector
                std::vector<log_entry<command>> now_logs;
                // for(int i=this->commitIndex+1;i<=write_end_index;i++){
                //     now_logs.push_back(this->storage->readLog(i));
                // }
                now_logs=this->storage->readLogs(now_commitIndex+1,write_end_index);
                //get prevLogIndex and prevLogTerm
                RAFT_LOG("now commitIndex is%d",now_commitIndex);
                if(now_commitIndex==0){
                    //do not have prev log
                    args.prevLogIndex=0;
                    args.prevLogTerm=0;
                }else{
                    //judge if now_commitIndex exist
                    if(this->storage->isExist(now_commitIndex)){
                    log_entry<command> log=this->storage->readLog(now_commitIndex);
                    args.prevLogIndex=log.index;
                    args.prevLogTerm=log.term;
                    }else{
                        //in snapshot
                        this->storage->readSnapShotIndexAndTerm(args.prevLogIndex,args.prevLogTerm);
                    }
                }
                args.logNums=now_logs.size();
                args.logs=now_logs;
                // for(int i=0;i<args.logNums;i++){
                //     args.logs[i]=now_logs[i];
                // }
                for(int i=0;i<totle_num;i++){ //init
                    if(i==this->my_id) continue;
                    this->commit_helpers[i].lastjob_finished=false;
                    this->commit_helpers[i].wait_round=0;
                    this->commit_helpers[i].state=DO_NOTHING;
                }
                bool flag=true;
                bool begin_commit=false;
                bool can_quit=false;
                //three kinds of mission
                std::map<int,CommitHelper> unfinished_missions;
                std::map<int,CommitHelper> copy_missions;
                std::map<int,CommitHelper> commit_missions;
                RAFT_LOG("copy mission have send,log nums:%d",args.logNums);
                mtx.unlock();
                for(int i=0;i<totle_num;i++){
                    //do the copy mission
                    if(i==myId) continue;
                    this->thread_pool->addObjJob(this,&raft::send_append_entries,i,args);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(60));
                while(true){
                    //loop and finish the copy result
                    mtx.lock();
                    RAFT_LOG("handle mission reply");
                    if(this->role!=leader){
                        RAFT_LOG("copy failed and became stop");
                        mtx.unlock();
                        break;
                    }
                    //prepare for the missions set
                    if(flag){
                        //initial the three kinds misssion
                        for(int i=0;i<totle_num;i++){
                            if(i==this->my_id) continue;
                            //missions.insert(std::pair<int,CommitHelper>(i,this->commit_helpers[i]));
                            if(this->commit_helpers[i].state==FAIL_CONNECT){
                                this->commit_helpers[i].lastjob_finished=true;
                                continue;
                            }
                            if(!this->commit_helpers[i].lastjob_finished){
                                this->commit_helpers[i].wait_round++;
                                unfinished_missions.insert(std::pair<int,CommitHelper>(i,this->commit_helpers[i]));
                            }else{
                                if(this->commit_helpers[i].state==COPY_STATE){
                                    copy_missions.insert(std::pair<int,CommitHelper>(i,this->commit_helpers[i]));
                                }else{
                                    commit_missions.insert(std::pair<int,CommitHelper>(i,this->commit_helpers[i]));
                                }
                            }
                        }
                        flag=false;
                        RAFT_LOG("fist init three mission_set:%d,copy size:%d,commit size%d",unfinished_missions.size(),copy_missions.size(),commit_missions.size());
                    }else{
                        //update the three missions set
                        std::map<int,CommitHelper> unfinished_missions2;
                        std::map<int,CommitHelper> copy_missions2;
                        //RAFT_LOG("unfinished size:%d,copy size:%d,commit size%d",unfinished_missions.size(),copy_missions.size(),commit_missions.size());
                        for(auto it:copy_missions){
                            CommitHelper chelper=this->commit_helpers[it.first];
                            if(!chelper.lastjob_finished){
                                this->commit_helpers[it.first].wait_round++;
                                unfinished_missions2.insert(std::pair<int,CommitHelper>(it.first,this->commit_helpers[it.first]));
                            }else{
                                if(chelper.state==COPY_STATE){
                                    copy_missions2.insert(std::pair<int,CommitHelper>(it.first,this->commit_helpers[it.first]));
                                }else if(chelper.state==COMMIT_STATE){
                                    commit_missions.insert(std::pair<int,CommitHelper>(it.first,this->commit_helpers[it.first]));
                                }
                            }
                        }
                        
                        // if(begin_commit){
                        //     for(auto it:commit_missions){
                        //        CommitHelper cmhelp=this->commit_helpers[it.first];
                        //        if(cmhelp.lastjob_finished){
                        //            commit_missions.erase(it.first);
                        //        }
                        //     }
                        // }
                        for(auto it:unfinished_missions){
                            CommitHelper uhelper=this->commit_helpers[it.first];
                            if(uhelper.lastjob_finished){
                                 if(uhelper.state==COPY_STATE){
                                    copy_missions2.insert(std::pair<int,CommitHelper>(it.first,this->commit_helpers[it.first]));
                                }else if(uhelper.state==COMMIT_STATE){
                                    commit_missions.insert(std::pair<int,CommitHelper>(it.first,this->commit_helpers[it.first]));
                                }
                                continue;
                            }
                            this->commit_helpers[it.first].wait_round++;
                            if(this->commit_helpers[it.first].wait_round>5){
                                this->commit_helpers[it.first].wait_round=0;
                                this->commit_helpers[it.first].lastjob_finished=true;
                                this->commit_helpers[it.first].state=DO_NOTHING;
                                continue;
                            }
                            else{
                                unfinished_missions2.insert(std::pair<int,CommitHelper>(it.first,this->commit_helpers[it.first]));
                            }
                        }
                        unfinished_missions.clear();
                        unfinished_missions=unfinished_missions2;
                        copy_missions.clear();
                        copy_missions=copy_missions2;
                         RAFT_LOG("the new mission set is unfinished size:%d,copy size:%d,commit size%d",unfinished_missions.size(),copy_missions.size(),commit_missions.size());
                        //RAFT_LOG("die 2");
                    }
                    //based on the three missions set, form the args and set task
                    std::map<int,append_entries_args<command>> missions_args;
                    if(begin_commit){
                        if(unfinished_missions.empty()&&copy_missions.empty()){
                            can_quit=true;
                            RAFT_LOG("turn can_quit to true");
                        }
                        for(auto it:copy_missions){ 
                            append_entries_args<command> cparg;
                            CommitHelper chelp=this->commit_helpers[it.first];
                            append_entries_reply creply=chelp.reply;
                            bool haveLog; 
                            //form the prev arg
                            cparg.term=this->current_term;
                            if(creply.ask_begin_index<=0){
                                haveLog=!this->storage->haveSnapShot();
                                if(haveLog){
                                    cparg.prevLogIndex=0;
                                    cparg.prevLogTerm=0;
                                }else{
                                    cparg.Snapshot=this->storage->readSnapshot(cparg.haveSnapshot,cparg.prevLogIndex,cparg.prevLogTerm);
                                    RAFT_LOG("do not have log with index%d , send snapshot,snapshot size=%d,Latindex=%d,LastTerm=%d",creply.ask_begin_index,cparg.Snapshot.size(),cparg.prevLogIndex,cparg.prevLogTerm);
                                }
                            }else{
                                //check if ask_begin_index exist
                                haveLog=this->storage->isExist(creply.ask_begin_index);
                                if(haveLog){
                                    RAFT_LOG("the log that index:%d ,exist",creply.ask_begin_index);
                                    log_entry<command> check_one=this->storage->readLog(creply.ask_begin_index);
                                    cparg.prevLogIndex=check_one.index;
                                    cparg.prevLogTerm = check_one.term;
                                }else{
                                    //send snap short
                                     RAFT_LOG("the log that index:%d ,do not exist",creply.ask_begin_index);
                                    int snapShortIndex;
                                    int snapShortTerm;
                                    this->storage->readSnapShotIndexAndTerm(snapShortIndex,snapShortTerm);
                                    if(snapShortIndex==creply.ask_begin_index){
                                        cparg.prevLogIndex=snapShortIndex;
                                        cparg.prevLogTerm =snapShortTerm;
                                    }else{
                                        //should send snapshort
                                        cparg.Snapshot=this->storage->readSnapshot(cparg.haveSnapshot,cparg.prevLogIndex,cparg.prevLogTerm);
                                    }
                                }
                            }
                            std::vector<log_entry<command>> prev_logs;
                            if(!cparg.haveSnapshot){
                            //get the follow logs
                                prev_logs=this->storage->readLogs(creply.ask_begin_index+1,write_end_index);
                            //     for(int i=0;i<now_logs.size();i++){
                            //     //add the uncommit work
                            //     prev_logs.push_back(now_logs[i]);
                            // }
                            }else{
                                prev_logs=this->storage->readLogs(cparg.prevLogIndex+1,write_end_index);
                            }
                            cparg.term=this->current_term;
                            cparg.leaderId=this->my_id;
                            cparg.type=COPY_MISSION;
                            cparg.logNums=prev_logs.size();
                            if(prev_logs.size()>200){
                                RAFT_LOG("ERROR,TOO MANY LOG");
                            }
                            cparg.logs=prev_logs;
                            // for(int i=0;i<prev_logs.size();i++){
                            //     cparg.logs[i]=prev_logs[i];
                            // }
                            missions_args.insert(std::pair<int,append_entries_args<command>>(it.first,cparg));                           
                        }
                        for(auto it:commit_missions){
                            //finish copy,new come commit_mission
                            append_entries_args<command> cmarg;
                            cmarg.term=this->current_term;
                            cmarg.leaderId=this->my_id;
                            cmarg.type=COMMIT_MISSION;
                            cmarg.commitIndex=this->commitIndex;
                            cmarg.logNums=0;
                            missions_args.insert(std::pair<int,append_entries_args<command>>(it.first,cmarg));  
                        }
                    }else{
                        //haven't begin commit yet
                        //judge if can start commit
                        if(2*(commit_missions.size()+1)>this->totle_num){
                            begin_commit=true;
                            RAFT_LOG("commit_missions-size=%d ,totle_num:%d",commit_missions.size(),this->totle_num);
                            RAFT_LOG("turn begin_commit to true");
                        }else{
                            if(unfinished_missions.size()==0&&copy_missions.size()==0){
                                can_quit=true;                             
                                RAFT_LOG("fail copy,can not commit");
                                this->mtx.unlock();
                                std::this_thread::sleep_for(std::chrono::milliseconds(10));  
                                break;
                            }
                        }
                         for(auto it:copy_missions){ 
                            append_entries_args<command> cparg;
                            CommitHelper chelp=this->commit_helpers[it.first];
                            append_entries_reply creply=chelp.reply;
                            bool haveLog; 
                            
                            //form the prev arg
                            if(creply.ask_begin_index<=0){
                                //if have snapshot, means don't have log
                                haveLog=!this->storage->haveSnapShot();
                                if(haveLog){
                                    cparg.prevLogIndex=0;
                                    cparg.prevLogTerm=0;
                                }else{
                                    cparg.Snapshot=this->storage->readSnapshot(cparg.haveSnapshot,cparg.prevLogIndex,cparg.prevLogTerm);
                                    RAFT_LOG("do not have log with index%d , send snapshot,snapshot size=%d,Latindex=%d,LastTerm=%d",creply.ask_begin_index,cparg.Snapshot.size(),cparg.prevLogIndex,cparg.prevLogTerm);
                                }
                                
                            }else{      
                                haveLog=this->storage->isExist(creply.ask_begin_index);
                                if(haveLog){
                                    log_entry<command> check_one=this->storage->readLog(creply.ask_begin_index);
                                    cparg.prevLogIndex=check_one.index;
                                    cparg.prevLogTerm = check_one.term;
                                }else{
                                    // this->storage->readSnapShotIndexAndTerm(cparg.prevLogIndex,cparg.prevLogTerm);
                                    int snapShortIndex;
                                    int snapShortTerm;
                                    this->storage->readSnapShotIndexAndTerm(snapShortIndex,snapShortTerm);
                                    if(snapShortIndex==creply.ask_begin_index){
                                        cparg.prevLogIndex=snapShortIndex;
                                        cparg.prevLogTerm =snapShortTerm;
                                    }else{
                                        //should send snapshort
                                        cparg.Snapshot=this->storage->readSnapshot(cparg.haveSnapshot,cparg.prevLogIndex,cparg.prevLogTerm);
                                    }
                                }
                            }
                            //get the folloe logs
                            std::vector<log_entry<command>> prev_logs;
                            if(!cparg.haveSnapshot){
                               prev_logs=this->storage->readLogs(creply.ask_begin_index+1,write_end_index);
                            }else{
                                
                               prev_logs= this->storage->readLogs(cparg.prevLogIndex+1,write_end_index);
                            }
                            cparg.term=this->current_term;
                            cparg.leaderId=this->my_id;
                            cparg.type=COPY_MISSION;
                            cparg.logNums=prev_logs.size();
                            RAFT_LOG("send to%d,log size:%d",it.first,prev_logs.size());
                            cparg.logs=prev_logs;
                            this->commit_helpers[it.first].lastjob_finished=false;
                            missions_args.insert(std::pair<int,append_entries_args<command>>(it.first,cparg));                           
                        }
                        if(begin_commit){
                            //write all the logs into storage
                            RAFT_LOG("begin leader's commit");
                            this->commitIndex=write_end_index;
                            this->storage->writeCommitIndex(this->commitIndex);
                            //add to missions_args
                            for(auto it:commit_missions){
                                append_entries_args<command> cmarg;
                                cmarg.term=this->current_term;
                                cmarg.leaderId=this->my_id;
                                cmarg.type=COMMIT_MISSION;
                                cmarg.commitIndex=this->commitIndex;
                                cmarg.logNums=0;//not important
                                missions_args.insert(std::pair<int,append_entries_args<command>>(it.first,cmarg));  
                            }
                            commit_missions.clear();
                        }
                    }
                    //rpc call
                    this->mtx.unlock();
                    for(auto it:missions_args){
                          this->thread_pool->addObjJob(this,&raft::send_append_entries,it.first,it.second);
                    }
                     std::this_thread::sleep_for(std::chrono::milliseconds(50));    
                    //this->mtx.lock();
                    if(can_quit){
                        //quit loop
                        RAFT_LOG("finish one commit");
                         //this->mtx.unlock(); 
                         break;
                    }
                    //this->mtx.unlock();                
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }    
    
    return;
}

template<typename state_machine, typename command>
void raft<state_machine, command>::run_background_apply() {
    // Apply committed logs the state machine
    // Work for all the nodes.

    // Hints: You should check the commit index and the apply index.
    //        Update the apply index and apply the log if commit_index > apply_index

    
    while (true) {
        if (is_stopped()) return;
        // Your code here:
        this->mtx.lock();
        if(this->lastApplied==this->commitIndex){
            this->mtx.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        RAFT_LOG("begin apply");
        // for(int i=lastApplied+1;i<=this->commitIndex;i++){
        //     log_entry<command> log=this->storage->readLog(i);
        //     RAFT_LOG("APPLY LOG VALUE=%d",log.c.value);
        //     state->apply_log(log.c);
        // }
        std::vector<log_entry<command>> logs=this->storage->readLogs(lastApplied+1,this->commitIndex);
        for(auto it:logs){
            state->apply_log(it.c);
            RAFT_LOG("apply log index:%d,value%d",it.index,it.c.value);
        }
        lastApplied=this->commitIndex;
        this->storage->writeAppliedIndex(this->lastApplied);
        this->mtx.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }    
    return;
}

template<typename state_machine, typename command>
void raft<state_machine, command>::run_background_ping() {
    // Send empty append_entries RPC to the followers.

    // Only work for the leader.
    append_entries_args<command> ping_args;
    while (true) {
        if (is_stopped()) return;
        // Your code here:
            #ifdef DEBUGLOCK
                RAFT_LOG("LOCK");
                this->mtx.lock();
            #else
            mtx.lock();
            #endif  //get the args needed
        // raft_role current_role=this->role;
        // int term = this->current_term;
        // int current_commitIndex = this->commitIndex;

        //     #ifdef DEBUGLOCK
        //         RAFT_LOG("UNLOCK");
        //         this->mtx.unlock();
        //     #else
        //     mtx.unlock();
        //     #endif
        // if(current_role!=leader){
        //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
        //     continue;
        // }
        // else{
            if(this->role!=leader){
                mtx.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }else{
            int myId=this->my_id;
            ping_args.term=this->current_term;
            ping_args.commitIndex=this->commitIndex;
            ping_args.type=HEART_BEAT_MISSION;
            ping_args.leaderId=this->my_id;
            ping_args.logNums=0;
            //initial pingResult
            for(int i=0;i<totle_num;i++){
                this->pingResult[i].return_state=HEART_BEAT_SUCCESS;
            }
            int Intevel=this->getPingInterVal(this->pingRound);
            pingRound++;
            mtx.unlock();
            //RAFT_LOG("ping send-term:%d,commitIndex:%d,type:%d,leaderId:%d",this->current_term,this->commitIndex,HEART_BEAT_MISSION,ping_args.leaderId);
            for(int i=0;i<totle_num;i++){
                if(i==myId) continue;
                this->thread_pool->addObjJob(this,&raft::send_append_entries,i,ping_args);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(Intevel)); //wait for handler
            //begin handle result
            #ifdef DEBUGLOCK
                RAFT_LOG("LOCK");
                this->mtx.lock();
            #else
            mtx.lock();
            #endif
            if(this->role!=leader) {
                mtx.unlock();
                continue;
            }
            int ping_fail_num=1;
            for(int i=0;i<totle_num;i++){
                if(i==this->my_id) continue;
                if(this->pingResult[i].return_state==FAIL_CONNECT) ping_fail_num++;
            }
            if(ping_fail_num==totle_num){
                can_recive_command=false;
                this->mtx.unlock();
                continue;
            }
            can_recive_command=true;
            for(int i=0;i<totle_num;i++){
                if(i==this->my_id) continue;
                if(this->pingResult[i].return_state==HEART_BEAT_SUCCESS||this->pingResult[i].return_state==FAIL_CONNECT) continue;
                RAFT_LOG("term too old, turn to follower");
                this->last_received_RPC_time=clock();
                this->timeInterVal=baseTime+my_id*timeChunk+this->getRandom()*timeChunk;
                this->role=follower;
                this->current_term=this->pingResult[i].term;
                this->votefor=-1;
                this->storage->writeCurrentTerm(this->current_term);
                this->storage->writeVotedFor(this->votefor);
                break;
            }
            #ifdef DEBUGLOCK
                RAFT_LOG("UNLOCK");
                this->mtx.unlock();
            #else
            mtx.unlock();
            #endif
        }
        //std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Change the timeout here!
    }    
    return;
}


/******************************************************************

                        Other functions

*******************************************************************/



#endif // raft_h