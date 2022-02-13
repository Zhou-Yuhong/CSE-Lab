#include "shard_client.h"


int shard_client::put(chdb_protocol::operation_var var, int &r) {
    // TODO: Your code here
    int tx_id = var.tx_id;
    int key = var.key;
    int value = var.value;
    int oldvar = failNum;
    for(int i=0;i<this->replica_num;i++){
        if(store[i].find(key)==store[i].end()){
            //not exist, should insert
            store[i].insert(
                std::pair<int,value_entry>(key,value_entry(value))
            );
        }else{
            //exist should modify
            auto iter = store[i].find(key);
            oldvar = iter->second.value;
            iter->second = value_entry(value);
        }
    }
    //insert into commands
    if(this->commands.find(tx_id)==commands.end()){
        //not exist, should insert
        std::vector<chdb_protocol::command> newCommands;
        chdb_protocol::command c(chdb_protocol::PUT_TYPE,key,value,oldvar);
        newCommands.push_back(c);
        commands.insert(std::pair<int,std::vector<chdb_protocol::command>>(tx_id,newCommands));
    }else{
        //exist, just push_back
        auto vec = this->commands.find(tx_id);
        chdb_protocol::command c(chdb_protocol::PUT_TYPE,key,value,oldvar);
        vec->second.push_back(c);
    }
    r = value;
    return 0;
}

bool shard_client::getMajorNum(int *group,int &result){
    //map value to nums
    std::map<int,int> helper;
    for(int i=0;i<replica_num;i++){
        if(helper.find(group[i])==helper.end()){
            //not exist, should insert
            helper.insert(
                std::pair<int,int>(group[i],1)
            );
        }
        else{
            //exist should add the nums
            auto it = helper.find(group[i]);
            it->second = it->second+1;
        }
    }
    //judge if have result
    bool flag = false;
    for(auto it:helper){
        if(it.second*2 > replica_num){
            flag = true;
            result = it.first;
        }
    }
    if(result==failNum) flag = false;
    return flag;
}

int shard_client::get(chdb_protocol::operation_var var, int &r) {
    // TODO: Your code here
    //if fail, return -999 ?
    int key = var.key;
    int tx_id = var.tx_id;
    static int* result = nullptr;
    if(!result){
        result = new int[replica_num];
    }else{
        for(int i=0;i<replica_num;i++)
        result[i] = failNum;
    }
    for(int i=0;i<replica_num;i++){
        if(store[i].find(key)!=store[i].end()){
            result[i] = store[i].find(key)->second.value;
        }
    }
    // //find the major num
    // getMajorNum(result,r);
    r = failNum;
    for(int i=0;i<replica_num;i++){
        if(result[i]!=failNum){
            r = result[i];
            break;
        }
    }
     if(this->commands.find(tx_id)==commands.end()){
        //not exist, should insert
        std::vector<chdb_protocol::command> newCommands;
        chdb_protocol::command c(chdb_protocol::GET_TYPE,key);
        newCommands.push_back(c);
        commands.insert(std::pair<int,std::vector<chdb_protocol::command>>(tx_id,newCommands));
    }else{
        //exist, just push_back
        auto vec = this->commands.find(tx_id);
        chdb_protocol::command c(chdb_protocol::GET_TYPE,key);
        vec->second.push_back(c);
    }
    return 0;
}

int shard_client::commit(chdb_protocol::commit_var var, int &r) {
    // TODO: Your code here
    if(this->commands.find(var.tx_id)!=this->commands.end()){
        //remove this tx_id logs
        this->commands.erase(var.tx_id);
    }
    return 0;
}

void shard_client::rollbackvar(int key,int oldvar){
    //if oldvar == -999,means no oldvar
    for(int i=0;i<replica_num;i++){
        auto iter = this->store[i].find(key);
        if(iter == this->store[i].end()) continue;
        if(oldvar == failNum){
            this->store[i].erase(iter);
        }else{
            iter->second=oldvar;
        }
        // auto singleStore = this->store[i];
        // auto iter = singleStore.find(key);
        // if(iter == singleStore.end()) continue;
        // if(oldvar == failNum){
            
        // }
    }
}

int shard_client::rollback(chdb_protocol::rollback_var var, int &r) {
    // TODO: Your code here
    auto comvec = this->commands.find(var.tx_id);
    if(comvec == this->commands.end()){
        //already rollback
        return 0;
    }
    std::vector<chdb_protocol::command> coms=comvec->second;
    //remove
    this->commands.erase(comvec);
    //undo command
    int size = coms.size();
    for(int i=size-1;i>=0;i--){
        //Loop from end to begin and roll back
        chdb_protocol::command item = coms[i];
        if(item.type==chdb_protocol::PUT_TYPE){
            this->rollbackvar(item.key,item.oldvar);
        }    
    }
    return 0;
}

int shard_client::check_prepare_state(chdb_protocol::check_prepare_state_var var, int &r) {
    // TODO: Your code here
    return 0;
}

int shard_client::prepare(chdb_protocol::operation_var var, int &r) {
    // TODO: Your code here
    if(!this->active){
        r = chdb_protocol::prepare_not_ok;
        return 0;
    }
    int tx_id = var.tx_id;
    auto comvec = this->commands.find(tx_id);
    if(comvec == this->commands.end()){
        r = chdb_protocol::prepare_not_ok;
        return 0;        
    }
    //check if the command exist
    bool flag=false;
    for(auto it:comvec->second){
        if(it.key==var.key){
            if(it.val==var.value){
                flag = true;
                break;
            }
        }
    }
    if(flag){
        r = chdb_protocol::prepare_ok;
    }
    else{
        r = chdb_protocol::prepare_not_ok;
    }
    return 0;
}

shard_client::~shard_client() {
    delete node;
}