//BHAVY SINGH 170212
#include<types.h>
#include<context.h>
#include<file.h>
#include<lib.h>
#include<serial.h>
#include<entry.h>
#include<memory.h>
#include<fs.h>
#include<kbd.h>
#include<pipe.h>

/************************************************************************************/
/***************************Do Not Modify below Functions****************************/
/************************************************************************************/
void free_file_object(struct file *filep)
{
    if(filep)
    {
       os_page_free(OS_DS_REG ,filep);
       stats->file_objects--;
    }
}

struct file *alloc_file()
{
  
  struct file *file = (struct file *) os_page_alloc(OS_DS_REG); 
  file->fops = (struct fileops *) (file + sizeof(struct file)); 
  bzero((char *)file->fops, sizeof(struct fileops));
  stats->file_objects++;
  return file; 
}

static int do_read_kbd(struct file* filep, char * buff, u32 count)
{
  kbd_read(buff);
  return 1;
}

static int do_write_console(struct file* filep, char * buff, u32 count)
{
  struct exec_context *current = get_current_ctx();
  return do_write(current, (u64)buff, (u64)count);
}

struct file *create_standard_IO(int type)
{
  struct file *filep = alloc_file();
  filep->ref_count=1;
  filep->type = type;
  if(type == STDIN)
     filep->mode = O_READ;
  else
      filep->mode = O_WRITE;
  if(type == STDIN){
        filep->fops->read = do_read_kbd;
  }else{
        filep->fops->write = do_write_console;
  }
  filep->fops->close = generic_close;
  return filep;
}

int open_standard_IO(struct exec_context *ctx, int type)
{
   int fd = type;
   struct file *filep = ctx->files[type];
   if(!filep){
        filep = create_standard_IO(type);
   }else{
         filep->ref_count++;
         fd = 3;
         while(ctx->files[fd])
             fd++; 
   }
   ctx->files[fd] = filep;
   return fd;
}
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/



void do_file_fork(struct exec_context *child)
{
   /*TODO the child fds are a copy of the parent. Adjust the refcount*/
  if(child == NULL){
    return;
  }
  for (int i =0 ;i<MAX_OPEN_FILES;i++){ 
    if(child->files[i]!=NULL){
      child->files[i]->ref_count++; // increase all ref counts
    }
  }
}

void do_file_exit(struct exec_context *ctx)
{
   /*TODO the process is exiting. Adjust the ref_count
     of files*/
    if(!ctx){return;}
    for (int i =0 ;i<MAX_OPEN_FILES;i++){
      if(ctx->files[i]!=NULL){
        generic_close(ctx->files[i]);
        ctx->files[i]=NULL; // set ctx's file to NULL
      }
  }
}

long generic_close(struct file *filep)
{
  /** TODO Implementation of close (pipe, file) based on the type 
   * Adjust the ref_count, free file object
   * Incase of Error return valid Error code 
   */
   
  if(filep==NULL){
     return -1;
   }
   else if(filep->ref_count<=0)
    return -1;

   filep->ref_count --; // decrease refcount
   if(filep->ref_count == 0){

     if(filep->type==REGULAR){
      free_file_object(filep);
    }
     else if(filep->type == PIPE){ // for PIPE check if both ends are close then free the PIPE
         
        if(filep->mode == O_READ){
           filep->pipe->is_ropen=0;
           if(filep->pipe->is_wopen==0){
             free_pipe_info(filep->pipe);
           }
         }
        else if(filep->mode == O_WRITE){
           filep->pipe->is_wopen=0;
           if(filep->pipe->is_ropen==0){
             free_pipe_info(filep->pipe);
           }
         }
         free_file_object(filep);
     }
     else{
         free_file_object(filep);
     }
   }
   return 0;
    
  
}

static int do_read_regular(struct file *filep, char * buff, u32 count)
{
   /** TODO Implementation of File Read, 
    *  You should be reading the content from File using file system read function call and fill the buf
    *  Validate the permission, file existence, Max length etc
    *  Incase of Error return valid Error code 
    * */
     if (filep == NULL)
    {
      return -1;
    }
    if(((filep->mode&O_READ))){
    u64 i, size = 0, s_pos;

    long int offp = (long int)filep->offp;
    long int remain_len;
    remain_len = (long int)filep->inode->file_size - offp;  //  an implementation of flat_read()
    if( remain_len <= 0 )
      return 0;
    size = ( count > remain_len ? remain_len : count );
          s_pos = filep->inode->s_pos;
    for( i=0 ; i<size ; i++)
    {
      buff[i] = *(char *)(s_pos+ offp + i);
    }
    filep->offp += i; // check this
    return size;
    }
    
    int ret_fd = -1; 
    return ret_fd;
}


static int do_write_regular(struct file *filep, char * buff, u32 count)
{
    /** TODO Implementation of File write, 
    *   You should be writing the content from buff to File by using File system write function
    *   Validate the permission, file existence, Max length etc
    *   Incase of Error return valid Error code 
    * */
  if(!filep){return -1;}
  if(((filep->mode&O_WRITE))){
     
  u64 i, size, s_pos;
  long int offp = filep->offp;
	long int max_len;
        max_len = (long int)filep->inode->e_pos - offp - (long int)filep->inode->s_pos;

  if( count > max_len )
  {
    
    return -1; // file_size exceeded
  }
  s_pos = filep->inode->s_pos;
  for( i=0 ; i<count ; i++)
  {
    *(char *)(s_pos + offp + i) = buff[i];
  }

  if( filep->inode->s_pos + offp + count > filep->inode->max_pos )  //  an implementation of flat_write()
    filep->inode->max_pos = filep->inode->s_pos + offp + count;
  filep->inode->file_size = filep->inode->max_pos - filep->inode->s_pos;
    filep->offp = offp+count;			 //setoffset	
  return count;
  }
    int ret_fd = -EINVAL; 
    return ret_fd;
}

static long do_lseek_regular(struct file *filep, long offset, int whence)
{
    /** TODO Implementation of lseek 
    *   Set, Adjust the ofset based on the whence
    *   Incase of Error return valid Error code 
    * */
    long var = filep->offp;  //setting all cases for SEEK
    if(filep==NULL){
      return -1;
    }
    if(whence == SEEK_SET){
      filep->offp = offset;
    }
    else if(whence == SEEK_CUR){
      filep->offp += offset; 
    }
    else if(whence == SEEK_END){
      filep->offp = filep->inode->file_size + offset; 
    }
    else { return -1; }
    if(filep->offp<0 || filep->offp > filep->inode->file_size ){
      filep->offp=var;
      return -1;}
    return filep->offp;

    //int ret_fd = -1; 
    //return ret_fd;
}

extern int do_regular_file_open(struct exec_context *ctx, char* filename, u64 flags, u64 mode)
{ 
  /**  TODO Implementation of file open, 
    *  You should be creating file(use the alloc_file function to creat file), 
    *  To create or Get inode use File system function calls, 
    *  Handle mode and flags 
    *  Validate file existence, Max File count is 32, Max Size is 4KB, etc
    *  Incase of Error return valid Error code 
    * */
    if(!ctx){return -1;}
    struct file *nfile;
    struct inode* inode=lookup_inode(filename);
    //  if ( inode && (flags&O_CREAT)){
    //    return -1;
    //   }
    int i = 3;
    while(((ctx->files)[i])!=NULL){
      if(i==MAX_OPEN_FILES){
        return -1;
        break;
      }
      i++;
    }
    int ret_fd=i;
    if (inode==NULL){
      if ((flags&O_CREAT)){  //is file is not already created and opened with O_CREAT flag then creat inode for it 
        inode = create_inode(filename,mode);
        if(!inode){
          return -1;
        }
        nfile = alloc_file();

        nfile->type= REGULAR;
        nfile->mode=flags;
        nfile->inode=inode;
        nfile->ref_count=1;
      }
      else{return -1;}
    }
    else{
      //inode = lookup_inode(filename);
    // if(flags&O_CREAT ){ 
    //     return -1;
    //   }
    if(!((flags&O_READ) || (flags&O_EXEC) || (flags&O_WRITE))){ //permission check
      return -1;
    }
    if((flags&O_READ)&&(!(inode->mode & O_READ))){
        return -1;
    }
    if((flags&O_WRITE)&&(!(inode->mode & O_WRITE))){
        return -1;
    }
    if((flags&O_EXEC)&&(!(inode->mode & O_EXEC))){
        return -1;
    }
      nfile = alloc_file();
    }
    //int ret_fd = -EINVAL; 
    
    nfile->type= REGULAR;
    nfile->inode= inode;
    nfile->fops->close = generic_close;
    nfile->fops->write = do_write_regular;
    nfile->fops->read = do_read_regular;
    nfile->fops->lseek = do_lseek_regular;
    nfile->mode=flags;
    nfile->offp=0;
    
    (ctx->files)[i]= nfile;
    (ctx->files)[i]->ref_count=1;
    
    return ret_fd;
}

int fd_dup(struct exec_context *current, int oldfd)
{
     /** TODO Implementation of dup 
      *  Read the man page of dup and implement accordingly 
      *  return the file descriptor,
      *  Incase of Error return valid Error code 
      * */
    if(!current){return -1;}
    if(oldfd > MAX_OPEN_FILES || oldfd <0 ){
      return -1; // invalid fd
    }
    if(!current->files[oldfd]){
      return -1; // old fd is pointing to NULL 
    }
    struct file* nfile = (current->files)[oldfd];
    int i = 0;
    while(((current->files)[i])!=NULL){
      if(i==MAX_OPEN_FILES){
        break;
      }
      i++;
    }
    (current->files)[i]= nfile;
    (current->files)[i]->ref_count++;
    return i;
}


int fd_dup2(struct exec_context *current, int oldfd, int newfd)
{
  /** TODO Implementation of the dup2 
    *  Read the man page of dup2 and implement accordingly 
    *  return the file descriptor,
    *  Incase of Error return valid Error code 
    * */
    //generic_close((current->files)[oldfd]);
    if(!current){return -1;}
    if(oldfd > MAX_OPEN_FILES){
      return -1; // invalid fd
    }
    if(!current->files[oldfd]){
      return -1; // old fd is pointing to NULL 
    }
    if(newfd > MAX_OPEN_FILES){
      return -1; // invalid fd
    }
    if(oldfd == newfd){
      return oldfd; 
    }
    struct file* filename = (current->files)[oldfd];

    if(current->files[newfd]!=NULL){
      generic_close(current->files[newfd]);
    }
    (current->files)[newfd]= filename;
    (current->files)[newfd]->ref_count++;
    return newfd;
    int ret_fd = -EINVAL; 
    return ret_fd;
}
