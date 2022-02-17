#include "raft_protocol.h"

marshall& operator<<(marshall &m, const request_vote_args& args) {
    m << args.my_term;
    m << args.candidateId;
    m << args.last_log_term;
    m << args.last_log_index;
    return m;
}
unmarshall& operator>>(unmarshall &u, request_vote_args& args) {
     u >> args.my_term;
    u >> args.candidateId;
    u >> args.last_log_term;
    u >> args.last_log_index;
    return u;
}

marshall& operator<<(marshall &m, const request_vote_reply& reply) {
    m << reply.voteGranted;
    m << reply.term;
    return m;
}

unmarshall& operator>>(unmarshall &u, request_vote_reply& reply) {
    u >> reply.voteGranted;
    u >> reply.term;
    return u;
}

marshall& operator<<(marshall &m, const append_entries_reply& reply) {
    // Your code here
    // m << reply.term;
    // m << reply.if_success;
    m << reply.term;
    m << reply.return_state;
    m << reply.ask_begin_index;
    return m;
}
unmarshall& operator>>(unmarshall &u, append_entries_reply& reply) {
    // Your code here
    // u >> reply.term;
    // u >> reply.if_success;
    u >> reply.term;
    u >> reply.return_state;
    u >> reply.ask_begin_index;
    return u;
}

marshall& operator<<(marshall &m, const install_snapshot_args& args) {
    // Your code here

    return m;
}

unmarshall& operator>>(unmarshall &u, install_snapshot_args& args) {
    // Your code here

    return u; 
}

marshall& operator<<(marshall &m, const install_snapshot_reply& reply) {
    // Your code here

    return m;
}

unmarshall& operator>>(unmarshall &u, install_snapshot_reply& reply) {
    // Your code here

    return u;
}