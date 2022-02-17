#include "chdb_state_machine.h"

chdb_command::chdb_command():chdb_command(CMD_NONE,0,0,0) {
    // TODO: Your code here
}

chdb_command::chdb_command(command_type tp, const int &key, const int &value, const int &tx_id)
        : cmd_tp(tp), key(key), value(value), tx_id(tx_id) {
    // TODO: Your code here
    this->res=std::make_shared<result>();
    this->res->start = std::chrono::system_clock::now();
    this->res->key = key;

}

chdb_command::chdb_command(const chdb_command &cmd) :
        cmd_tp(cmd.cmd_tp), key(cmd.key), value(cmd.value), tx_id(cmd.tx_id), res(std::move(cmd.res)) {
    // TODO: Your code here

}


void chdb_command::serialize(char *buf, int size) const {
    // TODO: Your code here
    memcpy(buf,&(this->cmd_tp),sizeof(int));
    memcpy(buf+4,&(this->tx_id),sizeof(int));
    memcpy(buf+8,&(this->key),sizeof(int));
    memcpy(buf+12,&(this->value),sizeof(int));
}

void chdb_command::deserialize(const char *buf, int size) {
    // TODO: Your code here
    memcpy(&(this->cmd_tp),buf,sizeof(int));
    memcpy(&(this->tx_id),buf+4,sizeof(int));
    memcpy(&(this->key),buf+8,sizeof(int));
    memcpy(&(this->value),buf+12,sizeof(int));

}

marshall &operator<<(marshall &m, const chdb_command &cmd) {
    // TODO: Your code here
    int type = cmd.cmd_tp;
    m << type;
    m << cmd.tx_id;
    m << cmd.key;
    m << cmd.value;
    return m;
}

unmarshall &operator>>(unmarshall &u, chdb_command &cmd) {
    // TODO: Your code here
    int type;
    u >> type;
    u >> cmd.tx_id;
    u >> cmd.key;
    u >> cmd.value;
    cmd.cmd_tp=chdb_command::command_type(type);
    return u;
}

void chdb_state_machine::apply_log(raft_command &cmd) {
    // TODO: Your code here
    chdb_command &chdb_cmd = dynamic_cast<chdb_command &>(cmd);
    printf("key:%d value:%d txid:%d \n",chdb_cmd.key,chdb_cmd.value,chdb_cmd.tx_id,chdb_cmd);
    // chdb_cmd.res->mtx.lock();
    std::unique_lock<std::mutex> lock(chdb_cmd.res->mtx);
    std::map<int,int>::iterator it;
    switch (chdb_cmd.cmd_tp)
    {
    case chdb_command::CMD_PUT:
        it = this->store.find(chdb_cmd.key);
        if(it == this->store.end()){
            //not exist, should insert
            this->store.insert(
                std::pair<int,int>(chdb_cmd.key,chdb_cmd.value)
            );
        }else{
            it->second = chdb_cmd.value;
        }
        chdb_cmd.res->key = chdb_cmd.key;
        chdb_cmd.res->value = chdb_cmd.value;
        chdb_cmd.res->tp=chdb_cmd.cmd_tp;
        chdb_cmd.res->tx_id=chdb_cmd.tx_id;
        break;
    case chdb_command::CMD_GET:
        it = this->store.find(chdb_cmd.key);
        if(it == this->store.end()){
            //not set the value?
        }
        else{
            chdb_cmd.value = it->second;
        }
        chdb_cmd.res->tx_id=chdb_cmd.tx_id;
        chdb_cmd.res->key=chdb_cmd.key;
        chdb_cmd.res->tp=chdb_cmd.cmd_tp;
        break;
    case chdb_command::CMD_NONE:
        //do nothing
        chdb_cmd.res->tx_id=chdb_cmd.tx_id; 
        chdb_cmd.res->tp=chdb_cmd.cmd_tp;
        break;   
    default:
        break;
    }
    chdb_cmd.res->done = true;
    chdb_cmd.res->cv.notify_all();
    return;
}