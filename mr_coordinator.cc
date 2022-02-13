#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include <mutex>

#include "mr_protocol.h"
#include "rpc.h"

using namespace std;
void printResponce(mr_protocol::AskTaskResponse responce){
	fprintf(stderr,"server site:accept:%d,r_status:%d,type:%d,index:%d\n",responce.accept,(int)responce.r_status,(int)responce.type,responce.index);
}
struct Task {
	int taskType;     // should be either Mapper or Reducer
	bool isAssigned;  // has been assigned to a worker
	bool isCompleted; // has been finised by a worker
	int index;        // index to the file
};

class Coordinator {
public:
	Coordinator(const vector<string> &files, int nReduce);
	mr_protocol::status askTask(int ask_type, mr_protocol::AskTaskResponse &reply);
	mr_protocol::status submitTask(int t_status,int index,mr_protocol::SubmitTaskResponse &reply);
	bool isFinishedMap();
	bool isFinishedReduce();
	bool Done();

private:
	vector<string> files;
	vector<Task> mapTasks;
	vector<Task> reduceTasks;

	mutex mtx;

	long completedMapCount;
	long completedReduceCount;
	bool isFinished;
	
	string getFile(int index);
};


// Your code here -- RPC handlers for the worker to call.

mr_protocol::status Coordinator::askTask(int ask_type, mr_protocol::AskTaskResponse &reply) {
	// Lab2 : Your code goes here.
	//fprintf(stderr,"recive askTast request,ask_type=%d\n",ask_type);
	switch (ask_type)
	{
	case NONE:      //NONE
		break;

	case MAP:   //ask for map task
		//check if map finished
		if(this->isFinishedMap()){
			reply.accept=false;
			reply.r_status=NEED_REDUCE;
		}else{
			//find a task to release,need lock
			//fprintf(stderr,"find a task to release\n");
			this->mtx.lock();
			int i=0;
			while(i<mapTasks.size()){
				if(!mapTasks[i].isAssigned) break;
				i++;
			}
			if(i==mapTasks.size()){  //if all work is assigned
				//fprintf(stderr,"all map work is assigned\n");
				reply.accept=false;
				reply.r_status=WAIT;  //all map task is assigned but not finished, so worker should wait
			}else{
				//assigned a work
				//fprintf(stderr,"findone:i=%d \n",i);
				reply.accept=true;
				reply.r_status=NEED_MAP;
				mapTasks[i].isAssigned=true;
				reply.index=mapTasks[i].index;
				reply.type=MAP;
				//char* file=(char*)((this->getFile(mapTasks[i].index)).c_str());
				char* file=(char*)((this->files[mapTasks[i].index]).c_str());
				//fprintf(stderr,"filename is%s \n",files[i].c_str());
				reply.len=strlen(file);
				memcpy(reply.filename,file,reply.len);
				// fprintf(stderr,"first place after assign\n");
				// printResponce(reply);
			}
			this->mtx.unlock();
		}
		break;
	case REDUCE:  //ask for reduce task
		if(this->isFinishedReduce()){
			reply.accept=false;
			reply.r_status=FINISHED;
		}else{
			//find a reduce task, need lock
			this->mtx.lock();
			int i=0;
			while (i<reduceTasks.size())
			{
				/* code */
				if(!reduceTasks[i].isAssigned) break;
				i++;
			}
			if(i==reduceTasks.size()){
				//if all reduce work is assigned
				reply.accept=false;
				reply.r_status=WAIT;
			}else{
				//assign a reduce work
				reply.accept=true;
				reply.r_status=NEED_REDUCE;
				reduceTasks[i].isAssigned=true;
				reply.index=reduceTasks[i].index;
				reply.type=REDUCE;
			}
			this->mtx.unlock();
		}
		break;
	case AFTER_WAIT:
		//check if the work is done
		if(this->Done()){
			reply.accept=false;
			reply.r_status=FINISHED;
		}else{
			//not finished yet
			if(this->isFinishedMap()){
				//map finished,should assign reduce task
				this->mtx.lock();
				int i=0;
				while(i<this->reduceTasks.size()){
					if(!reduceTasks[i].isAssigned) break;
					i++;
				}
				if(i==this->reduceTasks.size()){
					//still unfinished or the work has reassigned to another worker
					reply.accept=false;
					reply.r_status=WAIT;
				}else{
					reply.accept=true;
					reply.r_status=NEED_REDUCE;
					reduceTasks[i].isAssigned=true;
					reply.index=reduceTasks[i].index;
					reply.type=REDUCE;
				}
				this->mtx.unlock();
			}else{
				//map is not finished, should assign map task
				this->mtx.lock();
				int i=0;
				while(i<this->mapTasks.size()){
					if(!mapTasks[i].isAssigned) break;
					i++;
				}
				if(i==mapTasks.size()){
					//still unfinished(wait for other worker) or the work has reassigned to another worker
					reply.accept=false;
					reply.r_status=WAIT;
				}
				else{
					//assign map work
					reply.accept=true;
					reply.r_status=NEED_MAP;
					mapTasks[i].isAssigned=true;
					reply.index=mapTasks[i].index;
					reply.type=MAP;
				}
				this->mtx.unlock();
			}
		}
		break;
	default:
		break;
	}
	//fprintf(stderr,"after assign\n");
	//printResponce(reply);
	return mr_protocol::OK;
}

mr_protocol::status Coordinator::submitTask(int t_status,int index,mr_protocol::SubmitTaskResponse &reply) {
	// Lab2 : Your code goes here.
	switch (t_status)
	{
	case MAP_FINISH: //map finish
		//modify the relate variables
		this->mtx.lock();
		this->completedMapCount++;
		this->mapTasks[index].isCompleted=true;
		//set reply
		if (this->completedMapCount >= long(this->mapTasks.size())) {
			reply.r_status=NEED_REDUCE;
		}else{
			reply.r_status=NEED_MAP;
		}
		this->mtx.unlock();
		break;
	case REDUCE_FINISH: //reduce finish
	    //modify the relate variables
		this->mtx.lock();
		this->completedReduceCount++;
		this->reduceTasks[index].isCompleted=true;
		//set reply
		if (this->completedReduceCount >= long(this->reduceTasks.size())) {
			this->isFinished=true;
			//fprintf(stderr,"mission complete\n");
			reply.r_status=FINISHED;
	    }else{
			reply.r_status=NEED_REDUCE;
		}
		this->mtx.unlock();
		break;
	case MAP_FAIL: //map_file
		//modify the relate variables
		this->mtx.lock();
		this->mapTasks[index].isAssigned=false;
		//set reply,once filed,it should wait so that other worker take the task
		reply.r_status=WAIT;	
		this->mtx.unlock();
		break;
	case REDUCE_FAIL: //reduce fail
		this->mtx.lock();
		this->reduceTasks[index].isAssigned=false;
		//set reply
		reply.r_status=WAIT;
		this->mtx.unlock();
		break;	
	default:
		break;
	}
	return mr_protocol::OK;
}

string Coordinator::getFile(int index) {
	this->mtx.lock();
	string file = this->files[index];
	this->mtx.unlock();
	return file;
}

bool Coordinator::isFinishedMap() {
	bool isFinished = false;
	this->mtx.lock();
	if (this->completedMapCount >= long(this->mapTasks.size())) {
		isFinished = true;
	}
	this->mtx.unlock();
	return isFinished;
}

bool Coordinator::isFinishedReduce() {
	bool isFinished = false;
	this->mtx.lock();
	if (this->completedReduceCount >= long(this->reduceTasks.size())) {
		isFinished = true;
	}
	this->mtx.unlock();
	return isFinished;
}

//
// mr_coordinator calls Done() periodically to find out
// if the entire job has finished.
//
bool Coordinator::Done() {
	bool r = false;
	this->mtx.lock();
	r = this->isFinished;
	this->mtx.unlock();
	return r;
}

//
// create a Coordinator.
// nReduce is the number of reduce tasks to use.
//
Coordinator::Coordinator(const vector<string> &files, int nReduce)
{
	this->files = files;
	this->isFinished = false;
	this->completedMapCount = 0;
	this->completedReduceCount = 0;

	int filesize = files.size();
	for (int i = 0; i < filesize; i++) {
		this->mapTasks.push_back(Task{mr_tasktype::MAP, false, false, i});
	}
	for (int i = 0; i < nReduce; i++) {
		this->reduceTasks.push_back(Task{mr_tasktype::REDUCE, false, false, i});
	}
}

int main(int argc, char *argv[])
{
	cout<<"coordinator main"<<endl;
	int count = 0;

	if(argc < 3){
		fprintf(stderr, "Usage: %s <port-listen> <inputfiles>...\n", argv[0]);
		exit(1);
	}
	char *port_listen = argv[1];
	
	setvbuf(stdout, NULL, _IONBF, 0);

	char *count_env = getenv("RPC_COUNT");
	if(count_env != NULL){
		count = atoi(count_env);
	}

	vector<string> files;
	char **p = &argv[2];
	while (*p) {
		files.push_back(string(*p));
		++p;
	}

	rpcs server(atoi(port_listen), count);

	Coordinator c(files, REDUCER_COUNT);
	
	//
	// Lab2: Your code here.
	// Hints: Register "askTask" and "submitTask" as RPC handlers here
	// 

	server.reg(mr_protocol::asktask,&c,&Coordinator::askTask);
	server.reg(mr_protocol::submittask,&c,&Coordinator::submitTask);
	while(!c.Done()) {
		sleep(1);
	}
	//fprintf(stderr,"server stop\n");
	return 0;
}


