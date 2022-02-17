#ifndef raft_protocol_h
#define raft_protocol_h

#include "rpc.h"
#include "raft_state_machine.h"
enum raft_rpc_opcodes {
    op_request_vote = 0x1212,
    op_append_entries = 0x3434,
    op_install_snapshot = 0x5656
};

enum raft_rpc_status {
   OK,
   RETRY,
   RPCERR,
   NOENT,
   IOERR
};
class request_vote_args {
public:
    // Your code here
    int my_term;
    int candidateId;
    int last_log_term;
    int last_log_index;
    request_vote_args(int my_term,int last_log_term,int candidateId,int last_log_index){
        this->my_term=my_term;
        this->candidateId=candidateId;
        this->last_log_term=last_log_term;
        this->last_log_index=last_log_index;
    }
    request_vote_args(){}
};

marshall& operator<<(marshall &m, const request_vote_args& args);
unmarshall& operator>>(unmarshall &u, request_vote_args& args);


class request_vote_reply {
public:
    // Your code here
    int voteGranted;    //0 means not vote, and can still be cadidate,1 means not vote, but you can't be candidate,2 means vote
    int term;
    request_vote_reply(int voteGranted, int term){
        this->voteGranted=voteGranted;
        this->term = term;
    }
    request_vote_reply(){}
};

marshall& operator<<(marshall &m, const request_vote_reply& reply);
unmarshall& operator>>(unmarshall &u, request_vote_reply& reply);

template<typename command>
class log_entry {
public:
    // Your code here
    int index; 
    int term;
    command c;
    log_entry(int index,int term,command c){
        this->index=index;
        this->term=term;
        this->c=c;
    }
    log_entry(){}
};

template<typename command>
marshall& operator<<(marshall &m, const log_entry<command>& entry) {
    // Your code here
    m<<entry.index;
    m<<entry.term;
    m<<entry.c;
    // int size=entry.c.size();
    // char* buf = new char[size];
    // entry.c.serialize(buf,size);
    // m<<size;
    // for(int i=0;i<size;i++){
    //     m<<buf[i];
    // }
    return m;
}

template<typename command>
unmarshall& operator>>(unmarshall &u, log_entry<command>& entry) {
    // Your code here
    u>>entry.index;
    u>>entry.term;
    u>>entry.c;
    // int size;
    // u>>size;
    // char* buf=new char[size];
    // for(int i=0;i<size;i++){
    //     u>>buf[i];
    // }
    // command c;
    // c.deserialize(buf,size);
    // entry.c=c;
    return u;
}

template<typename command>
class append_entries_args {
public:
    // Your code here
    int term;
    int leaderId;
    int type;   //means the request type: HEART_BEAT_MISSION,COPY_MISSION,COMMIT_MISSION
    int prevLogIndex;
    int prevLogTerm;
    int commitIndex;
    bool haveSnapshot=false;
    int logNums;   //
    std::vector<log_entry<command>> logs;
    // log_entry<command> logs[200];  //max num is 200
    std::vector<char> Snapshot;
};

template<typename command>
marshall& operator<<(marshall &m, const append_entries_args<command>& args) {
    // Your code here
    m << args.term;
    m << args.leaderId;
    m << args.type;
    m << args.prevLogIndex;
    m << args.prevLogTerm;
    m << args.commitIndex;
    m << args.haveSnapshot;
    m << args.logNums;
    m << args.logs;
    m << args.Snapshot;
    return m;
}

template<typename command>
unmarshall& operator>>(unmarshall &u, append_entries_args<command>& args) {
    // Your code here
    u >> args.term;
    u >> args.leaderId;
    u >> args.type;
    u >> args.prevLogIndex;
    u >> args.prevLogTerm;
    u >> args.commitIndex;
    u >> args.haveSnapshot;
    u >> args.logNums;
    u >> args.logs;
    u >> args.Snapshot;
    return u;
}

class append_entries_reply {
public:
    // Your code here
    int term;
    int return_state;  //HEART_BEAT_SUCCESS,APPEND_FAIL_AND_BE_FOLLOWER,APPEND_FAIL_AND_ASK_LOGS,FINISH_COPY,FINISH_COMMIT
    int ask_begin_index; //to ask the leader the log begin from ask_begin_index
};

marshall& operator<<(marshall &m, const append_entries_reply& reply);
unmarshall& operator>>(unmarshall &u, append_entries_reply& reply);


class install_snapshot_args {
public:
    // Your code here
};

marshall& operator<<(marshall &m, const install_snapshot_args& args);
unmarshall& operator>>(unmarshall &m, install_snapshot_args& args);


class install_snapshot_reply {
public:
    // Your code here
};

marshall& operator<<(marshall &m, const install_snapshot_reply& reply);
unmarshall& operator>>(unmarshall &m, install_snapshot_reply& reply);


#endif // raft_protocol_h