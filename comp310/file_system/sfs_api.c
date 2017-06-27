#include "sfs_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "disk_emu.h"

#define JITS_DISK "sfs_disk.disk"
#define BLOCK_SZ 1024
#define NUM_BLOCKS 1024  
#define NUM_INODES 400 

int NUM_INODE_BLOCKS = ((NUM_INODES * sizeof(inode_t)) + BLOCK_SZ - 1)/BLOCK_SZ;

superblock_t sb;
inode_t table[NUM_INODES];
file_descriptor fdt[NUM_INODES];
filenode_t directory[NUM_INODES];
char bitmap[(NUM_BLOCKS+7)/8];
int file_name_index=0;

void init_superblock() {
    sb.magic = 0xACBD0005;
    sb.block_size = BLOCK_SZ;
    sb.fs_size = NUM_BLOCKS * BLOCK_SZ;
    sb.inode_table_len = NUM_INODE_BLOCKS;
    sb.root_dir_inode = 0;
}

void init_root_inode(inode_t root){
	root.mode = 0x777; //full permissions
	root.link_cnt = 1;
	root.uid = 0;
	root.gid = 0;
	root.size = (NUM_INODES * sizeof(filenode_t)); //root stores the directory
	root.initialized = 1;
	memset(root.data_ptrs,0,sizeof(root.data_ptrs[0]));
	root.indirect_pointer=0;
}

/* set_block, is_block_used, get_unused_block
 * ------
 * Functions to interact with the free block bitmap
*/
void set_block(int block_index, int used){
	if (used != 0 && used != 1){
		printf("Invalid set_bock input!\n");
		exit(-1);
	}
	int byte_number = block_index / 8;
	int bit_number = block_index % 8;
		
	if (used == 0) {
		bitmap[byte_number] &= ~(1 << bit_number);
	}
	else{
		bitmap[byte_number] |= (1 << bit_number);
	}
}

int is_block_used(int block_index){
	int byte_number = block_index / 8;
	int bit_number = block_index % 8;
	
	char bit = bitmap[byte_number] & (1 << bit_number);

	if (bit == 0) return 0; //this block index is unused!
	else return 1;
}
int get_unused_block(){
	//superblock #0 is always used
	for (int i=1; i<NUM_BLOCKS; i++){
		if (is_block_used(i) == 0){
			return i;
		}
	}
	return -1;
}
/* get_many_unused_blocks
 * ------
 * input: array of block numbers and number of blocks to assign
 * finds unused blocks, and puts them in the array of block numbers
 * we will set these blocks to 'used' in the bitmap when we successfully write to them
*/
unsigned int get_many_unused_blocks(unsigned int* unused_blocks, unsigned int num_blocks){
	unsigned int allocated_blocks = 0;
	for (unsigned int i=1; i<NUM_BLOCKS; i++){ //super block is always in block 0
		if (is_block_used(i) == 0){
			unused_blocks[allocated_blocks] = i;	
			allocated_blocks++;
			if (allocated_blocks == num_blocks){
				return allocated_blocks;
			}
		}
	}
	printf("No more free blocks left! Assigned %d, but we needed %d\n", allocated_blocks, num_blocks);
	return allocated_blocks;
}

void mksfs(int fresh) {
	// initialize fd table so we can find empty slots later
	// we assume an fdt entry is empty when its inode is 0
	memset(fdt, 0, sizeof(file_descriptor) * NUM_INODES);
	// Initialize bitmap
	memset(bitmap,0,(NUM_BLOCKS+7)/8);
	// Initialize directory
	memset(directory, 0, sizeof(filenode_t) * NUM_INODES);
	for (int i=0; i<NUM_INODES; i++){
		directory[i].file_name[MAXFILENAME-1] = '\0';
		directory[i].inode_num = 0;
	}
	// Initialize inode table
	memset(table, 0, sizeof(inode_t) * NUM_INODES);

	// Make a new file system
    if (fresh) {	
        printf("making new file system\n");

        // create super block
        init_superblock();
		// Initialize root
		init_root_inode(table[sb.root_dir_inode]); //root inode# is stored in superblock
		// open r/w pointer to root directory. always put it in fdt[0]
		fdt[0].inode = sb.root_dir_inode;
		fdt[0].rwptr = 0;

		//initializes disk to all 0's
        init_fresh_disk(JITS_DISK, BLOCK_SZ, NUM_BLOCKS);

		// SET USED BLOCKS IN BITMAP
		// number of starting contiguous blocks: super block, inode table
		int used_blocks = 1 + sb.inode_table_len;
		for(int i=0; i<used_blocks ; i++){
			set_block(i, 1); //super block and root node
		}
		set_block(NUM_BLOCKS-1, 1); //initial bitmap block
	
		//sfs_fwrite calls flush() at the end, so everything will be written to disk

		char* disk_buffer = calloc(NUM_INODES, sizeof(filenode_t));
		memcpy(disk_buffer, directory, NUM_INODES * sizeof(filenode_t));
		table[sb.root_dir_inode].data_ptrs[0]= 1 + sb.inode_table_len;
		set_block(1+sb.inode_table_len, 1);
		sfs_fseek(sb.root_dir_inode,0);
		sfs_fwrite(sb.root_dir_inode, disk_buffer, NUM_INODES * sizeof(filenode_t));
		free(disk_buffer);

	// Load an old file system
    } else {
        printf("reopening file system\n");

		//super block and bitmap are of block size 1. so we just worry about the inode table size
		char* disk_buffer = (char*) calloc(sb.inode_table_len, BLOCK_SZ);
        // open super block
        read_blocks(0, 1, &sb);
        printf("Block Size is: %lu\n", sb.block_size);
        // open inode table
        read_blocks(1, sb.inode_table_len, disk_buffer);
		memcpy(table, disk_buffer, NUM_INODES * sizeof(inode_t));
        // open free block list
		read_blocks(NUM_BLOCKS-1, 1, disk_buffer);
		memcpy(bitmap, disk_buffer, (NUM_BLOCKS+7)/8);
		free(disk_buffer);

		//open root directory. sfs_read calls flush(), so we do this at the end to avoid overwriting block data
		//directory might be bigger than inode table; adjust buffer
		disk_buffer = (char*) calloc(NUM_INODES, sizeof(filenode_t));
		// open r/w pointer to root directory. always put it in fdt[0]
		fdt[0].inode = sb.root_dir_inode;
		fdt[0].rwptr = 0;
		sfs_fread(0, disk_buffer, table[0].size);
		for (int i=0; i < NUM_INODES; i++){
			directory[i] = *(filenode_t *) (disk_buffer + i*sizeof(filenode_t));	
		}
		free(disk_buffer);
    }
	return;
}

void flush(){
	//super block and bitmap are of block size 1. so we just worry about the inode table size
	char* disk_buffer = (char*) calloc(sb.inode_table_len, BLOCK_SZ);
	
	memcpy(disk_buffer, &sb, sizeof(superblock_t));
	write_blocks(0,1,disk_buffer);

	memcpy(disk_buffer, table, NUM_INODES * sizeof(inode_t));
	write_blocks(1,sb.inode_table_len,disk_buffer);
	
	memcpy(disk_buffer, bitmap, (NUM_BLOCKS+7)/8);
	write_blocks(NUM_BLOCKS-1,1,disk_buffer); 

	free(disk_buffer);
	return;
}


//Loops through the directory until it finds a non-empty slot.
// returns file name
int sfs_getnextfilename(char *fname) {
	
	if (file_name_index == NUM_INODES-1) file_name_index = 0;
	while (strlen(directory[file_name_index].file_name) == 0) {
		file_name_index++;
		if (file_name_index == NUM_INODES) file_name_index = 0;
	}
	strcpy(fname, directory[file_name_index].file_name);
	return file_name_index;
}
int sfs_get_next_filename(char *fname){
	return sfs_getnextfilename(fname);
}

//returns 0 if file is not found, inode.size otherwise
int sfs_getfilesize(const char* path) {
	int inode_num = find_inode_index(path);
	if (inode_num < 0) return 0;
	else return table[inode_num].size;
}
int sfs_GetFileSize(const char *path){
	return sfs_getfilesize(path);
}

/* Find inodes, file_descriptors, filenames
 * ------
 * find_inode searches directory for the filename and returns its inode #
 * find_fd searches the fd table for a corresponding inode#, returns fd index
*/
int find_inode_index(char* filename){
	for (int i=0; i<NUM_INODES; i++){
		if (strcmp(directory[i].file_name, filename) == 0) {
			return directory[i].inode_num;
		}
	}
	return -1;
}
int find_fd_index(char* filename){
	int inode_index = find_inode_index(filename);
	if (inode_index < 0) {
		return -1; 
	}
	for (int i=0; i<NUM_INODES; i++){
		if (fdt[i].inode == inode_index) {
			return i; 
		}
	}
	return -1;
}
int find_directory_index(unsigned int n){
	for (int i=0; i<NUM_INODES; i++){
		if (directory[i].inode_num == n){
			return i;
		}
	}
	return -1;
}


/* new_fd
 * ------
 * finds an empty slot in the file descriptor table
 * returns indice of this empty slot
*/
int new_fd(){
	//root directory is always in fdt[0] so we start loop at 1
	for (int i=1; i<NUM_INODES; i++){
		if (fdt[i].inode == 0) { //entry in table hasn't changed from 0
			return i;
		}
	}
	// no empty slots were found
	return -1;
}

/* new_directory_entry()
 * ------
 * Finds an empty slot in the directory table
 * initializes it to inode_num and filename pair
 * returns directory index of the new slot
*/
int new_directory_entry(char* filename, int inode_num){
	int i=0;
	for (; i<NUM_INODES; i++){
		// we assume that a directory entry is empty if its inode_num is 0
		if (directory[i].inode_num == 0){
			break;
		}
		// Unable to find room in the directory
		if (i == NUM_INODES-1) return -1;
	}
	directory[i].inode_num = inode_num;
	strcpy(directory[i].file_name, filename);
	//write new directory to root
		char* disk_buffer = calloc(NUM_INODES, sizeof(filenode_t));
		memcpy(disk_buffer, directory, NUM_INODES * sizeof(filenode_t));
		sfs_fseek(sb.root_dir_inode,0);
		sfs_fwrite(sb.root_dir_inode, disk_buffer, NUM_INODES * sizeof(filenode_t));
		free(disk_buffer);

	return i;
}

/* new_inode()
 * ------
 * Finds an empty slot in the inode table 
 * returns index of this slot, initializes the inode 
*/
int new_inode(char* file_name){
	int i=1; //inode 0 is the root and already taken
	if (strlen(file_name) > MAXFILENAME){
		printf("File name too long!\n");
		return -1;
	}
	for (; i<NUM_INODES; i++){
		if (table[i].initialized == 0) {
			break;
		}
		// Can't find any room in the table
		if (i == NUM_INODES) return -1;
	}
	table[i].mode = 0x755;
	table[i].link_cnt = 1;
	table[i].initialized = 1;
	table[i].indirect_pointer=0;
	for(int i=0; i<12; i++) table[i].data_ptrs[i]=0;

	if (new_directory_entry(file_name, i) < 0) {
		printf("ERROR: cannot save file %s inode number %d to in-memory directory\n", file_name, i);
	}

	return i;
}

/* sfs_fopen
 * ------
 * input: file name
 * output: index of the file descriptor corresponding to this file in the fd table
*/
int sfs_fopen(char *name) {

	//STEP 1: check for pre-existing file descriptor in the table
	int fd_index = find_fd_index(name);
	if (fd_index != -1) { return fd_index; }

	//STEP 2.1: look for the inode
	int inode_index = find_inode_index(name);
	//STEP 2.2: make the inode
	// if we couldn't find one or if we can't make one return error
	if (inode_index < 0 && (((inode_index = new_inode(name)) < 0))) {
		printf("Cannot open file %s\n", name);
		return -1;
	}

	//STEP 3: find an empty slot in the file descriptor table
	fd_index = new_fd(); 
	if (fd_index < 0){
		printf("Failed to make entry in file descriptor table\n");
		return -1;
	}
	
    fdt[fd_index].inode = inode_index;
    fdt[fd_index].rwptr = table[inode_index].size; 
	
	flush();
	return fd_index;
}

int sfs_fclose(int fileID){

	if (fileID >= NUM_INODES || fdt[fileID].inode == 0){
		return -1;
	}

    fdt[fileID].inode = 0;
    fdt[fileID].rwptr = 0;
	return 0;
}

/* fetch_blocks_from_inode
 * -------
 * start_block is the nth block of this inode that we will start fetching from
*/
unsigned int fetch_blocks_from_inode(inode_t file_inode, unsigned int* blocks, unsigned int blocks_to_fetch, unsigned int start_block, unsigned int* indirect_pointers){

	//number of blocks we're able to fetch
	unsigned int fetched_blocks = 0;
	//size of file in blocks
	unsigned int file_block_size = (file_inode.size + BLOCK_SZ - 1)/ BLOCK_SZ;
	char* disk_buffer = (char*) calloc(BLOCK_SZ, 1);
	//index for the data pointers
	unsigned int index = 0;

	/* Fetch blocks from direct data pointers
		-> start from current r/w pointer block
		-> in three cases we stop:
			1) run out of direct data pointers
			2) we've fetched all the blocks we need
			3) there aren't any more blocks allocated to this file
	*/

	//direct pointers correspond to the first twelve blocks
	if (start_block < 12){	
		index = start_block;
		while ((index < 12) && (fetched_blocks < blocks_to_fetch) && (start_block + fetched_blocks < file_block_size)) {
			blocks[fetched_blocks] = file_inode.data_ptrs[index];
			index++;
			fetched_blocks++;
		}
		if (index == 12) index = 0; //reset index for indirect pointer array
	}
	
	/* Fetch blocks from indirect data pointers
		-> read indirect pointer block into memory
		-> fetch blocks from beginning of array. we stop in two cases:
			1) the nth inode block we're trying to fetch is beyond the file block size
			2) we've fetched all the blocks we need
	*/
	//if we have more than 12 blocks
	if (index >= 0 && file_block_size > 12){
		if (start_block > 12) index = start_block-12; //if we're starting in the indirect pointer block, adjust the index
		//Read in the indirect pointer block into an array of data pointers
		read_blocks(file_inode.indirect_pointer, 1, disk_buffer);
		for (int i = 0;  i < file_block_size - 12; i++){
			indirect_pointers[i] = *(unsigned int *) (disk_buffer + i*sizeof(unsigned int));
		}
		free(disk_buffer);
		//Save the block numbers from our indirect pointers
		//index is the start_block and fetched indirect pointers combined. we adjust by 12 to account for direct pointers
		while ((index < file_block_size - 12) && (fetched_blocks < blocks_to_fetch)){ 
			blocks[fetched_blocks] = indirect_pointers[index];
			fetched_blocks++;
			index++;
		}
	}
	return fetched_blocks;
}

int sfs_fread(int fileID, char *buf, int length){
    file_descriptor fd = fdt[fileID];
    inode_t file_inode = table[fd.inode];

	// we assume that an fd entry is empty if its inode is 0
	// but we make an exception for the root (inode = 0) which sits in fdt[0]
	if (fd.inode == 0 && fileID != 0){
		printf("Can't read a file that has not been opened\n");
		return -1;
	}
	// Ensure that we'll be reading from at least one block
	if (length == 0){
		return 0;
	}
	if (fdt[fileID].rwptr + length > table[fdt[fileID].inode].size){
		printf("Cannot read past file size. Truncating requested length %d down to file size %d.\n", length, table[fdt[fileID].inode].size);
		length = table[fdt[fileID].inode].size;
	}

	//block index (relative to inode) that we start reading from. start_block = 0 corresponds to data_ptrs[0]
	unsigned int start_block = fdt[fileID].rwptr / BLOCK_SZ;
	//block index that we end reading from. we minus 1 in case the block size perfectly divides rwptr + length
	//we don't need an extra block of size 0)
	unsigned int end_block = (fdt[fileID].rwptr + length - 1) / BLOCK_SZ;
	//total # of blocks we'll read. add one to adjust for counting from 0; if end_block - start_block = 0 there is still one block to read
	unsigned int blocks_to_read = 1 + end_block - start_block;

	/* STEP 1: FETCH BLOCKS TO READ */
	//blocks[] are all the block numbers that this file uses, which we'lll read from
	unsigned int blocks[blocks_to_read];
	//index for the blocks[] array
	unsigned int assigned_blocks = 0;

	unsigned int total_indirect_pointers = BLOCK_SZ/sizeof(unsigned int);
	unsigned int* indirect_pointers = (unsigned int*) calloc(total_indirect_pointers, sizeof(unsigned int));

	assigned_blocks = fetch_blocks_from_inode(file_inode, blocks, blocks_to_read, start_block, indirect_pointers);

	/* PART 2: READ THE BLOCKS 
		For the first and last blocks to be read, we might be reading incomplete blocks
	 	-> read the block, modify the start / end points, copy it into buffer
		-> for two cases we will stop reading: we've run out of blocks to read, or we've read all that we need
	*/	
	//number of blocks we've read so far
	unsigned int blocks_read = 0;
	//number of bytes we've read so far
	unsigned int bytes_read = 0;
	//last byte we'll end up reading if we get that far
	unsigned last_byte = fdt[fileID].rwptr + length;
	//blocks from disk will be read into here
	char disk_buffer[BLOCK_SZ];
	//start reading just after the residue data
	unsigned int residue_data = fdt[fileID].rwptr % BLOCK_SZ;
	unsigned int read_pointer = residue_data;
	
	while (start_block + blocks_read <= end_block && blocks_read < assigned_blocks){
		//there's a bug in fwrite where some data pointers don't get assigned, so they stay at 0. skip these empty blocks.
		if (blocks[blocks_read] == 0){
			blocks_read++; end_block++; assigned_blocks++;
			continue;
		}

		//read in current block to buffer
		read_blocks(blocks[blocks_read], 1, disk_buffer);
	
		while ((fdt[fileID].rwptr < last_byte) && (fdt[fileID].rwptr < file_inode.size)) {
			buf[bytes_read] = disk_buffer[read_pointer];
			fdt[fileID].rwptr++;
			read_pointer++;
			bytes_read++;
			if ((fdt[fileID].rwptr % BLOCK_SZ) == 0) break;
		}
	
		blocks_read++;
		read_pointer=0;
	}
	flush();
	return bytes_read;
}


unsigned int allocate_blocks_for_inode(int fileID, unsigned int* free_blocks, unsigned int blocks_to_allocate, unsigned int* indirect_pointers){
	
	unsigned int file_block_size = (table[fdt[fileID].inode].size + BLOCK_SZ - 1)/BLOCK_SZ;
	unsigned int start_block = file_block_size; //start_block is the nth (counting from 0) inode block# that we start allocating		
	unsigned int blocks_allocated = 0;
	char* disk_buffer = (char*) calloc(BLOCK_SZ, 1);
	unsigned int total_indirect_pointers = BLOCK_SZ/sizeof(unsigned int);

	//Get an array of free blocks we can use
	unsigned int num_allocated_blocks = get_many_unused_blocks(free_blocks, blocks_to_allocate); 

// TRACE
/*
printf("\n Found %d unused blocks: ", num_allocated_blocks);
for (int i=0; i<num_allocated_blocks; i++){
	printf("%d, ", free_blocks[i]);
}
printf("\n");
*/

	//Keep track of how many inode data pointers we've added
	int dir_pointers_allocated = 0;
	int ind_pointers_allocated = 0;

	/* STEP 2.1: ALLOCATE DIRECT POINTERS
		We want to stop allocating in two cases:
			-> we've run out of direct data pointers
			-> we've run out of blocks to point to
	*/
	
	while (((file_block_size + dir_pointers_allocated) < 12) && (blocks_allocated < num_allocated_blocks)){
		/* The index of the next free direct data pointer is:
				previous block size of file
				+ blocks we've added so far
		*/
		table[fdt[fileID].inode].data_ptrs[start_block + dir_pointers_allocated] = free_blocks[blocks_allocated];
		set_block(free_blocks[blocks_allocated], 1);
		dir_pointers_allocated++;
		blocks_allocated++;
	}
	file_block_size += dir_pointers_allocated;
	
//TRACE
/*
printf("Allocated %d direct pointers: ", dir_pointers_allocated);
for (int i=0; i<dir_pointers_allocated; i++){
	//inode block#, disk block #
	printf("(%d,%d), ", start_block + i, free_blocks[i]);
}
*/
	

	/* STEP 2.2: ALLOCATE INDIRECT POINTERS 
		-> We've either just stopped assigning direct pointers, or the file block size was already greater than 12.
		-> So we stop the loop if:
			1) file block size + indirect pointers allocated is more than the total number of pointers possible
			2) we've allocated all the pointers & blocks we can (subject to the constraint of how many blocks we were able to get). keep in mind num_allocated_blocks <= blocks_to_allocate
	*/
	//we only want to manipulate indirect pointers if the file needs 12 blocks or more
	if (file_block_size + blocks_to_allocate >= 12 && blocks_allocated < num_allocated_blocks){
//printf("\nAllocated indirect pointer blocks (inode block#, disk block#): ");
		while (((file_block_size + ind_pointers_allocated) < total_indirect_pointers+12) && (blocks_allocated < num_allocated_blocks)){
			/* The index of the next free (~indirect) data pointer is:
					previous block size of file
				+	pointers we've added so far
				-	number of direct pointers already allocated
			*/	
			indirect_pointers[file_block_size + ind_pointers_allocated - 12] = free_blocks[blocks_allocated];	
//printf("(%d, %d), ", file_block_size + ind_pointers_allocated, free_blocks[blocks_allocated]);
			set_block(free_blocks[blocks_allocated], 1);
			ind_pointers_allocated++;
			blocks_allocated++;
		}	
//printf("\n");
		//Find a block for the indirect pointers (if we dont have one already)
		if ((ind_pointers_allocated > 0) && ((table[fdt[fileID].inode].indirect_pointer > 0) || ((table[fdt[fileID].inode].indirect_pointer = get_unused_block()) > 0))){
			set_block(table[fdt[fileID].inode].indirect_pointer, 1);
			file_block_size += ind_pointers_allocated;
	
			for (int i=0; i<file_block_size-12; i++){
				memcpy(disk_buffer + i*sizeof(unsigned int), &indirect_pointers[i], sizeof(unsigned int)); 
			}
			write_blocks(table[fdt[fileID].inode].indirect_pointer, 1, disk_buffer);
		}
		else if (table[fdt[fileID].inode].indirect_pointer == 0){
			printf("Cannot find an empty block for the indirect pointer!\n");
			ind_pointers_allocated = 0;
		}
	}
	free(disk_buffer);

	// Check if we've assigned enough inode pointers
	if ((ind_pointers_allocated + dir_pointers_allocated) != blocks_to_allocate){
		printf("Not enough data pointers in inode, or we're out of blocks. Allocated %d pointers and blocks out of required %d\n", ind_pointers_allocated + dir_pointers_allocated, blocks_to_allocate);
		blocks_allocated = dir_pointers_allocated + ind_pointers_allocated;
	}
	fflush(stdout);
	return blocks_allocated;
}

int sfs_fwrite(int fileID, const char *buf,int length){
	
	// we assume fd entries with inode = 0 to be empty.
	// we allow writing to the root (inode = 0) by always putting it in fdt[0]
	if (fdt[fileID].inode == 0 && fileID != 0){
		printf("Cant write to a file that has not been opened \n");
		return -1;
	}

	inode_t file_inode = table[fdt[fileID].inode];

	//last byte we'll write to if we get that far
	unsigned int last_byte = fdt[fileID].rwptr + length;
	//size of file in # of blocks: file_block_size=1 means data_ptrs[0] should be occupied
	unsigned int file_block_size = (file_inode.size + BLOCK_SZ - 1)/BLOCK_SZ;
	//block index (relative to inode) that we start writing from. start_block = 0 corresponds to data_ptrs[0]
	unsigned int start_block = fdt[fileID].rwptr / BLOCK_SZ;
	//block index that we end writing from. we minus 1 in case the new file size perfectly divides rwptr + length (we don't need an extra empty block)
	unsigned int end_block = (fdt[fileID].rwptr + length - 1) / BLOCK_SZ;
	//total # of blocks we'll read. add one to adjust for counting from 0; if end_block - start_block = 0 there is still one block to read
	unsigned int blocks_to_write = 1 + end_block - start_block;
	// difference between file block size and what will be needed after this write (needs to be able to store the minimum unsigned integer) 
	int block_difference = 1 + end_block - file_block_size;
	//buffer will be read into here and then written block by block to disk
	char* disk_buffer = (char*) calloc(BLOCK_SZ, 1);

	/* STEP 1: FETCH BLOCKS THAT HAVE ALREADY BEEN ALLOCATED */
	
	unsigned int blocks[blocks_to_write];
	unsigned int total_indirect_pointers = BLOCK_SZ/sizeof(unsigned int);
	unsigned int* indirect_pointers = (unsigned int*) calloc(total_indirect_pointers, sizeof(unsigned int));

//printf("\n\n *** FILE %d: rw %d, size %d, start %d, will write %d bytes and %d blocks, block diff %d *** \n", fileID, fdt[fileID].rwptr,file_block_size, start_block, length, blocks_to_write, block_difference);
//printf("File %d trying to fetch %d blocks ... ", fileID, blocks_to_write);
	unsigned int assigned_blocks = fetch_blocks_from_inode(file_inode, blocks, blocks_to_write, start_block, indirect_pointers);
//printf("%d blocks fetched \n", assigned_blocks);

	/* STEP 2: ALLOCATE NEW BLOCKS IF NEEDED */
	unsigned int new_blocks[block_difference];
	unsigned int allocated_blocks = 0;
//if (block_difference > 0) printf("File %d is trying to allocate %d blocks... ", fileID, block_difference);
	if ((block_difference > 0) && ((allocated_blocks = allocate_blocks_for_inode(fileID, new_blocks, block_difference, indirect_pointers)) > 0)){
		for (int i=0; i<allocated_blocks; i++){
			blocks[assigned_blocks + i] = new_blocks[i];	
		}
		assigned_blocks += allocated_blocks;
	}	
//TRACE
/*
printf("ALL WRITING BLOCKS (inode block#, disk block#): ");
for (int i=0; i<assigned_blocks; i++){
	printf("(%d, %d), ", start_block + i, blocks[i]);
}	
printf("\n");
*/

	/* STEP 3: START WRITING 	
	 * 	In the first and last blocks, we might be writing incomplete blocks
	 		-> read the block, modify it, and write it back to disk
	*/
	//number of blocks we've written so far
	unsigned int blocks_written = 0;
	//residue_data is the number of bytes we need to save in the first and last block to prevent losing data during overwrite
	unsigned int residue_data = fdt[fileID].rwptr % BLOCK_SZ;
	//number of bytes we're writing in this iteration
	unsigned int bytes_written = 0;	
		
	/* Loop through all of the assigned blocks that we have
		-> if we run out of blocks (pointer or block allocation issue) then we'll break the loop
	*/

	

	unsigned int write_pointer = residue_data;
	while(bytes_written < length && blocks_written < assigned_blocks){
		//there's a bug that lets data pointers remain unassigned, so they point to the super block sometimes.
		if (blocks[blocks_written] == 0) {
			blocks_written++; blocks_to_write++; assigned_blocks++;
			continue;
		}
		if (blocks_written == 0 || blocks_written == blocks_to_write || blocks_written == assigned_blocks){
			read_blocks(blocks[blocks_written], 1, disk_buffer);	
		}
			
		while (fdt[fileID].rwptr < last_byte && write_pointer < BLOCK_SZ){
			disk_buffer[write_pointer++] = buf[bytes_written++];
			fdt[fileID].rwptr++; 
		}	

		write_pointer = 0;
		write_blocks(blocks[blocks_written++], 1, disk_buffer);
	}

	free(disk_buffer);
	if (fdt[fileID].rwptr > file_inode.size) table[fdt[fileID].inode].size = fdt[fileID].rwptr;
	return bytes_written;
}
int sfs_fseek(int fileID, int loc){

	inode_t file_inode = table[fdt[fileID].inode];	
	if (loc > file_inode.size){
		printf("Cannot seek to byte %d beyond file size %d \n", loc, file_inode.size);
		return -1;
	}
	if (loc < 0){
		printf("Cannot seek to negative byte location\n");
		return -1;
	}
    fdt[fileID].rwptr = loc;
	return 0;
}

int sfs_remove(char *file) {

	int fd_index = find_fd_index(file);
	if (fd_index < 0) {
		printf("File %s was not found in fd table; it's probably already removed\n", file);
		return -1;
	}
	if(sfs_fclose(fd_index) < 0){
		printf("Cannot close file\n");
		return -1;
	}
	file_descriptor fd = fdt[fd_index];
	inode_t file_inode = table[fd.inode];
	
	
	//Block array of this file; round size up to nearest block
	unsigned int file_block_size = (file_inode.size + BLOCK_SZ - 1)/ BLOCK_SZ;
	unsigned int blocks[file_block_size];	
	unsigned int* indirect_pointers = (unsigned int*) calloc(BLOCK_SZ / sizeof(unsigned int), sizeof(unsigned int)); 
	char* disk_buffer = (char*) calloc(BLOCK_SZ, 1);

	unsigned int num_blocks = fetch_blocks_from_inode(file_inode, blocks, file_block_size, 0, indirect_pointers);
	
	//erase all blocks and set them to free
	for (int i=0; i<num_blocks; i++){
printf("removing \n");
		write_blocks(blocks[i], 1, disk_buffer);
		set_block(blocks[i], 0);
	}
	//erase indirect pointer block
	write_blocks(file_inode.indirect_pointer, 1, disk_buffer);
	set_block(file_inode.indirect_pointer, 0);
	//erase from inode table
	memset(&table[fd.inode], 0, sizeof(inode_t));
	//erase from directory
	int n = find_directory_index(fd.inode);
	if (n >= 0)	memset(&directory[n], 0, sizeof(filenode_t));

	free(disk_buffer);
	//write new directory to root inode
		disk_buffer = calloc(NUM_INODES, sizeof(filenode_t));
		memcpy(disk_buffer, directory, NUM_INODES * sizeof(filenode_t));
		sfs_fseek(sb.root_dir_inode,0);
		sfs_fwrite(sb.root_dir_inode, disk_buffer, NUM_INODES * sizeof(filenode_t));
		free(disk_buffer);
	free(disk_buffer);
	return 0;
}

