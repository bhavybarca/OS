//BHAVY SINGH 170212
#include<pipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
/***********************************************************************
 * Use this function to allocate pipe info && Don't Modify below function
 ***********************************************************************/
struct pipe_info* alloc_pipe_info()
{
    struct pipe_info *pipe = (struct pipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);
    pipe ->pipe_buff = buffer;
    return pipe;
}


void free_pipe_info(struct pipe_info *p_info)
{
    if(!p_info)
    {
        os_page_free(OS_DS_REG ,p_info->pipe_buff);
        os_page_free(OS_DS_REG ,p_info);
    }
}
/*************************************************************************/
/*************************************************************************/



int pipe_read(struct file *filep, char *buff, u32 count)
{
    /**
    *  TODO:: Implementation of Pipe Read
    *  Read the contect from buff (pipe_info -> pipe_buff) and write to the buff(argument 2);
    *  Validate size of buff, the mode of pipe (pipe_info->mode),etc
    *  Incase of Error return valid Error code 
    */
    if(!filep){
        return -1;
    }
    if(filep->pipe->is_ropen != 1 || (buff==NULL) || count<0){
        return -1;
    }
    if( (filep->mode & O_READ) ==0 ){
        return -1;
    }
    
    u64 i, size = 0, r_pos;
    r_pos = filep->pipe->read_pos;   // start position for readend of pipe
    if(count > filep->pipe->buffer_offset){
        return -1; //error handling
    }

    for( i=0 ; i<count ; i++)
    {
      buff[i] =filep->pipe->pipe_buff[r_pos++]; //reading from pipe
        r_pos%=PIPE_MAX_SIZE; //modulo arithematic
        filep->pipe->buffer_offset--; //offset is set here

    }
    filep->pipe->read_pos = r_pos;
    return i;

    int ret_fd = -EINVAL; 
    return ret_fd;
}


int pipe_write(struct file *filep, char *buff, u32 count)
{ 
    /**
    *  TODO:: Implementation of Pipe Read
    *  Write the contect from   the buff(argument 2);  and write to buff(pipe_info -> pipe_buff)
    *  Validate size of buff, the mode of pipe (pipe_info->mode),etc
    *  Incase of Error return valid Error code 
    */

    if(filep->pipe->is_wopen != 1 || (buff==NULL) || count<0 ){ // error handling
        return -1;
    }
    if(!filep){
        return -1;
    }
    if( (filep->mode & O_WRITE) ==0 ){
        return -1;
    }
    u64 i, size, s_pos;

    if( (u32)filep->pipe->buffer_offset + count > 4096 ) // error handling
    {
        return -1; // file_size exceeded
    }
    s_pos =  filep->pipe->write_pos ;
    for( i=0 ; i<count ; i++)
    {
        filep->pipe->pipe_buff[s_pos++] = buff[i];
        s_pos%=4096;
    }
    filep->pipe->write_pos = s_pos;
    // if(filep->pipe->buffer_offset < s_pos+count){
        filep->pipe->buffer_offset += count; //offset is set
    // }
    // filep->pipe->pipe_buff+=count;

    return count;

    int ret_fd = -EINVAL; 
    return ret_fd;
}

int create_pipe(struct exec_context *current, int *fd)
{
    /**
    *  TODO:: Implementation of Pipe Create
    *  Create file struct by invoking the alloc_file() function, 
    *  Create pipe_info struct by invoking the alloc_pipe_info() function
    *  fill the valid file descriptor in *fd param
    *  Incase of Error return valid Error code 
    */
    if(!current)
      return -1;
    struct file* n1file = alloc_file();
    struct file* n2file = alloc_file();
    struct pipe_info* mypipe =  alloc_pipe_info();
    if(!n1file || !n2file || !mypipe){
        free_file_object(n1file);
        free_file_object(n2file);
        free_pipe_info(mypipe);
        return -1;
        }
    

    n1file->fops->close = generic_close; //populate read end of pipe
    n1file->fops->write = pipe_write;
    n1file->fops->read = pipe_read;
    n1file->mode = O_READ;
    n1file->type = PIPE;
    n1file->inode = NULL;
    n1file->pipe = mypipe;
    //n1file->ref_count=0;
    n2file->fops->close = generic_close; // populate write end of pipe
    n2file->fops->write = pipe_write;
    n2file->fops->read = pipe_read;
    n2file->mode= O_WRITE;
    n2file->type = PIPE;
    n2file->inode = NULL;
    n2file->pipe = mypipe;


    mypipe->read_pos=0;       //Last Read position
    mypipe->write_pos=0;      // Last Write position
    mypipe->buffer_offset=0; 
    mypipe->is_wopen=1;
    mypipe->is_ropen=1;


    int i = 3;
    while(((current->files)[i])!=NULL){ //find empty fd 
      if(i==MAX_OPEN_FILES){
        return -EINVAL;
      }
      i++;
    }
    int ret_fd;
    ret_fd=i;
    i++;

    while(((current->files)[i])!=NULL){
      if(i==MAX_OPEN_FILES){
        return -EINVAL;
      }
      i++;
    }
    (current->files)[ret_fd]= n1file;
    n1file->ref_count=1;
    (current->files)[i]= n2file;
    n2file->ref_count=1;
    fd[0]=ret_fd;
    fd[1]=i;

    if(i<MAX_OPEN_FILES) return 0; // error handling
    else{
        free_file_object(n1file);
        free_file_object(n2file);
        free_pipe_info(mypipe);
        return -1;
    }
    // ret_fd = -EINVAL; 
    // return ret_fd;
}

