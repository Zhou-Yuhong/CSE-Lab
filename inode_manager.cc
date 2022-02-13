#include "inode_manager.h"
int read_time=0;
int write_time=0;
// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
   if(id<0||id>=BLOCK_NUM){
     printf("the id%d is out of range",id);
     return;
   }
   memcpy(buf,this->blocks[id],BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  if(id<0||id>=BLOCK_NUM) {
      printf("the id%d is out of range",id);
      return;
  }
  memcpy(this->blocks[id],buf,BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */

  int i=IBLOCK(INODE_NUM,BLOCK_NUM);//alloc from the data block
  // while(index<=BLOCK_NUM){
  //   uint32_t it=BBLOCK(index);
  //   char buf[BLOCK_SIZE];
  //   char mask = 0x80;
  //   read_block(it,buf);
  //   while(index<=BLOCK_NUM&&(BBLOCK(index)==BBLOCK(index+1))){ //do not need to change the content of buf
  //     uint32_t offset=index%BPB;  //offset is also the index of bitmap
  //     char target=buf[offset/8];
  //     if((target&(mask>>(offset%8)))!=0){
  //       index++;
  //       continue;
  //     }else{
  //       //alloc block 
  //       buf[offset/8]=target|(mask>>(offset%8));
  //       write_block(it,buf);
  //       printf("block %d alloced\n",index);
  //       return index;
  //     }
  //   }  
  // }
  while(i<BLOCK_NUM){
  if(!using_blocks[i]){
      using_blocks[i]=1;
      return i;
  }
  i++;
  }
  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  // if(id<0||id>=BLOCK_NUM){
  //    printf("the id%d is out of range",id);
  //    return;
  //  }
  // printf("block%d free\n",id);
  // uint32_t it=BBLOCK(id);
  // char buf[BLOCK_SIZE];
  // read_block(it,buf);
  // char mask = 0x80;
  // uint32_t offset=id%BPB;
  // char target=buf[offset/8];
  // buf[offset/8]=target&(~(mask>>(offset%8)));
  // write_block(it,buf);
  //NEWWAY
  using_blocks[id]=0;
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();
  //NEWWAY
  for(uint i = 0; i < IBLOCK(INODE_NUM, sb.nblocks); i++)
    using_blocks[i] = 1;
  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  uint32_t index=1;
  inode* result=NULL;
  for(;index<INODE_NUM;index++){
    result=this->get_inode(index);
    if(result==NULL) break;
  }
  result=new inode();
  result->type=(short)type;
  result->size=0;
  result->atime=(uint32_t)time(0);
  result->mtime=(uint32_t)time(0);
  result->ctime=(uint32_t)time(0);
  this->put_inode(index,result);
  free(result);
  return index;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
  inode* target=get_inode(inum);
  if(target){
    target->type=0;
    put_inode(inum,target);
    free(target);
  }
  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_out
   */
  //DEBUG
  inode* target=this->get_inode(inum);
  uint32_t origin_size=target->size;
  *size=origin_size;
  *buf_out=(char *)malloc(origin_size);
  int num=(origin_size)/(BLOCK_SIZE);
  if((origin_size%BLOCK_SIZE)!=0) num++; 
  char buf[BLOCK_SIZE];
   if(num>NDIRECT){
      this->bm->read_block(target->blocks[NDIRECT],buf);
    }
  char *data=(char*)malloc(num*BLOCK_SIZE);  
  char* pos=data;
  //copy data
  for(int i=0;i<num;i++){
    //NDIRECT 
    if(i<NDIRECT){
    this->bm->read_block(target->blocks[i],pos);
    pos+=BLOCK_SIZE;
    }else{
     int offset=i-NDIRECT;
     uint32_t block_id=*((uint32_t *)(buf+offset*4));
     //printf("read block num:%d\n",block_id);
     this->bm->read_block(block_id,pos);
     pos+=BLOCK_SIZE;
    }
  }
  memcpy(*buf_out,data,origin_size);
  target->atime=time(0);
  put_inode(inum,target);
  free(target);
  return;
  
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  //DEBUG
  inode* target=this->get_inode(inum);
  uint32_t origin_size=target->size;
  char group[BLOCK_SIZE];
  int origin_num=origin_size/BLOCK_SIZE;
  if(origin_size%BLOCK_SIZE!=0) origin_num++;
  int num=size/BLOCK_SIZE;
  if(size%BLOCK_SIZE!=0) num++;
  //transport the buf into data
  char *data=(char *)malloc(num*BLOCK_SIZE);
  memcpy(data,buf,size);
  char* pos=data;
  //store the specical block in group
  if(origin_num>NDIRECT){
        this->bm->read_block(target->blocks[NDIRECT],group);
    }
  int offset=0;
  uint32_t block_id=0;
  if(origin_size>size){
    //should free the extra blocks
    for(int i=0;i<num;i++){
      if(i<NDIRECT){
        this->bm->write_block(target->blocks[i],pos);
        pos+=BLOCK_SIZE;
        continue;
      }else{
        //get the block num
        offset=i-NDIRECT;
        block_id=*((uint32_t*)(group+offset*4));
        this->bm->write_block(block_id,pos);
        pos+=BLOCK_SIZE;
        continue;
      }
    }
    //free the extra blocks
    for(int i=num;i<origin_num;i++){
      if(i<NDIRECT){
        this->bm->free_block(target->blocks[i]);
        continue;
      }else{
        offset=i-NDIRECT;
        block_id=*((uint32_t*)(group+4*offset));
        this->bm->free_block(block_id);
      }
    }
  }else{
    //should alloc the extra blocks
    for(int i=0;i<origin_num;i++){
      if(i<NDIRECT){
        this->bm->write_block(target->blocks[i],pos);
        pos+=BLOCK_SIZE;
        continue;
      }else{
        //get the block num
        offset=i-NDIRECT;
        block_id=*((uint32_t*)(group+offset*4));
        this->bm->write_block(block_id,pos);
        pos+=BLOCK_SIZE;
        continue;
      }
    }
    //alloc the extra blocks
    for(int i=origin_num;i<num;i++){

       if(i<NDIRECT){
         uint32_t alloc_id=this->bm->alloc_block();
         target->blocks[i]=alloc_id;
         this->bm->write_block(target->blocks[i],pos);
         pos+=BLOCK_SIZE;
         continue;
       }
      //  else{
      //    uint32_t alloc_id=this->bm->alloc_block();
      //    offset=i-NDIRECT;
      //    memcpy(group+offset*4,(char*)&alloc_id,4);
      //  }
      if(i==NDIRECT){
        uint32_t alloc_id=this->bm->alloc_block();
        target->blocks[i]=alloc_id;
        //printf("alloc num set block:%d\n",alloc_id);
        uint32_t alloc_id2=this->bm->alloc_block();
        //printf("alloc indirect block:%d\n",alloc_id2);
        this->bm->write_block(alloc_id2,pos);
        pos+=BLOCK_SIZE;
        offset=i-NDIRECT;
        memcpy(group+offset*4,(char*)&alloc_id2,4);
        continue;
      }
      if(i>NDIRECT){
         uint32_t alloc_id=this->bm->alloc_block();
         //printf("alloc indirect block:%d\n",alloc_id);
         this->bm->write_block(alloc_id,pos);
         pos+=BLOCK_SIZE;
         offset=i-NDIRECT;
         memcpy(group+offset*4,(char*)&alloc_id,4);
         continue;
      }
    }
     if(num>NDIRECT){
       this->bm->write_block(target->blocks[NDIRECT],group);
     }
  }
  //modify the attr information
  target->size=(uint32_t)size;
  target->atime=(unsigned int)time(0);
  target->mtime=(unsigned int)time(0);
  target->ctime=(unsigned int)time(0);
  put_inode(inum,target);
  //get the target again to see if data was wrote
  free(target);


  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  inode * result=this->get_inode(inum);
  if(result==NULL){
    printf("the %d inode is empty\n",inum);
    return;
  }
  a.type=result->type;
  a.atime=result->atime;
  a.mtime=result->mtime;
  a.ctime=result->ctime;
  a.size=result->size;
  free(result);
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  //free the block
  inode* target=this->get_inode(inum);
  uint32_t size=target->size;
  int num=size/BLOCK_SIZE;
  if(size%BLOCK_SIZE!=0) num++;
  if(num<=NDIRECT){
    //free the block
    for(int i=0;i<num;i++){
      this->bm->free_block(target->blocks[i]);
    }
  }else{
    for(int i=0;i<NDIRECT;i++){
      this->bm->free_block(target->blocks[i]);
    }
    //free the NINDIRECT BLOCK
    char buf[BLOCK_SIZE];
    this->bm->read_block(target->blocks[NDIRECT],buf);
    for(int i=NDIRECT;i<num;i++){
      int offset=i-NDIRECT;
      uint32_t block_id=*((uint32_t*)(buf+offset*4));
      this->bm->free_block(block_id);
    }
     this->bm->free_block(target->blocks[NDIRECT]);
  }
  //free the inode
  target->type=0;
  put_inode(inum,target);
  free(target);
  return;
}
