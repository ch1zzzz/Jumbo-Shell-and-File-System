#include "jumbo_file_system.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// C does not have a bool type, so I created one that you can use
typedef char bool_t;
#define TRUE 1
#define FALSE 0


static block_num_t current_dir;


// optional helper function you can implement to tell you if a block is a dir node or an inode
static bool_t is_dir(block_num_t block_num) {
  void *buf = malloc(BLOCK_SIZE);
  read_block(block_num, buf);
  struct block *b = (struct block *) buf; 
  if((*b).is_dir == 0){
    free(buf);
    return TRUE;
  }else{
    free(buf);
    return FALSE;
  }
}


/* jfs_mount
 *   prepares the DISK file on the _real_ file system to have file system
 *   blocks read and written to it.  The application _must_ call this function
 *   exactly once before calling any other jfs_* functions.  If your code
 *   requires any additional one-time initialization before any other jfs_*
 *   functions are called, you can add it here.
 * filename - the name of the DISK file on the _real_ file system
 * returns 0 on success or -1 on error; errors should only occur due to
 *   errors in the underlying disk syscalls.
 */
int jfs_mount(const char* filename) {
  int ret = bfs_mount(filename);
  current_dir = 1;
  return ret;
}


/* jfs_mkdir
 *   creates a new subdirectory in the current directory
 * directory_name - name of the new subdirectory
 * returns 0 on success or one of the following error codes on failure:
 *   E_EXISTS, E_MAX_NAME_LENGTH, E_MAX_DIR_ENTRIES, E_DISK_FULL
 */
int jfs_mkdir(const char* directory_name) {
  //read directory_block of current directory
  void *buf = malloc(BLOCK_SIZE);
  read_block(current_dir, buf);
  struct block *cur_dir = (struct block *) buf;
  //check number of DIR_ENTRIES
  uint16_t *num_ent = &((*cur_dir).contents.dirnode.num_entries);
  if(*num_ent == MAX_DIR_ENTRIES){
    free(buf);
    return E_MAX_DIR_ENTRIES;
  }
  //check length of name
  int len = strlen(directory_name);
  if(len > MAX_NAME_LENGTH){
    free(buf);
    return E_MAX_NAME_LENGTH;
  }
  //check if the directory exists
  for(int i = 0; i < *num_ent; i++){
    if(strcmp(directory_name, (*cur_dir).contents.dirnode.entries[i].name) == 0){
      free(buf);
      return E_EXISTS;
    }
  }
  block_num_t block_num_new = allocate_block();
  //check if the disk is full
  if(block_num_new == 0){
    free(buf);
    return E_DISK_FULL;
  }
  //modify the information in current directory  
  *num_ent += 1;
  (*cur_dir).contents.dirnode.entries[(*num_ent) - 1].block_num = block_num_new;
  strncpy((*cur_dir).contents.dirnode.entries[(*num_ent) - 1].name, directory_name, len + 1);
  //write back the current directory
  write_block(current_dir, buf);
  free(buf);
  //Configure information for the new directory block
  void *buf2 = malloc(BLOCK_SIZE);
  read_block(block_num_new, buf2);
  struct block *new_dir = (struct block *) buf2;
  (*new_dir).is_dir = 0;
  (*new_dir).contents.dirnode.num_entries = 0;
  write_block(block_num_new, buf2);
  free(buf2);
  return E_SUCCESS;
}


/* jfs_chdir
 *   changes the current directory to the specified subdirectory, or changes
 *   the current directory to the root directory if the directory_name is NULL
 * directory_name - name of the subdirectory to make the current
 *   directory; if directory_name is NULL then the current directory
 *   should be made the root directory instead
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_NOT_DIR
 */
int jfs_chdir(const char* directory_name) {
  //check if the directory_name is NULL
  if(directory_name == NULL){
    current_dir = 1;
    return E_SUCCESS;
  }
  //read directory_block of current directory
  void *buf = malloc(BLOCK_SIZE);
  read_block(current_dir, buf);
  struct block *cur_dir = (struct block *) buf;
  uint16_t *num_ent = &((*cur_dir).contents.dirnode.num_entries);
  //check if the name exists or is a directory name
  for(int i = 0; i < *num_ent; i++){
    if(strcmp(directory_name, (*cur_dir).contents.dirnode.entries[i].name) == 0){
      if(is_dir((*cur_dir).contents.dirnode.entries[i].block_num)){
        current_dir = (*cur_dir).contents.dirnode.entries[i].block_num;
        free(buf);
        return E_SUCCESS;
      }else{
        free(buf);
        return E_NOT_DIR;
      }
    }
  }
  free(buf);
  return E_NOT_EXISTS;
}


/* jfs_ls
 *   finds the names of all the files and directories in the current directory
 *   and writes the directory names to the directories argument and the file
 *   names to the files argument
 * directories - array of strings; the function will set the strings in the
 *   array, followed by a NULL pointer after the last valid string; the strings
 *   should be malloced and the caller will free them
 * file - array of strings; the function will set the strings in the
 *   array, followed by a NULL pointer after the last valid string; the strings
 *   should be malloced and the caller will free them
 * returns 0 on success or one of the following error codes on failure:
 *   (this function should always succeed)
 */
int jfs_ls(char* directories[MAX_DIR_ENTRIES+1], char* files[MAX_DIR_ENTRIES+1]) {
  //read directory_block of current directory
  void *buf = malloc(BLOCK_SIZE);
  read_block(current_dir, buf);
  struct block *cur_dir = (struct block *) buf;
  uint16_t *num_ent = &((*cur_dir).contents.dirnode.num_entries);
  //copy name into array
  unsigned long long int dir_num = 0;
  unsigned long long int file_num = 0;
  for(int i = 0; i < *num_ent; i++){
    int len = strlen((*cur_dir).contents.dirnode.entries[i].name);
    if(is_dir((*cur_dir).contents.dirnode.entries[i].block_num)){     
      directories[dir_num] = (char *)malloc(len + 1);
      strncpy(directories[dir_num], (*cur_dir).contents.dirnode.entries[i].name, len + 1);
      dir_num += 1;
    }else{
      files[file_num] = (char *)malloc(len + 1);
      strncpy(files[file_num], (*cur_dir).contents.dirnode.entries[i].name, len + 1);
      file_num += 1;
    }
  }
  //fill the rest of array with NULL
  while(dir_num < MAX_DIR_ENTRIES + 1){
    directories[dir_num] = NULL;
    dir_num += 1;
  }
  while(file_num < MAX_DIR_ENTRIES + 1){
    files[file_num] = NULL;
    file_num += 1;
  }
  free(buf);
  return E_SUCCESS;
}


/* jfs_rmdir
 *   removes the specified subdirectory of the current directory
 * directory_name - name of the subdirectory to remove
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_NOT_DIR, E_NOT_EMPTY
 */
int jfs_rmdir(const char* directory_name) {
  //read directory_block of current directory
  void *buf = malloc(BLOCK_SIZE);
  read_block(current_dir, buf);
  struct block *cur_dir = (struct block *) buf;
  uint16_t *num_ent = &((*cur_dir).contents.dirnode.num_entries);
  //check if the name exists or is a directory name
  for(int i = 0; i < *num_ent; i++){
    if(strcmp(directory_name, (*cur_dir).contents.dirnode.entries[i].name) == 0){
      if(is_dir((*cur_dir).contents.dirnode.entries[i].block_num)){
        block_num_t rm_dir = (*cur_dir).contents.dirnode.entries[i].block_num;        
        //read directory_block of rm_dir
        void *buf2 = malloc(BLOCK_SIZE);
        read_block(rm_dir, buf2);
        struct block *remove_dir = (struct block *) buf2;
        //check if the dir is empty
        if((*remove_dir).contents.dirnode.num_entries != 0){
          free(buf);
          free(buf2);
          return E_NOT_EMPTY;
        }
        //remove the directory(swap the last directory or file with remove_dir)       
        if(i != *num_ent - 1){
          (*cur_dir).contents.dirnode.entries[i].block_num = (*cur_dir).contents.dirnode.entries[*num_ent - 1].block_num;
          strncpy((*cur_dir).contents.dirnode.entries[i].name, 
          (*cur_dir).contents.dirnode.entries[*num_ent - 1].name, 
          strlen((*cur_dir).contents.dirnode.entries[*num_ent - 1].name) + 1);
        }
        (*cur_dir).contents.dirnode.num_entries -= 1;
        write_block(current_dir, buf);
        release_block(rm_dir);
        free(buf);
        free(buf2);
        return E_SUCCESS;
      }else{
        free(buf);
        return E_NOT_DIR;
      }
    }
  }
  free(buf);
  return E_NOT_EXISTS;
}


/* jfs_creat
 *   creates a new, empty file with the specified name
 * file_name - name to give the new file
 * returns 0 on success or one of the following error codes on failure:
 *   E_EXISTS, E_MAX_NAME_LENGTH, E_MAX_DIR_ENTRIES, E_DISK_FULL
 */
int jfs_creat(const char* file_name) {
  //read directory_block of current directory
  void *buf = malloc(BLOCK_SIZE);
  read_block(current_dir, buf);
  struct block *cur_dir = (struct block *) buf;
  //check number of DIR_ENTRIES
  uint16_t *num_ent = &((*cur_dir).contents.dirnode.num_entries);
  if(*num_ent == MAX_DIR_ENTRIES){
    free(buf);
    return E_MAX_DIR_ENTRIES;
  }
  //check length of name
  int len = strlen(file_name);
  if(len > MAX_NAME_LENGTH){
    free(buf);
    return E_MAX_NAME_LENGTH;
  }
  //check if the file exists
  for(int i = 0; i < *num_ent; i++){
    if(strcmp(file_name, (*cur_dir).contents.dirnode.entries[i].name) == 0){
      free(buf);
      return E_EXISTS;
    }
  }
  block_num_t block_num_new = allocate_block();
  //check if the disk is full
  if(block_num_new == 0){
    free(buf);
    return E_DISK_FULL;
  }
  //modify the information in current directory  
  *num_ent += 1;
  (*cur_dir).contents.dirnode.entries[(*num_ent) - 1].block_num = block_num_new;
  strncpy((*cur_dir).contents.dirnode.entries[(*num_ent) - 1].name, file_name, len + 1);
  //write back the current directory
  write_block(current_dir, buf);
  free(buf);
  //Configure information for the new file(inode)
  void *buf2 = malloc(BLOCK_SIZE);
  read_block(block_num_new, buf2);
  struct block *new_file = (struct block *) buf2;
  (*new_file).is_dir = 1;
  (*new_file).contents.inode.file_size = 0;
  write_block(block_num_new, buf2);
  free(buf2);
  return E_SUCCESS;
}


/* jfs_remove
 *   deletes the specified file and all its data (note that this cannot delete
 *   directories; use rmdir instead to remove directories)
 * file_name - name of the file to remove
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_IS_DIR
 */
int jfs_remove(const char* file_name) {
  //read directory_block of current directory
  void *buf = malloc(BLOCK_SIZE);
  read_block(current_dir, buf);
  struct block *cur_dir = (struct block *) buf;
  uint16_t *num_ent = &((*cur_dir).contents.dirnode.num_entries);
  //check if the name exists or is a directory name
  for(int i = 0; i < *num_ent; i++){
    if(strcmp(file_name, (*cur_dir).contents.dirnode.entries[i].name) == 0){
      if(!is_dir((*cur_dir).contents.dirnode.entries[i].block_num)){
        block_num_t rm_file = (*cur_dir).contents.dirnode.entries[i].block_num;        
        //read inode of rm_file
        void *buf2 = malloc(BLOCK_SIZE);
        read_block(rm_file, buf2);
        struct block *remove_file = (struct block *) buf2;
        //remove the file in current dir(swap the last file or directory with remove_file)
        (*cur_dir).contents.dirnode.entries[i].block_num = (*cur_dir).contents.dirnode.entries[*num_ent - 1].block_num;
        if(i != *num_ent - 1){
          strncpy((*cur_dir).contents.dirnode.entries[i].name, 
          (*cur_dir).contents.dirnode.entries[*num_ent - 1].name, 
          strlen((*cur_dir).contents.dirnode.entries[*num_ent - 1].name) + 1);
        }
        (*cur_dir).contents.dirnode.num_entries -= 1;
        write_block(current_dir, buf);
        //release inode and all of the data blocks
        uint32_t size = remove_file->contents.inode.file_size;
        uint32_t data_block_total = size / BLOCK_SIZE;
        if(size % BLOCK_SIZE != 0){
          data_block_total += 1;
        }
        for(uint32_t j = 0; j < data_block_total; j++){
          release_block(remove_file->contents.inode.data_blocks[j]);
        }
        release_block(rm_file);
        free(buf);
        free(buf2);
        return E_SUCCESS;
      }else{
        free(buf);
        return E_IS_DIR;
      }
    }
  }
  free(buf);
  return E_NOT_EXISTS;
}


/* jfs_stat
 *   returns the file or directory stats (see struct stat for details)
 * name - name of the file or directory to inspect
 * buf  - pointer to a struct stat (already allocated by the caller) where the
 *   stats will be written
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS
 */
int jfs_stat(const char* name, struct stats* buf) {
  //read directory_block of current directory
  void *buf1 = malloc(BLOCK_SIZE);
  read_block(current_dir, buf1);
  struct block *cur_dir = (struct block *) buf1;
  uint16_t *num_ent = &((*cur_dir).contents.dirnode.num_entries);
  //check if the name exists
  for(int i = 0; i < *num_ent; i++){
    if(strcmp(name, (*cur_dir).contents.dirnode.entries[i].name) == 0){
      //read the block of name
      block_num_t data = (*cur_dir).contents.dirnode.entries[i].block_num;        
      void *buf2 = malloc(BLOCK_SIZE);
      read_block(data, buf2);
      struct block *file_or_dir = (struct block *) buf2;
      //read the information to buf
      strncpy(buf->name, name, strlen(name) + 1);
      buf->block_num = data;
      if(is_dir(data)){
        //the block is a directory
        buf->is_dir = 0;
      }else{
        //the block is a file
        buf->is_dir = 1;
        buf->file_size = file_or_dir->contents.inode.file_size;
        uint32_t data_block_total = buf->file_size / BLOCK_SIZE;
        if(buf->file_size % BLOCK_SIZE != 0){
          data_block_total += 1;
        }
        buf->num_data_blocks = data_block_total;
      }
      free(buf1);
      free(buf2);
      return E_SUCCESS;
    }
  }
  free(buf1);
  return E_NOT_EXISTS;
}


/* jfs_write
 *   appends the data in the buffer to the end of the specified file
 * file_name - name of the file to append data to
 * buf - buffer containing the data to be written (note that the data could be
 *   binary, not text, and even if it is text should not be assumed to be null
 *   terminated)
 * count - number of bytes in buf (write exactly this many)
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_IS_DIR, E_MAX_FILE_SIZE, E_DISK_FULL
 */
int jfs_write(const char* file_name, const void* buf, unsigned short count) {
  //read directory_block of current directory
  void *buf1 = malloc(BLOCK_SIZE);
  read_block(current_dir, buf1);
  struct block *cur_dir = (struct block *) buf1;
  uint16_t *num_ent = &((*cur_dir).contents.dirnode.num_entries);
  for(int i = 0; i < *num_ent; i++){
    if(strcmp(file_name, (*cur_dir).contents.dirnode.entries[i].name) == 0){
      //check if the file is dir or file
      if(is_dir((*cur_dir).contents.dirnode.entries[i].block_num)){
        free(buf1);
        return E_IS_DIR;
      }else{
        //read the inode of file
        void *buf2 = malloc(BLOCK_SIZE);
        block_num_t file_num = (*cur_dir).contents.dirnode.entries[i].block_num;
        read_block(file_num, buf2);
        struct block *write_to_file = (struct block *) buf2;
        //check if the size after writing is too large
        uint32_t original_size = write_to_file->contents.inode.file_size;
        uint32_t after_size = original_size + count;
        if(after_size > MAX_FILE_SIZE){
          free(buf1);
          free(buf2);
          return E_MAX_FILE_SIZE;
        }
        //calculate the number of blocks need to be add
        uint32_t data_block_total_ori = original_size / BLOCK_SIZE;
        if(original_size != 0 && original_size % BLOCK_SIZE != 0){
          data_block_total_ori += 1;
        }
        uint32_t data_block_total_aft = after_size / BLOCK_SIZE;
        if(after_size != 0 && after_size % BLOCK_SIZE != 0){
          data_block_total_aft += 1;
        }
        int32_t add_block = data_block_total_aft - data_block_total_ori;
        //check if the disk is full after add new blocks
        block_num_t add_block_num[add_block];
        int32_t j = 0;
        while(j < add_block){
          add_block_num[j] = allocate_block();
          if(add_block_num[j] == 0){
            //disk is full, release all blocks have been allocated
            j -= 1;
            while(j >= 0){
              release_block(add_block_num[j]);
              j--;
            }
            free(buf1);
            free(buf2);
            return E_DISK_FULL;
          }
          j += 1;
        }
        //change the information in inode of file
        write_to_file->contents.inode.file_size = after_size;
        for(int32_t k = 0; k < add_block; k++){
          write_to_file->contents.inode.data_blocks[k + data_block_total_ori] = add_block_num[k];
        }
        write_block(file_num, buf2);
        //append data to block(get the last block of file to buf3, append buf to buf3, write it all, start at the original last block)
        uint32_t offset = original_size % BLOCK_SIZE;
        void *buf4;
        if(original_size != 0){
          //the last node has space to append
          buf4 = malloc(BLOCK_SIZE * (add_block + 1));
          memset(buf4, -1, BLOCK_SIZE * (add_block + 1));
          void *buf3 = malloc(BLOCK_SIZE);
          read_block(write_to_file->contents.inode.data_blocks[data_block_total_ori - 1], buf3);
          memcpy(buf4, buf3, BLOCK_SIZE);
          free(buf3);
          if(offset == 0){
            memcpy(buf4 + BLOCK_SIZE, buf, count);
          }else{
            memcpy(buf4 + offset, buf, count);
          } 
        }else{
          buf4 = malloc(BLOCK_SIZE * add_block);
          memset(buf4, -1, BLOCK_SIZE * add_block);
          memcpy(buf4, buf, count);
        }
        
        
        uint32_t start = 0;
        if(data_block_total_ori > 0){
          start = data_block_total_ori - 1;
        }
        for(uint32_t q = 0; start < data_block_total_aft; q++){
          write_block(write_to_file->contents.inode.data_blocks[start], buf4 + BLOCK_SIZE * q);
          start += 1;
        }
        free(buf1);
        free(buf2);
        free(buf4);
        return E_SUCCESS;
      }
    }
  }
  free(buf1);
  return E_NOT_EXISTS;
}


/* jfs_read
 *   reads the specified file and copies its contents into the buffer, up to a
 *   maximum of *ptr_count bytes copied (but obviously no more than the file
 *   size, either)
 * file_name - name of the file to read
 * buf - buffer where the file data should be written
 * ptr_count - pointer to a count variable (allocated by the caller) that
 *   contains the size of buf when it's passed in, and will be modified to
 *   contain the number of bytes actually written to buf (e.g., if the file is
 *   smaller than the buffer) if this function is successful
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_IS_DIR
 */
int jfs_read(const char* file_name, void* buf, unsigned short* ptr_count) {
  //read directory_block of current directory
  void *buf1 = malloc(BLOCK_SIZE);
  read_block(current_dir, buf1);
  struct block *cur_dir = (struct block *) buf1;
  uint16_t *num_ent = &((*cur_dir).contents.dirnode.num_entries);
  for(int i = 0; i < *num_ent; i++){
    if(strcmp(file_name, (*cur_dir).contents.dirnode.entries[i].name) == 0){
      //check if the file is dir or file
      if(is_dir((*cur_dir).contents.dirnode.entries[i].block_num)){
        free(buf1);
        return E_IS_DIR;
      }else{
        void *buf2 = malloc(BLOCK_SIZE);
        block_num_t file_num = (*cur_dir).contents.dirnode.entries[i].block_num;
        read_block(file_num, buf2);
        struct block *read_file = (struct block *) buf2;
        uint32_t size = read_file->contents.inode.file_size;
        if(size < *ptr_count){
          *ptr_count = size;
        }
        int32_t read_size = (int32_t) *ptr_count;
        uint32_t i = 0;
        //read data into buf3 then copy to buf
        uint32_t data_block_total = read_size / BLOCK_SIZE;
        if(read_size != 0 && read_size % BLOCK_SIZE != 0){
          data_block_total += 1;
        }
        void *buf3 = malloc(BLOCK_SIZE * (data_block_total + 1));
        while(read_size > 0){
          read_block(read_file->contents.inode.data_blocks[i], buf3 + i * BLOCK_SIZE);
          i += 1;
          read_size -= BLOCK_SIZE;
        }
        memcpy(buf, buf3, *ptr_count);
        free(buf1);
        free(buf2);
        free(buf3);
        return E_SUCCESS;
      }
    }
  }
  free(buf1);
  return E_NOT_EXISTS;
}


/* jfs_unmount
 *   makes the file system no longer accessible (unless it is mounted again).
 *   This should be called exactly once after all other jfs_* operations are
 *   complete; it is invalid to call any other jfs_* function (except
 *   jfs_mount) after this function complete.  Basically, this closes the DISK
 *   file on the _real_ file system.  If your code requires any clean up after
 *   all other jfs_* functions are done, you may add it here.
 * returns 0 on success or -1 on error; errors should only occur due to
 *   errors in the underlying disk syscalls.
 */
int jfs_unmount() {
  int ret = bfs_unmount();
  return ret;
}
