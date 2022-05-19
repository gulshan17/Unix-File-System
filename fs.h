#ifndef __FS_H__
#define __FS_H__
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define FSSIZE 10000000
#define BLOCK_SIZE 512
#define INODE_SIZE 32
// #define FS_NAME "FS_FILE"

struct block {
    char a[BLOCK_SIZE];
};

struct superblock {
    char FS_name[32];
    unsigned int total_blocks;
    unsigned int data_blocks;
    unsigned int block_size;
    unsigned char first_data_block;
    unsigned char first_inode_block;
    unsigned char first_freelist_block;
};

struct dir_entry {
    char name[30];
    unsigned short int inode;
};

struct directory {
    struct dir_entry content[(unsigned short int)(BLOCK_SIZE / 32)];
};

//each inode is of 32 byte size
struct inode{
    char type; // storing type directory or file d->directory, f->file
    unsigned short int size; //size in bytes
    unsigned short int contents[14];
};

extern unsigned char* fs;

void mapfs(int fd);
void unmapfs();
void formatfs(char *);
// void loadfs();
void lsfs();
void addfilefs(char* fname);
int extract_file_name_from_path(char *path, char * name);
void add(struct inode * fileinode, char * name);
void add_fs(struct inode * fileinode, char * path);
void removefilefs(char* fname);
void writefilefs(char* fname, char * data);
void write_fs(struct inode * parent_fileinode, char* path, char * data);
void f_write(struct inode * fileinode, char * data);
void readfilefs(char* fname);
void read_fs(struct inode * parent_fileinode, char* path);
void f_read(struct inode * fileinode);
void traverse_dfs(struct inode *parentinode, char path[256]);
void remove_fs(struct inode *parent_inode, char *path);
void f_remove(struct inode * fileinode);
void remove_file_ref_from_directory(struct inode* parent_inode, struct dir_entry* file_name_address);

#endif
