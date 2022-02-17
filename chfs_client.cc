// chfs client.  implements FS operations using extent and lock server
#include "chfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
int create_time=1;
int lookup_time=1;
int writeTime=1;
chfs_client::chfs_client()
{
    ec = new extent_client();

}

chfs_client::chfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

chfs_client::inum
chfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
chfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
chfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is a dir\n", inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */
bool
chfs_client::issymlink(inum inum){
     extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYMLINK) {
        printf("isSymlink: %lld is a Symlink\n", inum);
        return true;
    } 
    printf("isfile: %lld is not a symlink\n", inum);
    return false;
}
int
chfs_client::symlink(inum parent,const char * name,const char *link,inum &ino_out){
    int r=OK;
    bool found=false;
    inum id;
    direntWithName target;
    std::string buf;
    if(this->ec->get(parent,buf)!=extent_protocol::OK){
        return extent_protocol::IOERR;
    }
    if(lookup(parent,name,found,id)!=extent_protocol::OK){
        return extent_protocol::IOERR;
    }
    if(found){
        return EXIST;
    }
    this->ec->create(extent_protocol::T_SYMLINK,ino_out);
    this->ec->put(ino_out,std::string(link));
    target.inum=ino_out;
    target.len=strlen(name);
    memcpy(target.name,name,target.len);
    buf.append((char *)(&target),sizeof(direntWithName));
    ec->put(parent,buf);
    return r;
}
int
chfs_client::readlink(inum ino,std::string &buf){
    if(ec->get(ino,buf)!=extent_protocol::OK){
        return IOERR;
    }
    return extent_protocol::OK;
}

bool
chfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
     extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        return false;
    }
    if (a.type == extent_protocol::T_DIR) {
        return true;
    } 
    return false;
}

int
chfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
chfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
chfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    std::string buf;
    if(this->ec->get(ino,buf)!=extent_protocol::OK){
        return extent_protocol::IOERR;
    }
    buf.resize(size);
    if(this->ec->put(ino,buf)!=extent_protocol::OK){
         return extent_protocol::IOERR;
    }
    return r;
}

int
chfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    //check if the file already exist
    //printf("create file%s,time:%d\n",name,create_time++);
    bool found;
    this->lookup(parent,name,found,ino_out);
    if(found){
        printf("the file %s already exist\n",name);
        return EXIST;
    }
    //create the file
    extent_protocol::extentid_t eid;
    this->ec->create(extent_protocol::T_FILE,eid);
    //add the new file into parent
    std::string buf;
    if(this->ec->get(parent,buf)!=extent_protocol::OK){
        return IOERR;
    }
    direntWithName newfile;
    newfile.inum=eid;
    newfile.len=strlen(name);
    memcpy(newfile.name,name,newfile.len);
    buf.append((char*)&newfile,sizeof(direntWithName));
    printf("after create,buf size=%d\n",buf.size());
    if(this->ec->put(parent,buf)!=extent_protocol::OK){
        return IOERR;
    }
    ino_out=eid;
    return r;
}

int
chfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    bool found;
    this->lookup(parent,name,found,ino_out);
    if(found){
        printf("the file %s already exist\n",name);
        return EXIST;
    }
    //create the file
    extent_protocol::extentid_t eid;
    this->ec->create(extent_protocol::T_DIR,eid);
    //add the new file into parent
    std::string buf;
    if(this->ec->get(parent,buf)!=extent_protocol::OK){
        return IOERR;
    }
    direntWithName newfile;
    newfile.inum=eid;
    newfile.len=strlen(name);
    memcpy(newfile.name,name,newfile.len);
    buf.append((char*)&newfile,sizeof(direntWithName));
    printf("after create,buf size=%d\n",buf.size());
    if(this->ec->put(parent,buf)!=extent_protocol::OK){
        return IOERR;
    }
    ino_out=eid;
    return r;
}

int
chfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    if(lookup_time==3){
        int debug=1;
    }
    //printf("called lookup,time%d\n",lookup_time++);
    std::list<dirent> list;
    if(this->readdir(parent,list)!=OK){
        return IOERR;
    }
    std::string s_name;
    s_name=name;
    found=false;
    for(dirent it: list){
        if(it.name.compare(s_name)==0){
            ino_out=it.inum;
            found=true;
            break;
        }
    }
    return r;
    //  std::list<dirent> entries;
    // r = readdir(parent, entries);
    // std::list<dirent>::iterator it = entries.begin();
    // while(it != entries.end()) {
    //     std::string file = it->name;
    //     if (file == std::string(name)) {
    //         found = true;
    //         ino_out = it->inum;
    //         r = EXIST;
    //         return r;
    //     }
    //     it++;
    // }

    // return r;
}

int
chfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;
    
    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    printf("read addrir\n");
    extent_protocol::attr a;
    std::string buf;
    char * pos;
    //get the attr information
    if(this->ec->getattr(dir,a)!=extent_protocol::OK){
        return IOERR;
    }
    if(a.type!=extent_protocol::T_DIR){
        printf("EERROR: the type of file should be dir !\n");
        return r;
    }
    if(this->ec->get(dir,buf)!=extent_protocol::OK){
        return IOERR;
    }
    pos=(char *)buf.c_str();
    //cppy the content into dirent
    printf("addr of pos:%p,the size of buf:%d",pos,buf.size());
    int num=buf.size()/sizeof(direntWithName);
    if(lookup_time==4){
        int debug=3;
    }
    for(int i=0;i<num;i++){
        // dirent item;
        // memcpy((char*)&item,pos,sizeof(dirent));
        // pos+=sizeof(dirent);
        // list.push_back(item); 
        direntWithName dir1;
        memcpy((char*)&dir1,pos,sizeof(direntWithName));
        dirent item;
        item.inum=dir1.inum;
        item.name.assign(dir1.name,dir1.len);
        list.push_back(item);
        pos+=sizeof(direntWithName);
    }
    return r;
    // std::string buf;

    // r = ec->get(dir, buf);
    // int size = buf.size();
    // int start = 0;
    // struct dirent tmp;
    // for(int i = 0; i < size; i++){
    //     if(buf[i] == '/'){
    //         if(buf[i+1] == '/'){
    //             tmp.inum = n2i(buf.substr(start, i - start));
    //             i++;
    //             start = i + 1;
    //             list.push_back(tmp);
    //         }
    //         else{
    //             tmp.name = buf.substr(start, i - start);
    //             start = i + 1;
    //         }
    //     }
    // }

    // return r;
}

int
chfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your code goes here.
     * note: read using ec->get().
     */
    std::string buf;
    this->ec->get(ino,buf);
    size_t origin_size=buf.size();
    if(off>=origin_size){
        return r;
    }
    data.assign(buf,off,size);

    return r;
}

int
chfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    std::string input;
    std::string buf;
    if(writeTime==10){
        int DEBUG=1;
    }
    printf("writetime:%d\n",writeTime++);
    input.assign(data,size);
    this->ec->get(ino,buf);
    size_t origin_size=buf.size();
    if((size_t)off>origin_size){
        buf.resize(off,'\0');
        buf.replace(off,size,input);
        bytes_written=off-origin_size+size;
        if(this->ec->put(ino,buf)!=extent_protocol::OK){
            return extent_protocol::IOERR;
        }
    }else{
        buf.replace(off,size,input);
        bytes_written=size;
        if(this->ec->put(ino,buf)!=extent_protocol::OK){
            return extent_protocol::IOERR;
        }
    }
    return r;
}

int chfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    std::list<dirent> dirList;
    std::string buf;
    bool found=false;
    inum target;
    if(lookup(parent,name,found,target)!=extent_protocol::OK||!found){
        printf("the file unlink doesn't exist!");
        return extent_protocol::IOERR;
    }
    //remove the file
    if(this->ec->remove(target)!=extent_protocol::OK){
        return extent_protocol::IOERR;
    }
    if(readdir(parent,dirList)!=extent_protocol::OK){
        return extent_protocol::IOERR;
    }
    for(dirent it:dirList){
        if(it.inum!=target){
            direntWithName temp;
            temp.inum=it.inum;
            temp.len=it.name.size();
            memcpy(temp.name,it.name.data(),temp.len);
            buf.append((char *)(&temp),sizeof(direntWithName));
        }
    }
    if(this->ec->put(parent,buf)!=extent_protocol::OK){
        return extent_protocol::IOERR;
    }
    return r;
}


