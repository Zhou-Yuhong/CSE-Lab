#include "ch_db.h"
#include <dirent.h>
int view_server::execute(unsigned int query_key, unsigned int proc, const chdb_protocol::operation_var &var, int &r) {
    // TODO: Your code here
    chdb_command::command_type cmd_type;
    if(proc == chdb_protocol::Put){
        cmd_type = chdb_command::CMD_PUT;
    }else if(proc == chdb_protocol::Get){
        cmd_type = chdb_command::CMD_GET;
    }
    int chdbVar = var.value;
    chdb_command chdb_cmd(cmd_type,var.key,chdbVar,var.tx_id);
    int term =0;
    int index=0;
    
    if(RAFT_GROUP){
        int leader = this->raft_group->check_exact_one_leader();
        this->raft_group->nodes[leader]->new_command(chdb_cmd,term,index);
    
    //sleep some time for command to apply
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    
    // std::unique_lock<std::mutex> lock(chdb_cmd.res->mtx);
    // while(!chdb_cmd.res->done){
    //     if (chdb_cmd.res->cv.wait_until(lock, std::chrono::system_clock::now() + std::chrono::milliseconds(2500)) ==
    //             std::cv_status::timeout) {
    //             printf("Timeout!\n");
    //         }
    // }
    //int wait_round=0;
    // while(true){
    //     //get lock and check
    //     std::this_thread::sleep_for(std::chrono::milliseconds(30));
    //     if(wait_round>=15){
    //         //at most wait 20 rounds
    //         return 0;    //?
    //     }
    //     chdb_cmd.res->mtx.lock();
    //     if(chdb_cmd.res->done){
    //         chdb_cmd.res->mtx.unlock();
    //         break;
    //     }else{
    //         wait_round++;
    //          chdb_cmd.res->mtx.unlock();
    //     }
    // }

    int base_port = this->node->port();
    int shard_offset = this->dispatch(query_key, shard_num());

    return this->node->template call(base_port + shard_offset, proc, var, r);
}
int view_server::rollback(unsigned int query_key, unsigned int proc,const chdb_protocol::rollback_var &var,int &r){
    int base_port = this->node->port();
    int shard_offset = this->dispatch(query_key,shard_num());

    return this->node->template call(base_port + shard_offset, proc, var, r);
}
int view_server::prepare(unsigned int query_key, unsigned int proc, const chdb_protocol::operation_var &var, int &r) {
    // TODO: Your code here
    int base_port = this->node->port();
    int shard_offset = this->dispatch(query_key, shard_num());

    return this->node->template call(base_port + shard_offset, proc, var, r);
}

int view_server::commit(unsigned int query_key,unsigned int proc,const chdb_protocol::commit_var &var,int &r){
    int base_port = this->node->port();
    int shard_offset = this->dispatch(query_key,shard_num());  

    return this->node->template call(base_port + shard_offset, proc, var, r);  
}
view_server::~view_server() {
    #if RAFT_GROUP
        delete this->raft_group;
        rmdir("raft_temp");
    #endif
        delete this->node;
}
int chdb::tryLock(int key,int tx_id){
    //return the tx_id that hold the lock
    std::unique_lock<std::mutex> lock(tx_id_mtx);
    auto it =this->Locks.find(key);
    if(it == this->Locks.end()){
        //not exist, insert lock
        this->Locks.emplace(key,new std::mutex);
        this->Locks.find(key)->second->lock();
        //insert keyToTx_id
        this->insertKeyToTx(key,tx_id);
        return tx_id;
    }else{
        //alread exist, 
        if(it->second->try_lock()){
            //successfully get lock
            this->insertKeyToTx(key,tx_id);
            return tx_id;
        }
        else{
            //fail to get lock,return the tx_id that hold the lock
            auto iter = this->keyToTx.find(key);
            return iter->second;
        }
    }
}
int chdb::releaseLock(int key,int tx_id){
    std::unique_lock<std::mutex> lock(tx_id_mtx);
    if(this->keyToTx.find(key)==this->keyToTx.end()){
        return tx_id;
    }
    if(this->keyToTx.find(key)->second!=tx_id){
        return tx_id;
    }
    auto it = this->Locks.find(key);
    it->second->unlock();
    this->keyToTx.erase(key);
    return tx_id;
}