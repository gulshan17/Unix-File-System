#include "fs.h"
#include <math.h>
#include <string.h>

unsigned char* fs;

void mapfs(int fd){
  if ((fs = (unsigned char *)mmap(NULL, FSSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == NULL){
      perror("mmap failed");
      exit(EXIT_FAILURE);
  }
}


void unmapfs(){
  munmap(fs, FSSIZE);
}


void formatfs(char * fsname){
  unsigned int num_of_block = (unsigned int)(FSSIZE / BLOCK_SIZE);
  unsigned int block_inode_ratio = BLOCK_SIZE / INODE_SIZE;
  unsigned int num_inodes = (unsigned int)((num_of_block - 1) * block_inode_ratio / (block_inode_ratio + 1));
  unsigned int num_of_blocks_inode = floor(num_inodes / block_inode_ratio);
  unsigned int num_of_blocks_data = (num_of_block - num_of_blocks_inode - 1);
  float num_of_blocks_free_block_list = ceil(((num_of_blocks_data / 8.0) + (num_of_blocks_data & 7)) / BLOCK_SIZE);
  num_of_blocks_data -= num_of_blocks_free_block_list;
  if (num_inodes > num_of_blocks_data) {
    num_of_blocks_inode -= (num_inodes - num_of_blocks_data);
    num_inodes = (num_of_blocks_inode * block_inode_ratio);
    num_of_blocks_data = num_of_block - 1 - num_of_blocks_free_block_list - num_of_blocks_inode;
  }
  struct superblock * super_block = (struct superblock *) fs;
  strcpy(super_block->FS_name, fsname);
  super_block->total_blocks = num_of_block;
  super_block->data_blocks = num_of_blocks_data;
  super_block->block_size = BLOCK_SIZE;
  super_block->first_data_block = num_of_block - num_of_blocks_data;
  super_block->first_freelist_block = 1;
  super_block->first_inode_block = 1 + num_of_blocks_free_block_list;

  // creating root node
  struct inode *root = (struct inode *) (((struct block *) (fs)) + (super_block->first_inode_block));
  root->type = 'd'; // directory
  root->size = 0; //incase of directory number of files else bytes of data in case of regular file.
  char * freelist = (char *)(((struct block *) (fs)) + 1);
  freelist[0] =  freelist[0] | 1;
}

void lsfs(){
  struct superblock *sb = (struct superblock *)fs;
  struct inode * rootinode = (struct inode *)(((struct block *) (fs)) + (sb->first_inode_block));
  if(rootinode->size) {
    char path[256];
    path[0] = '\0';
    traverse_dfs(rootinode, path);
  } else {
    printf("/\n");
  }
}

void traverse_dfs(struct inode *parentinode, char path[256]) {
  if(((parentinode->size) == 0) || ((parentinode->type) == 'f')) {
    printf("%s\n", path);
    char *c = strrchr(path, '/');
    *c = '\0';
    return;
  }
  unsigned short int init_l = strlen(path);
  struct superblock *sb = (struct superblock *)fs;
  struct dir_entry *content = (struct dir_entry *)(((struct block *) fs) + (sb->first_data_block) + (parentinode->contents[0]));
    int i = 0;
    int j = 0;
    int k = 0;
    while(i++ < (parentinode->size)) {
      strcat(path, "/");
      strcat(path, content->name);
      traverse_dfs(((struct inode *)((((struct block *) fs) + sb->first_inode_block))) + (content->inode), path);
      path[init_l] = '\0';
      ++j;
      if (j >= 16) {
        j = 0;
        content = (struct dir_entry *)(((struct block *) fs) + parentinode->contents[++k]);
      } else {
        ++content;
      }
    }
}

void addfilefs(char* fname){
  struct superblock *sb = (struct superblock *)fs;
  struct inode *root_inode = (struct inode *)(((struct block *) (fs)) + (sb->first_inode_block));
  add_fs(root_inode, fname);
}

int extract_file_name_from_path(char *path, char * name) {
  unsigned short int i = 0, j = 0, len = strlen(path);
  path = strtok(path, "\r\t\n ");
  if (path[0] == '/') {
    ++i;
  }
  while((i < len) && (path[i] != '/')) {
    name[j++] = path[i++];
  }
  name[j] = '\0';
  return i;
}

int find_empty_inode() {
  struct superblock * super_block = (struct superblock *) fs;
  struct inode * first_inode = ((struct inode *) (((struct block *) (fs)) + (super_block->first_inode_block)));
  struct inode * first_data_block = (struct inode *) (((struct block *) (fs)) + (super_block->first_data_block));
  int i = 1;
  while((first_inode + i) < first_data_block) {
    if (((first_inode + i) -> type) == 0) {
      return i;
    }
    ++i;
  }
  return -1;
}

int find_empty_datablock() {
  struct superblock * super_block = (struct superblock *) fs;
  char * freelist = (char *)(((struct block *) (fs)) + 1);
  int i = 0;
  int j = 0;
  int k = 0;
  while (i < (super_block->data_blocks)) {
    if (((freelist[k] >> j) & 1) == 0) {
      return i;
    }
    ++j;
    ++i;
    if (j >= 8) {
      ++k;
      j = 0;
    }
  }
  return -1;
}

struct inode* add_file(struct inode * parent_fileinode, char * name, char filetype) {
  int free_inode_pos = find_empty_inode();
  if (free_inode_pos == -1) {
    printf("Storage Full\n");
    return NULL;
  }
  struct superblock * super_block = (struct superblock *) fs;
  struct inode * free_inode = ((struct inode *) (((struct block *) (fs)) + (super_block->first_inode_block))) + free_inode_pos;
  int block_dir_capacity = BLOCK_SIZE >> 5;
  struct dir_entry * d_entry;
  if ((parent_fileinode->size) % (block_dir_capacity) == 0) {
    int free_datablock_pos = find_empty_datablock();
    if (free_datablock_pos == -1) {
      printf("Storage Full\n");
      return NULL;
    }
    d_entry = (struct dir_entry *)(((struct block *) (fs)) + (super_block->first_data_block) + free_datablock_pos);
    char * freelist = (char *)(((struct block *) (fs)) + 1);
    int i = free_datablock_pos >> 3;
    int j = free_datablock_pos & 7;
    freelist[i] |= (1 << j);
    parent_fileinode->contents[(unsigned short int) ((parent_fileinode->size) & 31)] = (unsigned short int) free_datablock_pos;
  } else {
    d_entry = ((struct dir_entry *)(((struct block *) (fs)) + (super_block->first_data_block) + ((parent_fileinode->contents[(int) ((parent_fileinode->size) / block_dir_capacity)])))) + ((parent_fileinode->size) % (block_dir_capacity));
  }
  strcpy(d_entry->name, name);
  d_entry->inode = free_inode_pos;
  free_inode->size = 0;
  free_inode->type = filetype;
  ++(parent_fileinode->size);
  return free_inode;
}

void add_fs(struct inode * parent_fileinode, char * path) {
  struct superblock * super_block = (struct superblock *) fs;
  int i = 1;
  char name[30], filetype = 'd';
  int l = extract_file_name_from_path(path, name);
  if(l >= strlen(path)) {
    filetype = 'f';
  }
  char file_found = 0;
  unsigned short int file_size = parent_fileinode->size;
  if(file_size) {
    int i = 0;
    int j = 0;
    int k = 0;
    struct dir_entry *content = (struct dir_entry *)(((struct block *) fs) + (super_block->first_data_block) + (parent_fileinode->contents[0]));
    while(i++ < file_size) {
      if (!strcmp((content->name), name)) {
        file_found = 1;
        break;
      }
      ++j;
      if (j >= 16) {
        j = 0;
        content = (struct dir_entry *)(((struct block *) fs) + parent_fileinode->contents[++k]);
      } else {
        ++content;
      }
    }
    if (file_found) {
      if (filetype == 'd') {
        add_fs(((struct inode *)((((struct block *) fs) + super_block->first_inode_block))) + (content->inode), path + l);
      }
    } else {
      struct inode * new_inode = add_file(parent_fileinode, name, filetype);
      if ((new_inode != NULL) && (filetype == 'd')){
        add_fs(new_inode, path + l);
      }
    }
  } else {
      struct inode * new_inode = add_file(parent_fileinode, name, filetype);
      if ((new_inode != NULL) && (filetype == 'd')){
        add_fs(new_inode, path + l);
      }
  }
}

void writefilefs(char* fname, char * data){
  struct superblock *sb = (struct superblock *)fs;
  struct inode *root_inode = (struct inode *)(((struct block *) (fs)) + (sb->first_inode_block));
  write_fs(root_inode, fname, data);
}

void write_fs(struct inode * parent_fileinode, char* path, char * data) {
  struct superblock * super_block = (struct superblock *) fs;
  int i = 1;
  char name[30], filetype = 'd';
  int l = extract_file_name_from_path(path, name);
  if(l >= strlen(path)) {
    filetype = 'f';
  }
  char file_found = 0;
  unsigned short int file_size = parent_fileinode->size;
  if(file_size) {
    int i = 0;
    int j = 0;
    int k = 0;
    struct dir_entry *content = (struct dir_entry *)(((struct block *) fs) + (super_block->first_data_block) + (parent_fileinode->contents[0]));
    while(i++ < file_size) {
      if ((!strcmp((content->name), name)) && (((((struct inode *)((((struct block *) fs) + super_block->first_inode_block))) + (content->inode))->type) == filetype)) {
        file_found = 1;
        break;
      }
      ++j;
      if (j >= 16) {
        j = 0;
        content = (struct dir_entry *)(((struct block *) fs) + parent_fileinode->contents[++k]);
      } else {
        ++content;
      }
    }
    if (file_found) {
      if (filetype == 'd') {
        write_fs(((struct inode *)((((struct block *) fs) + super_block->first_inode_block))) + (content->inode), path + l, data);
      } else {
        f_write(((struct inode *)((((struct block *) fs) + super_block->first_inode_block))) + (content->inode), data);
      }
    } else {
      printf("\n%s FILE NOT FOUND \n", name);
    }
  } else {
    printf("\n%s FILE NOT FOUND \n", name);
  }
}

void f_write(struct inode * fileinode, char * data) {
  struct superblock * super_block = (struct superblock *) fs;
  int free_datablock_pos = find_empty_datablock();
    if (free_datablock_pos == -1) {
      printf("Storage Full\n");
      return;
    }
  fileinode->contents[0] = free_datablock_pos;
  unsigned short int l = (unsigned short int) strlen(data);
  l = l > 512 ? 512 : l;
  fileinode->size = l;
  char* data_block = (char*) (((struct block *) (fs)) + (super_block->first_data_block) + free_datablock_pos);
  for(int i = 0; i < l; ++i) {
    data_block[i] = data[i];
  }
  char * freelist = (char *)(((struct block *) (fs)) + 1);
  int i = free_datablock_pos >> 3;
  int j = free_datablock_pos & 7;
  freelist[i] |= (1 << j);
}

void readfilefs(char* fname){
  struct superblock *sb = (struct superblock *)fs;
  struct inode *root_inode = (struct inode *)(((struct block *) (fs)) + (sb->first_inode_block));
  read_fs(root_inode, fname);
}

void read_fs(struct inode * parent_fileinode, char* path) {
  struct superblock * super_block = (struct superblock *) fs;
  int i = 1;
  char name[30], filetype = 'd';
  int l = extract_file_name_from_path(path, name);
  if(l >= strlen(path)) {
    filetype = 'f';
  }
  char file_found = 0;
  unsigned short int file_size = parent_fileinode->size;
  if(file_size) {
    int i = 0;
    int j = 0;
    int k = 0;
    struct dir_entry *content = (struct dir_entry *)(((struct block *) fs) + (super_block->first_data_block) + (parent_fileinode->contents[0]));
    while(i++ < file_size) {
      if ((!strcmp((content->name), name)) && (((((struct inode *)((((struct block *) fs) + super_block->first_inode_block))) + (content->inode))->type) == filetype)) {
        file_found = 1;
        break;
      }
      ++j;
      if (j >= 16) {
        j = 0;
        content = (struct dir_entry *)(((struct block *) fs) + parent_fileinode->contents[++k]);
      } else {
        ++content;
      }
    }
    if (file_found) {
      // struct inode * fileinode = ((struct inode *)((((struct block *) fs) + super_block->first_inode_block))) + (content->inode);
      if (filetype == 'd') {
        read_fs(((struct inode *)((((struct block *) fs) + super_block->first_inode_block))) + (content->inode), path + l);
      } else {
        f_read(((struct inode *)((((struct block *) fs) + super_block->first_inode_block))) + (content->inode));
      }
    } else {
      printf("\n%s FILE NOT FOUND \n", name);
    }
  } else {
    printf("\n%s FILE NOT FOUND \n", name);
  }
}

void f_read(struct inode * fileinode) {
  struct superblock * super_block = (struct superblock *) fs;
  char* data_block = (char*) (((struct block *) (fs)) + (super_block->first_data_block) + (fileinode->contents[0]));
  printf("\nFile Contains:\n");
  for(int i = 0; i < fileinode->size; ++i) {
    printf("%c", data_block[i]);
  }
}

void removefilefs(char* fname){
  struct superblock *sb = (struct superblock *)fs;
  struct inode *root_inode = (struct inode *)(((struct block *) (fs)) + (sb->first_inode_block));
  remove_fs(root_inode, fname);
}

void remove_fs(struct inode *parent_inode, char *path) {
  struct superblock * super_block = (struct superblock *) fs;
  int i = 1;
  char name[30], filetype = 'd';
  int l = extract_file_name_from_path(path, name);
  if(l >= strlen(path)) {
    filetype = 'f';
  }
  char file_found = 0;
  unsigned short int file_size = parent_inode->size;
  if(file_size) {
    int i = 0;
    int j = 0;
    int k = 0;
    struct dir_entry *content = (struct dir_entry *)(((struct block *) fs) + (super_block->first_data_block) + (parent_inode->contents[0]));
    while(i++ < file_size) {
      if ((!strcmp((content->name), name)) && (((((struct inode *)((((struct block *) fs) + super_block->first_inode_block))) + (content->inode))->type) == filetype)) {
        file_found = 1;
        break;
      }
      ++j;
      if (j >= 16) {
        j = 0;
        content = (struct dir_entry *)(((struct block *) fs) + parent_inode->contents[++k]);
      } else {
        ++content;
      }
    }
    if (file_found) {
      struct inode * current_fileinode = ((struct inode *)((((struct block *) fs) + super_block->first_inode_block))) + (content->inode);
      if (filetype == 'd') {
        remove_fs(current_fileinode, path + l);
        if ((current_fileinode->size) == 0) {
          f_remove(current_fileinode);
          remove_file_ref_from_directory(parent_inode, content);
        }
      } else {
        f_remove(current_fileinode);
        remove_file_ref_from_directory(parent_inode, content);
      }
    } else {
      printf("\n%s FILE NOT FOUND \n", name);
    }
  } else {
    printf("\n%s FILE NOT FOUND \n", name);
  }
}

void remove_file_ref_from_directory(struct inode* parent_inode, struct dir_entry* file_name_address) {
  struct superblock * super_block = (struct superblock *) fs;
  unsigned int i, j;
  i = ((parent_inode->size) >> 5);
  j = ((parent_inode->size) & 31) - 1;
  struct dir_entry *content = ((struct dir_entry *)(((struct block *) fs) + (super_block->first_data_block) + (parent_inode->contents[i]))) + j;
  memcpy(file_name_address, content, 32);
  if(((parent_inode->size) & 31) == 1) {
    char * freelist = (char *)(((struct block *) (fs)) + 1);
    freelist[((parent_inode->contents[i]) >> 3)] &= ((1 << (((parent_inode->contents[i]) & 7))) ^ (~0));
  }
  --(parent_inode->size);
}

void f_remove(struct inode * fileinode) {
  unsigned short int l;
  if ((fileinode->type) == 'f') {
    l = (fileinode->size / BLOCK_SIZE) + 1;
  } else {
    l = fileinode->size;
  }
  struct superblock * super_block = (struct superblock *) fs;
  char * freelist = (char *)(((struct block *) (fs)) + 1);
  unsigned short int k;
  for (int i = 0; i < l; ++i) {
    freelist[((fileinode->contents[i]) >> 3)] &= ((1 << (((fileinode->contents[i]) & 7))) ^ (~0));
  }
  fileinode->type = 0;
}
