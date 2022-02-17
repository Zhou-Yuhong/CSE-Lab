#include <fstream>
#include <iostream>
using namespace std;
int main(){
    cout<<"begin test"<<endl;
    string m;
    ofstream out("./chfs2/my");
    out<<"test"<<endl;
    out.close();
    ifstream in("./chfs2/my");
    if(!in.is_open()){
        cout<<"ERROR"<<endl;
    }else{
        cout<<"OPEN"<<endl;
    }
    in.close();
    return 0;
}
