#include <fstream>
#include <iostream>
using namespace std;
int main(int argc, char **argv){
    cout<<argv[1]<<endl;
    const string t=argv[1];
    ofstream out("write_test"+t);
    ofstream out2("write_test_b"+t);
    ofstream out3("write_test_c"+t);
    string r1="";
    string r2="";
    string r3="";
    for(int i=0;i<100000;i++){
        r1+="aaaaaa\n";
        r2+="bbbbbb\n";
        r3+="cccccc\n";
    }
    out<<r1;
    out2<<r2;
    out3<<r3;
    out.close();
    cout<<"finish job"<<endl;
    return 0;
}
