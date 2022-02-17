#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include <mutex>
#include <string>
#include <vector>
#include <map>

#include "rpc.h"
#include "mr_protocol.h"

using namespace std;

struct KeyVal {
    string key;
    string val;
};

//
// The map function is called once for each file of input. The first
// argument is the name of the input file, and the second is the
// file's complete contents. You should ignore the input file name,
// and look only at the contents argument. The return value is a slice
// of key/value pairs.
//
void printResponce(mr_protocol::AskTaskResponse responce){
	fprintf(stderr,"worker site:accept:%d,r_status:%d,type:%d,index:%d\n",responce.accept,(int)responce.r_status,(int)responce.type,responce.index);
}
bool isLetter(char input){
    if('a'<=input&&input<='z') return true;
    if('A'<=input&&input<='Z') return true;
    return false;
}
void addToken(map<string,int>&tool,string token){
    map<string,int>::iterator it;
    it=tool.find(token);
    if(it==tool.end()){
        tool.insert(std::pair<string,int>(token,1));
    }else{
        it->second=it->second+1;
        }
}
void addCount(map<string,vector<string>>&tool,string key,string count){
	map<string,vector<string>>::iterator it;
	it=tool.find(key);
	if(it==tool.end()){
		vector<string> docker;
		docker.push_back(count);
		tool.insert(std::pair<string,vector<string>>(key,docker));
	}else{
		it->second.push_back(count);
	}
}
bool shouldHandle(int index,string input){
	char first=input[0];
	switch (index)
	{
	case 0:
		if(first>='a'&&first<='m') return true;
		else return false;
	case 1:
		if(first>'m'&&first<='z')  return true;
		else return false;	
	case 2:
		if(first>='A'&&first<='M') return true;
		else return false;
	case 3:
		if(first>'M'&&first<='Z')  return true;
		else return false;	
	default:
		break;
	}
}
vector<KeyVal> Map(const string &filename, const string &content)
{
	// Copy your code from mr_sequential.cc here.
	vector<KeyVal> result;
    map<string,int> tool;
    map<string,int>::iterator it;
    int begin=0;
    int end;
    std::string token;
    while(begin<content.size()){
        if(isLetter(content[begin])){
            end=begin+1;
            while(end<content.size()){
                if(isLetter(content[end])){
                    end++;
                }else{
                    token=content.substr(begin,end-begin);
                    addToken(tool,token);  
                    begin=end+1; 
                    break;     
                }
            }
            if(end==content.size()){
                token=content.substr(begin,end-begin);
                addToken(tool,token);
                begin=end;
                break;
            }
        }else{
            begin++;
        }
    }
    it = tool.begin();
    while(it!=tool.end()){
        KeyVal temp;
        temp.key=it->first;
        temp.val=to_string(it->second);
        result.push_back(temp);
        it++;
    }
    return result;
}

KeyVal split(string input){
	KeyVal result;
	stringstream ss(input);
	ss>>result.key;
	ss>>result.val;
	return result;
}
int splitToFour(string input){
	int val=input[0];
	return val%4;
}
//
// The reduce function is called once for each key generated by the
// map tasks, with a list of all the values created for that key by
// any map task.
//
string Reduce(const string &key, const vector < string > &values)
{
    // Copy your code from mr_sequential.cc here.
	int count=0;
    for(int i=0;i<values.size();i++){
        int temp=std::atoi((values[i]).c_str());
        count+=temp;
    }
    return to_string(count);
}


typedef vector<KeyVal> (*MAPF)(const string &key, const string &value);
typedef string (*REDUCEF)(const string &key, const vector<string> &values);

class Worker {
public:
	Worker(const string &dst, const string &dir, MAPF mf, REDUCEF rf);

	void doWork();

private:
	void doMap(int index, const vector<string> &filenames);
	void doReduce(int index);
	//void doSubmit(mr_tasktype taskType, int index);
	mutex mtx;
	int id;

	rpcc *cl;
	std::string basedir;
	MAPF mapf;
	REDUCEF reducef;
};


Worker::Worker(const string &dst, const string &dir, MAPF mf, REDUCEF rf)
{
	this->basedir = dir;
	this->mapf = mf;
	this->reducef = rf;

	sockaddr_in dstsock;
	make_sockaddr(dst.c_str(), &dstsock);
	this->cl = new rpcc(dstsock);
	if (this->cl->bind() < 0) {
		printf("mr worker: call bind error\n");
	}
}

void Worker::doMap(int index, const vector<string> &filenames)
{
	// cout<<"begin do map with index= "<<index<<endl;
	//fprintf(stderr,"begin do map with index=%d,filename=%s\n",index,filenames[0].c_str());
	// Lab2: Your code goes here.
	for(int i=0;i<filenames.size();i++){
		ifstream in(filenames[i]);
		if(!in.is_open()){
			fprintf(stderr,"fail to open file:%s\n",filenames[i].c_str());
			throw(-1);
		}
		string content="";
		stringstream buffer;
		vector<KeyVal> result;
		buffer << in.rdbuf();
		content = buffer.str();
		//fprintf(stderr,"the content of file:%s is:%s\n",filenames[i].c_str(),content.c_str());
		result = Map(filenames[i],content);
		in.close();
		//write result into file
		// string middlefilename="mr-m"+to_string(index);
		// ofstream out(middlefilename);
		// if(!out){
		// 	cout<<"fail to open mr-m"<<to_string(index)<<endl;
		// }
		string Sresult0;
		string Sresult1;
		string Sresult2;
		string Sresult3;
		for(auto it:result){
			string temp=it.key+" "+it.val+" \n";
			// Sresult+=temp;
			switch (splitToFour(it.key))
			{
			case 0:
				Sresult0+=temp;
				break;
			case 1:
				Sresult1+=temp;
				break;
			case 2:
				Sresult2+=temp;
				break;
			case 3:
				Sresult3+=temp;
				break;		
			default:
				break;
			}
		}
		string middlefilename0="mr-m"+to_string(index)+"-r0";
		string middlefilename1="mr-m"+to_string(index)+"-r1";
		string middlefilename2="mr-m"+to_string(index)+"-r2";
		string middlefilename3="mr-m"+to_string(index)+"-r3";
		ofstream out0(middlefilename0);
		if(!out0){
			cout<<"fail to open file："<<middlefilename0<<endl;
		}
		out0<<Sresult0;
		out0.close();
		ofstream out1(middlefilename1);
		if(!out1){
			cout<<"fail to open file："<<middlefilename1<<endl;
		}
		out1<<Sresult1;
		out1.close();
		ofstream out2(middlefilename2);
		if(!out2){
			cout<<"fail to open file："<<middlefilename2<<endl;
		}
		out2<<Sresult2;
		out2.close();
		ofstream out3(middlefilename3);
		if(!out3){
			cout<<"fail to open file："<<middlefilename3<<endl;
		}
		out3<<Sresult3;
		out3.close();
		// out<<Sresult;
		// out.close();
	}

}

void Worker::doReduce(int index)
{
	//cout<<"begin do reduce with index= "<<index<<endl;
	//fprintf(stderr,"begin do reduce with index=%d \n",index);
	// Lab2: Your code goes here.
    //a-m index:0 n-z index:1 A-M index:2 N-Z:index:3
	map<string,vector<string>> docker;
	map<string,string> result;
	int read_index=0;
	while (true)
	{
		//read all the map result file
		ifstream in("mr-m"+to_string(read_index)+"-r"+to_string(index));
		if(!in.is_open()) {
			cout<<"can not open mr-m"<<read_index<<"-r"<<endl;
			break;
		}
		//count the file
		string line;
		while(getline(in,line)){
			KeyVal lineKV=split(line);	
			addCount(docker,lineKV.key,lineKV.val);
		}
		//finish one single file
		read_index++;
	}
	//finish all the file,now should reduce 
	for(auto it:docker){
		string totle=Reduce(it.first,it.second);
		result.insert(std::pair<string,string>(it.first,totle));
	}
	//write
	string finalname="mr-out"+to_string(index);
	string Sresult;
	ofstream out(finalname);
	for(auto it:result){
		string temp=it.first+" "+it.second+"\n";
		Sresult+=temp;
		//out<<it.first<<" "<<it.second<<endl;
	}
	out<<Sresult;
	out.close();
}

// void Worker::doSubmit(mr_tasktype taskType, int index)
// {
// 	bool b;
// 	mr_protocol::status ret = this->cl->call(mr_protocol::submittask, taskType, index, b);
// 	if (ret != mr_protocol::OK) {
// 		fprintf(stderr, "submit task failed\n");
// 		exit(-1);
// 	}
// }

void Worker::doWork()
{   
	int temp=1;
	int type=NEED_MAP;  //initail the guide
	int asktype;
	int index;
	int result;
	int t_status;
	mr_protocol::AskTaskRequest askRequest;
	mr_protocol::SubmitTaskRequest	submitRequest;
	for (;;) {
		mr_protocol::SubmitTaskResponse submitResponce;
		mr_protocol::AskTaskResponse askResponce;
		//ask stage
		switch (type)
		{
		case NEED_MAP:  //last responce is need map
			asktype=MAP;
			result=cl->call(mr_protocol::asktask,asktype,askResponce);
			//fprintf(stderr,"worker recieve\n");
			//printResponce(askResponce);
			break;
		case NEED_REDUCE:
			//fprintf(stderr,"ask reduce task\n");
			asktype=REDUCE;
			cl->call(mr_protocol::asktask,asktype,askResponce);
			//printResponce(askResponce);
			break;	
		case WAIT:
            //sleep for a second and ask request
			//sleep(1);
			//fprintf(stderr,"worker asked to sleep\n");
			asktype=AFTER_WAIT;
			if(cl->call(mr_protocol::asktask,asktype,askResponce)!=0) return;
			break;
		case FINISHED:
		    //the mission is finished, should return
			return;
		default:
		    //fprintf(stderr,"default situation\n");
			break;
		}
		//check the responce,IF has job do job and submit
		if(askResponce.accept){
			//if the mission is assigned
		    if(askResponce.type==MAP){
				//get file name
				string filename;
				int index;
				index=askResponce.index;
				filename.assign(askResponce.filename,askResponce.len);
				vector<string> map_vector;
				map_vector.push_back(filename);
				try{
				doMap(index,map_vector);
				//finished
				index=index;
				t_status=MAP_FINISH;
				cl->call(mr_protocol::submittask,t_status,index,submitResponce);
				type=(int)submitResponce.r_status;
				}catch(int e){
					//not finished
					t_status=MAP_FAIL;
					index=index;
					cl->call(mr_protocol::submittask,t_status,index,submitResponce);
					type=submitResponce.r_status;
				}
			}else{
				// the task is reduce,base on the index divide the task
				//a-m index:0 n-z index:1 A-M index:2 N-Z:index:3
				int index=askResponce.index;
				doReduce(index);
				//finished,now don't have fail condition
				index=index;
				t_status=REDUCE_FINISH;
				cl->call(mr_protocol::submittask,t_status,index,submitResponce);
				type=(int)submitResponce.r_status;
			}		
			}else{
			
			type=(int)askResponce.r_status;
			//fprintf(stderr,"asked denied,type=%d\n",type);
		}

	}
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <coordinator_listen_port> <intermediate_file_dir> \n", argv[0]);
		exit(1);
	}
	cout<<"start worker"<<endl;
	//fprintf(stderr, "start worker \n");
	MAPF mf = Map;
	REDUCEF rf = Reduce;
	
	Worker w(argv[1], argv[2], mf, rf);
	w.doWork();
	//fprintf(stderr,"quit work \n");
	return 0;
}
