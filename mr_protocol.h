#ifndef mr_protocol_h_
#define mr_protocol_h_

#include <string>
#include <vector>

#include "rpc.h"

using namespace std;

#define REDUCER_COUNT 4

enum mr_tasktype {
	NONE = 0, // this flag means no task needs to be performed at this point
	MAP,
	REDUCE,
	AFTER_WAIT  //means the last reply.r_status is WAIT,now need to check
};
enum response_status{
	//guide the next request
	NEED_MAP=0,
	NEED_REDUCE,
	WAIT,
	FINISHED
};
enum task_status{
	MAP_FINISH=0,
	REDUCE_FINISH,
	MAP_FAIL,
	REDUCE_FAIL
};

class mr_protocol {
public:
	typedef int status;
	enum xxstatus { OK, RPCERR, NOENT, IOERR };
	enum rpc_numbers {
		asktask = 0xa001,
		submittask,
	};

	struct AskTaskResponse {
		// Lab2: Your definition here.
		bool accept;      //if the task is released to worker
		response_status r_status;
		mr_tasktype type;
		int index;
		char filename[128];
		int len;
	};

	struct AskTaskRequest {
		// Lab2: Your definition here.
		mr_tasktype ask_type;

	};

	struct SubmitTaskResponse {
		// Lab2: Your definition here.
		response_status r_status;
	};

	struct SubmitTaskRequest {
		// Lab2: Your definition here.
		task_status t_status;
		int index;
	};

};
marshall& operator<<(marshall &m,mr_protocol::AskTaskRequest &request){
	 m << ((int)request.ask_type);
	 return m;
}
unmarshall& operator>>(unmarshall &u,mr_protocol::AskTaskRequest &request){
	int ask_type;
	u >> ask_type;
	request.ask_type=mr_tasktype(ask_type);
	return u;
}
marshall& operator<<(marshall &m,mr_protocol::AskTaskResponse &responce){
  m<<responce.accept;
  m<<((int)responce.r_status);
  m<<((int)responce.type);
  m<<responce.index;
  for(int i=0;i<128;i++){
	  m<<responce.filename[i];
  }
  m<<responce.len;
  return m;
}
unmarshall& operator>>(unmarshall &u,mr_protocol::AskTaskResponse &responce){
	u>>responce.accept;
	int r_status;
	u>>r_status;
	responce.r_status=response_status(r_status);
	int type;
	u>>type;
	responce.type=mr_tasktype(type);
	u>>responce.index;
	for(int i=0;i<128;i++){
		u>>responce.filename[i];
	}
	u>>responce.len;
	return u;
}
marshall& operator<<(marshall &m,mr_protocol::SubmitTaskRequest &request){
	m<<request.index;
	m<<((int)request.t_status);
	return m;
}
unmarshall& operator>>(unmarshall &u,mr_protocol::SubmitTaskRequest &request){
	u>>request.index;
	int t_status;
	u>>t_status;
	request.t_status=task_status(t_status);
	return u;
}
marshall& operator<<(marshall &m,mr_protocol::SubmitTaskResponse &responce){
	m<<((int)responce.r_status);
	return m;
}
unmarshall& operator>>(unmarshall &u,mr_protocol::SubmitTaskResponse &responce){
	int r_status;
	u>>r_status;
	responce.r_status=response_status(r_status);
	return u;
}

#endif

