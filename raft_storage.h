#ifndef raft_storage_h
#define raft_storage_h

#include "raft_protocol.h"
#include <fcntl.h>
#include <mutex>
#include <fstream>
#include <cstdio>
#include <vector>
#include <map>
#include <stdio.h>
template<typename command>
class raft_storage {
public:
    raft_storage(const std::string& file_dir);
    ~raft_storage();
    // Your code here
    void writeLog(log_entry<command> log);
    //void deleteLog(int index); //insert a special log,which means delete
    //the raftInfo file structure is currentTerm|voteFor|CommitIndex|Applied|last_log_index|last_log_term
    void writeCurrentTerm(int currentTerm);
    bool haveSnapShot();
    void writeVotedFor(int voteFor);
    void writeCommitIndex(int commitIndex);
    void writeAppliedIndex(int appliedIndex);
    void writeLastIndexAndLastTerm(int last_log_index,int last_log_term);
    void readAll(int &currentTerm,int &voteFor,int &commitIndex,int &appiledIndex,int &last_log_index,int&last_log_term);
    void writeDebug(std::string s);
    void readDebug(std::string s);
    bool isExist(int index);
    //called after checkSuccess, write logs(if the log already exist ,moved on)
    //void writeLogs(std::vector<log_entry<command>> logs);
    log_entry<command> readLog(int index);
    std::vector<log_entry<command>> readLogs(int begin,int end);
    std::map<int,int> indexes;   // log_index map offset
    std::map<int,log_entry<command>> cache;
    int last_appiled_index=0;
    //void writeLogs(std::vector<log_entry<command>>logs);

    //about snapshot,the file content is last included index,last included term,sizeofsnapshot,snapshot
    std::vector<char> readSnapshot(bool &success,int &lastIncludeIndex,int &lastIncludeTerm);
    bool readSnapShotIndexAndTerm(int &lastIncludeIndex,int &lastIncludeTerm);
    //rewrite the whole data.log,remake the indexes
    void makeSnapshot(int endIndex,std::vector<char> snapshot);
    //apply come snapshot--the snapshot come from leader, should delete the indexes,data,num,rewrite snapshot and metadata
    void applyComeSnapshot(int endIndex,int endTerm,std::vector<char> snapshot);

private:
    int totleNum=0;
    std::string dir;
    std::string data_dir; // the file that store the log
    std::string raftInfo_dir; //the file that store the other message
    std::string NumFire;
    std::string SnapShortFile;
    std::mutex mtx;
};
template<typename command>
void raft_storage<command>::writeDebug(std::string s){
    std::ofstream writeDEBUG(this->dir+"/write.txt",std::ios::app);
    writeDEBUG<<"time: "<<clock()<<" "<<s<<std::endl;
    writeDEBUG.close();
}
template<typename command>
void raft_storage<command>::readDebug(std::string s){
    std::ofstream readDEBUG(this->dir+"/read.txt",std::ios::app);
    readDEBUG <<"time:"<<clock()<<" "<<s<<std::endl;
    readDEBUG.close();
}

template<typename command>
raft_storage<command>::raft_storage(const std::string& dir){
    // Your code here
    this->dir=dir;
    this->data_dir=dir+"/"+"data.log";
    this->raftInfo_dir=dir+"/raftInfo.log";
    this->NumFire=dir+"/totleNum.txt";
    this->SnapShortFile=dir+"/snapshot";
    //get the totle num of logFile
    std::ifstream NUM(this->NumFire);
    if(NUM){
       NUM>>this->totleNum;
       NUM.close(); 
    }
    //judge if the file exist
    std::ifstream raftInfoIn(this->raftInfo_dir,std::ios::in|std::ios::binary);
    if(!raftInfoIn){
    raftInfoIn.close();
    //initial
    std::ofstream out(this->raftInfo_dir,std::ios::out|std::ios::binary);
    int initialVal=0;
    int votefor=-1;
    out.write((char *)&initialVal,sizeof(int));
    out.write((char *)&votefor,sizeof(int));
    out.write((char *)&initialVal,sizeof(int));
    out.write((char *)&initialVal,sizeof(int));
    out.write((char *)&initialVal,sizeof(int));
    out.write((char *)&initialVal,sizeof(int));
    out.close();
    }else{
        int initialVal=0;
        raftInfoIn.read((char *)&initialVal,sizeof(int));
        raftInfoIn.read((char *)&initialVal,sizeof(int));
        raftInfoIn.read((char *)&initialVal,sizeof(int));
        raftInfoIn.read((char *)&initialVal,sizeof(int));
        raftInfoIn.read((char *)&(this->last_appiled_index),sizeof(int));
        raftInfoIn.close();
    }
    //form the indexes
    std::ifstream in(this->data_dir,std::ios::in|std::ios::binary);
    if(in){
        //readDebug("data file exist,initial");
        char buf[2048];
        for(int i=0;i<this->totleNum;i++){
            int size;
            int index;
            int term;
            int pos=in.tellg();
            in.read((char *)&index,sizeof(int));
            in.read((char *)&term,sizeof(int));
            in.read((char *)&size,sizeof(int));
            //readDebug("initial data read index:"+std::to_string(index)+" term"+std::to_string(term)+" size"+std::to_string(size)+" pos"+std::to_string(pos));
            // if(term==-1){
            //     this->indexes.erase(index);
            //     continue;
            // } 
            if(index>this->last_appiled_index){
                continue;
            }
            in.read((char*)buf,size);
            log_entry<command> log;
    
            log.index=index;
            log.term=term;
            log.c.deserialize(buf,size);
            std::map<int,int>::iterator it;
            typename std::map<int,log_entry<command>>::iterator cacheIt;
            cacheIt=cache.find(index);
            it = indexes.find(index);
            if(it == indexes.end()) indexes.insert(std::make_pair(index,pos));
            else{
                it->second = pos;
            }
            if(cacheIt==cache.end()) cache.insert(std::make_pair(index,log));
            else{
                cacheIt->second = log;
            }
        }
        in.close();
    }

    
}
template<typename command>
void raft_storage<command>::readAll(int &currentTerm,int &voteFor,int &commitIndex,int &appiledIndex,int &last_log_index,int&last_log_term){
    std::ifstream in(this->raftInfo_dir,std::ios::in|std::ios::binary);
    if(!in){
        //do not have raftInfo
        return;
    }else{
        in.read((char*)&currentTerm,sizeof(int));
        in.read((char*)&voteFor,sizeof(int));
        in.read((char*)&commitIndex,sizeof(int));
        in.read((char*)&appiledIndex,sizeof(int));
        in.read((char*)&last_log_index,sizeof(int));
        in.read((char*)&last_log_term,sizeof(int));
        in.close();
        return;
    }

}
template<typename command>
void raft_storage<command>::writeLog(log_entry<command> log){
    // std::string log_dst=this->dir+"/"+std::to_string(log.index)+".log";
    // std::ofstream out(log_dst,std::ios::out|std::ios::binary);
    // out.write((char *)&log,sizeof(log));
    //based on the index,find the place and write
    // std::fstream out(this->data_dir,std::ios::out|std::ios::binary|std::ios::in);
    //write and update the indexes
    std::ofstream out(this->data_dir,std::ios::app|std::ios::binary);
    int pos=out.tellp();
    char buf[2048];
    int size = log.c.size();
    log.c.serialize(buf,size);
    out.write((const char *)&log.index,sizeof(int));
    out.write((const char*)&log.term,sizeof(int));
    out.write((const char*)&size,sizeof(int));
    out.write((const char *)buf,size);
    //writeDebug("write file index="+std::to_string(log.index)+" term="+std::to_string(log.term)+" buf_size="+std::to_string(size)+" begin pos="+std::to_string(pos));    
    std::map<int,int>::iterator it;
    it = indexes.find(log.index);
    if(it == indexes.end()) indexes.insert(std::make_pair(log.index,pos));
    else{
        it->second = pos;
    }
    typename std::map<int,log_entry<command>>::iterator cacheIt;
    cacheIt = cache.find(log.index);
    if(cacheIt == cache.end()) cache.insert(std::make_pair(log.index,log));
    else{
        cacheIt->second=log;
    }
    out.close();
    this->totleNum++;
    std::ofstream NUM(this->NumFire);
    NUM<<this->totleNum;
    NUM.close();
}
template<typename command>
void raft_storage<command>::writeCurrentTerm(int currentTerm){
    // std::ofstream out(this->raftInfo_dir,std::ios::out|std::ios::binary);
    // out.write((char *)&currentTerm,sizeof(int));
    // out.close();
    std::fstream out(this->raftInfo_dir,std::ios::out|std::ios::binary|std::ios::in);
    out.write((const char*)&currentTerm,sizeof(int));
    out.close();
}
template<typename command>
bool raft_storage<command>::isExist(int index){
      std::map<int,int>::iterator it=this->indexes.find(index);
      if(it==indexes.end()){
          return false;
      }
      else{
          return true;
      }
}
template<typename command>
void raft_storage<command>::writeVotedFor(int voteFor){
    std::fstream out(this->raftInfo_dir,std::ios::out|std::ios::binary|std::ios::in);
    out.seekp(sizeof(int),std::ios::beg);
    out.write((char*)&voteFor,sizeof(int));
    out.close();
}
template<typename command>
void raft_storage<command>::writeCommitIndex(int commitIndex){
    std::fstream out(this->raftInfo_dir,std::ios::out|std::ios::binary|std::ios::in);
    out.seekp(2*sizeof(int),std::ios::beg);
    out.write((char*)&commitIndex,sizeof(int));
    out.close();
}
template<typename command>
void raft_storage<command>::writeAppliedIndex(int appliedIndex){
    std::fstream out(this->raftInfo_dir,std::ios::out|std::ios::binary|std::ios::in);
    out.seekp(3*sizeof(int),std::ios::beg);
    out.write((char*)&appliedIndex,sizeof(int));
    out.close();
}
template<typename command>
void raft_storage<command>::writeLastIndexAndLastTerm(int last_log_index,int last_log_term){
    std::fstream out(this->raftInfo_dir,std::ios::out|std::ios::binary|std::ios::in);
    out.seekp(4*sizeof(int),std::ios::beg);
    out.write((char*)&last_log_index,sizeof(int));
    out.write((char*)&last_log_term,sizeof(int));
    out.close();
}
// template<typename command>
// void raft_storage<command>::writeLogs(std::vector<log_entry<command>> logs){
//     if(logs.empty()) {
    
//         return;
//     }
//     std::fstream out(this->data_dir,std::fstream::out|std::fstream::binary|std::fstream::in);
//     int offset=(logs[0].index-1)*sizeof(log_entry<command>);
//     out.seekp(offset,std::ios::beg);
//     for(int i=0;i<logs.size();i++){
//         out.write((char*)&(logs[i]),sizeof(log_entry<command>));
//     }
//     out.close();
        
// }

template<typename command>
log_entry<command> raft_storage<command>::readLog(int index){
    log_entry<command> log;
    //first find in cache
    if(this->cache.find(index)!=this->cache.end()){
        return this->cache.find(index)->second;
    }
    std::ifstream in(this->data_dir,std::ios::in|std::ios::binary);
    int pos=this->indexes.find(index)->second;
    //("read index:"+std::to_string(index)+" begin pos:"+std::to_string(pos));
    in.seekg(pos,std::ios::beg);
    int indexGet;
    int termGet;
    int sizeGet;
    char buf[2048];
    in.read((char *)&indexGet,sizeof(int));
    in.read((char *)&termGet,sizeof(int));
    in.read((char*)&sizeGet,sizeof(int));
    in.read((char *)buf,sizeGet);
    log.index=indexGet;
    log.term=termGet;
    log.c.deserialize(buf,sizeGet);
    in.close();
    return log;

    return log;
}
//read index from [begin,end]
template<typename command>
std::vector<log_entry<command>> raft_storage<command>::readLogs(int begin,int end){


    std::vector<log_entry<command>> result;
    if(begin>end) return result;
    for(int i=begin;i<=end;i++){
        result.push_back(this->readLog(i));
    }
    return result;
    // char buf[2048];
    // std::ifstream in(this->data_dir,std::ios::in|std::ios::binary);
    // for(int i=begin;i<=end;i++){
    //    std::map<int,int>::iterator it;
    //    int size;
    //    int index;
    //    int term;
    //    it=this->indexes.find(i);
    //    int pos=it->second;
    //    in.seekg(pos,std::ios::beg);
    //    in.read((char*)&index,sizeof(int));
    //    in.read((char*)&term,sizeof(int));
    //    in.read((char*)&size,sizeof(int));
    //    in.read((char*)buf,size);
    //    log_entry<command> log;
    //    log.index=index;
    //    log.term=term;
    //    log.c.deserialize(buf,size);
    //    result.push_back(log);
    // }
    // return result;

    
}
//about snap short
template<typename command>
std::vector<char> raft_storage<command>::readSnapshot(bool &success,int &lastIncludeIndex,int &lastIncludeTerm){
    std::ifstream in(this->SnapShortFile,std::ios::in|std::ios::binary);
    std::vector<char> result;
    if(!in){
        success=false;
        return result;
    }
    in.read((char*)&lastIncludeIndex,sizeof(int));
    in.read((char*)&lastIncludeTerm,sizeof(int));
    int size;
    in.read((char*)&size,sizeof(int));
    char *buf=new char[size];
    in.read(buf,size);
    for(int i=0;i<size;i++){
        result.push_back(buf[i]);
    }
    success=true;
    delete buf;
    return result;
}
template<typename command>
bool raft_storage<command>::readSnapShotIndexAndTerm(int &lastIncludeIndex,int &lastIncludeTerm){
    std::ifstream in(this->SnapShortFile,std::ios::in|std::ios::binary);
    if(!in){
        return false;
    }
    in.read((char*)&lastIncludeIndex,sizeof(int));
    in.read((char*)&lastIncludeTerm,sizeof(int));
    return true;
}
//form the snapshotfile,reform  data.log , NumFile and indexes
template<typename command>
void raft_storage<command>::makeSnapshot(int endIndex,std::vector<char> snapshot){
    std::string snapshotTempName=this->SnapShortFile+".new";
    std::string dataTempName=this->data_dir+".new";
    std::ofstream SnapShotWrite(snapshotTempName,std::ios::out|std::ios::binary);
    std::map<int,int> newIndexes;
    std::map<int,log_entry<command>> newCache;
    //get the endIndex's term
    log_entry<command> log=this->readLog(endIndex);
    int term = log.term;
    int index = log.index;
    //write the snapshot.bat first
    int size=snapshot.size();
    SnapShotWrite.write((char*)&index,sizeof(int));
    SnapShotWrite.write((char*)&term,sizeof(int));
    SnapShotWrite.write((char*)&size,sizeof(int));
    char* s_buf=new char[size]; 
    for(int i=0;i<size;i++){
        s_buf[i]=snapshot[i];
    }
    SnapShotWrite.write(s_buf,size);
    delete s_buf;
    //finish the snapshot.new write
    //form the new indexes,new data.log,Num file
    int iter=endIndex+1;
    int num=0;
    std::ifstream in(this->data_dir,std::ios::in|std::ios::binary);
    std::map<int,int>::iterator it=this->indexes.find(iter);
    bool haveRemain=false;
    std::ofstream out;
    if(it!=indexes.end()){
        haveRemain=true;
        out.open(dataTempName,std::ios::out|std::ios::binary);
    }
    while(it!=indexes.end()){
        //log_entry<command> log;//the log read
        int readPos=it->second;
        in.seekg(readPos,std::ios::beg);
        int indexGet;
        int termGet;
        int sizeGet;
        char buf[2048];
        in.read((char *)&indexGet,sizeof(int));
        in.read((char *)&termGet,sizeof(int));
        in.read((char*)&sizeGet,sizeof(int));
        in.read((char *)buf,sizeGet);
        // log.index=indexGet;
        // log.term=termGet;
        // log.c.deserialize(buf,sizeGet);
        //write log into new datafile
        int writePos=out.tellp();
        out.write((const char *)&indexGet,sizeof(int));
        out.write((const char*)&termGet,sizeof(int));
        out.write((const char*)&sizeGet,sizeof(int));
        out.write((const char *)buf,sizeGet);
        newIndexes.insert(std::make_pair(it->first,writePos));
        newCache.insert(std::make_pair(it->first,this->cache.find(it->first)->second));
        it=indexes.find(it->first+1);
        num++;
    }
    //rename and delete part
    std::remove(this->SnapShortFile.c_str());
    
    std::rename(snapshotTempName.c_str(),this->SnapShortFile.c_str());
    this->indexes.clear();
    this->indexes=newIndexes;
    this->cache.clear();
    this->cache=newCache;
    std::remove(this->data_dir.c_str());
    if(haveRemain){
        std::rename(dataTempName.c_str(),this->data_dir.c_str());
    }
    this->totleNum=num;
    std::ofstream numWriter(this->NumFire);
    numWriter<<this->totleNum;
    numWriter.close();
    SnapShotWrite.close();
    in.close();
    out.close();
}

template<typename command>
void raft_storage<command>::applyComeSnapshot(int endIndex,int endTerm,std::vector<char> snapshot){
    std::string snapshotTempName=this->SnapShortFile+".new";
    std::string dataTempName=this->data_dir+".new";
    std::ofstream SnapShotWrite(snapshotTempName,std::ios::out|std::ios::binary);
    this->indexes.clear();
    int size=snapshot.size();
    SnapShotWrite.write((char*)&endIndex,sizeof(int));
    SnapShotWrite.write((char*)&endTerm,sizeof(int));
    SnapShotWrite.write((char*)&size,sizeof(int));
    char* buf=new char[size];
    for(int i=0;i<size;i++){
        buf[i]=snapshot[i];
    }
    SnapShotWrite.write(buf,size);
    this->totleNum=0;
    std::ofstream numWriter(this->NumFire);
    numWriter<<this->totleNum;
    numWriter.close();
    this->indexes.clear();
    this->cache.clear();
    std::remove(this->data_dir.c_str());
    std::remove(this->SnapShortFile.c_str());
    std::rename(snapshotTempName.c_str(),this->SnapShortFile.c_str());
    this->writeCommitIndex(endIndex);
    this->writeAppliedIndex(endIndex);
    this->writeLastIndexAndLastTerm(endIndex,endTerm);
    SnapShotWrite.close();
    delete buf;
}
template<typename command>
bool raft_storage<command>::haveSnapShot(){
    std::ifstream isHave(this->SnapShortFile,std::ios::in|std::ios::binary);
    if(isHave){
        isHave.close();
        return true;
    }
    else{
        isHave.close();
        return false;
    }
}



template<typename command>
raft_storage<command>::~raft_storage() {
   // Your code here
}

#endif // raft_storage_h