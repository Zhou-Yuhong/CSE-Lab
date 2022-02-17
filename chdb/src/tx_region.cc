#include "tx_region.h"
#include <mutex> 

int tx_region::put(const int key, const int val) {
    // TODO: Your code here
    //this->db->tx_id_mtx.lock();
    //get lock
if(!BIG_LOCK){
    if(!containKey(key)){
        int wait_round = 0;  // use wait_round to judge if dead_lock
        while(true){
            int lockOwner = this->db->tryLock(key,this->tx_id);
            if(lockOwner == this->tx_id){
                //successfully get the lock
                break;
            }
            else{               
                //can't get lock,may dead lock here
                if(wait_round>10){
                    //wait for than 10 rounds, can be judged as dead lock
                    //the older one should release the lock
                    if(this->tx_id > lockOwner){
                    //release all my previous locks, because the younger one may access for my lock
                    releaseAllLocks();
                    //try to get lock
                    return put(key,val);
                    }
                }

                wait_round++;
                //sleep for some time
                std::this_thread::sleep_for(std::chrono::milliseconds(15));

            }
        }
    }
} 
    this->read_only=false;
    int r;
    unsigned int queryKey = key >= 0 ? key : -key;
    this->commands.push_back(
        chdb_protocol::command(chdb_protocol::PUT_TYPE,key,val)
    );
    this->db->vserver->execute(queryKey,
        chdb_protocol::Put,
        chdb_protocol::operation_var{.tx_id = tx_id, .key = key, .value = val},
        r);
    //this->db->tx_id_mtx.unlock();    
    return r;
}

int tx_region::get(const int key) {
    // TODO: Your code here
    //this->db->tx_id_mtx.lock();
    if(!BIG_LOCK){
    if(!containKey(key)){
        //haven't get lock before
        // auto it = this->db->Locks.find(key);
        // if(it == this->db->Locks.end()){
        //     //not exist, insert lock
        //     this->db->Locks.emplace(key,new std::mutex);
        //     this->db->Locks.find(key)->second->lock();
        // }else{
        //     //alread exist and lock by others,wait
        //     it->second->lock();
        // }
        int wait_round = 0;  // use wait_round to judge if dead_lock
        while(true){
            int lockOwner = this->db->tryLock(key,this->tx_id);
            if(lockOwner == this->tx_id){
                //successfully get the lock
                break;
            }
            else{               
                //can't get lock,may dead lock here
                if(wait_round>10){
                    //wait for than 10 rounds, can be judged as dead lock
                    //the older one should release the lock
                    if(this->tx_id > lockOwner){
                    //release all my previous locks, because the younger one may access for my lock
                    releaseAllLocks();
                    //try to get lock
                    return get(key);
                    }
                }

                wait_round++;
                //sleep for some time
                std::this_thread::sleep_for(std::chrono::milliseconds(15));

            }
        }

    }
    }
    int r;
    unsigned int queryKey = key >=0 ? key : -key;
    this->commands.push_back(
        chdb_protocol::command(chdb_protocol::GET_TYPE,key)
    );
    this->db->vserver->execute(queryKey,
        chdb_protocol::Get,
        chdb_protocol::operation_var{.tx_id = tx_id, .key = key},
        r);
    //this->db->tx_id_mtx.unlock();
    return r;
}

void tx_region::rollback(){
    //roll back the command in commands
    int r;
    for(auto it:commands){
        this->db->vserver->rollback(
            it.key,
            chdb_protocol::Rollback,
            chdb_protocol::rollback_var{.tx_id = tx_id},
            r
        );
    }
    if(BIG_LOCK){
        this->db->tx_id_mtx.unlock();
    }
}

int tx_region::tx_can_commit() {
    // TODO: Your code here
    //check if all the commands have prepare
    if(this->read_only) return chdb_protocol::prepare_ok;
    int status=chdb_protocol::prepare_ok;
    for(auto it:commands){
        chdb_protocol::operation_var var;
        var.tx_id=this->tx_id;
        var.key = it.key;
        if(it.type==chdb_protocol::PUT_TYPE){
            var.value=it.val;
        }
        else{
            var.value=-999;// -999 means get operation
        }
        this->db->vserver->prepare(it.key,
            chdb_protocol::Prepare,
            var,status
        );
        if(status!=chdb_protocol::prepare_ok) break;
    }
    if(status == chdb_protocol::prepare_not_ok){
        //should rollback
        this->rollback();
    }
    return status;
}

int tx_region::tx_begin() {
    // TODO: Your code here
    if(BIG_LOCK){
        this->db->tx_id_mtx.lock();
    }
    printf("tx[%d] begin\n", tx_id);
    return 0;
}

int tx_region::tx_commit() {
    // TODO: Your code here
    printf("tx[%d] commit\n", tx_id);
    //commit all
    int r;
    for(auto it:this->commands){
        this->db->vserver->commit(
            it.key,
            chdb_protocol::Commit,
            chdb_protocol::commit_var{.tx_id=tx_id},
            r
        );
    }
    if(BIG_LOCK){
        this->db->tx_id_mtx.unlock();
    }
    return 0;
}

int tx_region::tx_abort() {
    // TODO: Your code here
    printf("tx[%d] abort\n", tx_id);
    return 0;
}
