#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>

//CS 5348.002– Operating Systems Concepts Fall 2016 
//Project 2
//Authors: Marissa Miller (mam093220), Uri Smashnov (uxs051000)

//The program was tested on csgrads1/129.110.92.21 UTD server 
//Compilation instructions :  gcc fsaccess.c -ofs
//Run time: ./fs file_system_name
//User input:
//   - initfs 5000 400 - initialize file system with 5000 blocks and 400 i-nodes. 65535 is maximum number of blocks. See below for explanation
//   - cpin external_file internal_file - copy content of external_file into internal_file. Internal file size should be less than 14 chr
//   - cpout  internal_file external_file  - copy content of internal_file into external_file
//   - mkdir dir_name - create directory with dir_name. Directory name size should be less than 14 chr
//   - Rm internal_file - remove file and mark its blocks as free
//   - q - exit
//   - exit - exit (same as "q")
//This program is to emulate Unix V6 file system and support files with up to 32 MB
//File system size is limited to 65535 blocks of 512 Bytes each. This is due to maximum number that can be stored in unsigned short which is 2 bytes long
//File name is limited to 14 characters

//Used for bitwize operations to split file size before storing in inode
#define LAST(k,n) ((k) & ((1<<(n))-1))
#define MID(k,m,n) LAST((k)>>(m),((n)-(m)))

typedef struct one_bit
{
    unsigned x:1;
} one_bit;

typedef struct seven_bits
{
    unsigned x:7;
} seven_bits;

typedef union
{
    struct {
    		seven_bits    bit25_31;
        one_bit       bit24;
        unsigned char bit16_23;
        unsigned short bit0_15;
    } bits;
    unsigned int dword;
} File_Size;

typedef struct  {
			unsigned short isize;
			unsigned short fsize;
			unsigned short nfree;
			unsigned short free[100];
			unsigned short ninode;
			unsigned short inode[100];
			char flock;
			char ilock;
			char fmode;
			unsigned short time[2];
			} super_block_struct;

typedef struct  {
			unsigned short nfree;
			unsigned short free[100];
			} free_blocks_struct;
			
typedef struct {
			unsigned int allocated:1;
			unsigned int file_type:2;
			unsigned int large_file:1;
			unsigned int not_in_use:12;
} i_node_flags;
			
typedef struct {
			unsigned short flags;
			char nlinks;
			char uid;
			char gid;
			char size0;
			unsigned short size1;
			unsigned short addr[8];
			unsigned short actime[2];
			unsigned short modtime[2];
} i_node_struct;

typedef struct {
			unsigned short inode_offset;
			char file_name[14];
} file_struct;

int initfs(int num_blocks, int num_inodes,FILE* file_system);
int cpin(const char* from_filename,const char* to_filename,FILE* file_system);
int cpout(const char* from_filename,const char* to_filename,FILE* file_system);
int cpread(const char* from_filename,const char* to_filename,FILE* file_system);
int Rm(const char* filename,FILE* file_system);
int new_mkdir(const char* filename,FILE* file_system);
int init_free_blocks_chain(int next_free_block,int num_blocks,FILE* file_system);
super_block_struct read_super_block(FILE* file_system);
int add_file_to_dir(const char* to_filename,FILE* file_system);
int get_inode_by_file_name(const char* to_filename,FILE* file_system);
i_node_struct read_inode(int to_file_inode_num, FILE* file_system);
i_node_struct init_file_inode(int to_file_inode_num, unsigned int file_size, FILE* file_system);
int write_inode(int inode_num, i_node_struct inode, FILE* file_system);
int write_super_block(super_block_struct block1,FILE* file_system);
void add_free_block(int block_num, FILE* file_system);
int get_free_block(FILE* file_system);
void copy_int_array(unsigned short *from_array, unsigned short *to_array, int buf_len);
void add_block_to_inode(int block_order_num, int block_num, int to_file_inode_num, FILE* file_system);
unsigned short get_block_for_big_file(int file_node_num,int block_number_order,FILE* file_system);
unsigned int get_inode_file_size(int to_file_inode_num, FILE* file_system);
void add_block_to_free_list(int next_block_number, FILE* file_system);
void remove_file_from_directory(int file_node_num, FILE*  file_system);
void add_block_to_inode_small_file(int block_order_num, int block_num, int to_file_inode_num, FILE* file_system);
unsigned short get_block_for_small_file(int file_node_num,int block_number_order, FILE* file_system);

int block_size=512;
int inode_size=32;
int main(int argc, char *argv[])
{
        int pid,i,j,stat,ppid,status,file_size;
		FILE* file_system = NULL;
		const char *filename = argv[1];
        int max = 200;
        char* command = (char*)malloc(max); /* allocate buffer for user command */
        char c;
		int cmd_counter = 0;
		
		if (argc !=2){
			printf("\nWrong number of parameters. Only one parameter is allowed: file_system name \n");
			exit(0);
		  }
		  
		//printf("\nBefore open file.File name: {%s}\n", filename);

		if( access( filename, F_OK ) != -1 ) {
			// file exists
			printf("\nFile system %s exists. Trying to open...\n",filename);
			file_system = fopen(filename, "r+");
			fseek(file_system, 0L, SEEK_END); // Move to the end of the file
			file_size = ftell(file_system);
			if (file_size == 0){
				printf("\nFile system %s doesn't exists. You need to run initfs command\n",filename);
				}
			else{
				printf("\nFile size is %i\n",file_size);
				rewind(file_system);
				}
			} 
		else {
			// file doesn't exist
			file_system = fopen(filename, "w+");
			printf("\nFile system %s doesn't exists. You need to run initfs command\n",filename);
			}
		

		if(NULL == file_system){
        printf("\n fopen() Error!!!\n");
        return 1;
		}
		
        while(1){
				//While loop, wait for user input
				printf("MyFileSystem>>");
				cmd_counter++;

				int i = 0;
				while (1) {
					// Read user input and process it:
					//  - place zero in last byte
					// 
					int c = getchar();
					if (c=='\n'){ /* at end, add terminating zero */
						command[i] = 0;
						break;
					}
					command[i] = c;
					if (i == max - 1) { /* buffer full */
						printf("Command is too long\n");
						exit(1);
					}
					i++;
				}
			char * cmd;
			char * arg;
			// In case user typed command with argument, split to command and argument
			// Example: ls -la
			cmd = strtok (command," ,\n");
			if (cmd != NULL){
				arg = strtok (NULL, "\n");
				}
        	// printf("Entered command is {%s %s}\n", cmd,arg);

        	if (strcmp(command,"exit")==0 || strcmp(command,"q")==0){
				printf("Number of executed commands is..%i\n",cmd_counter);
				fclose(file_system);
        		exit(0);
        	}
			if (strcmp(command,"initfs")==0){
				printf("Init file_system was requested: %s\n",filename);
				char *  p    = strtok (arg, " ");
				long int num_blocks = atoi(p);
				p = strtok (NULL, " ");
				int num_inodes = atoi(p);
				if (num_blocks>65535){
					printf("\nFailed. Too many blocks requested, maximum is 65535. Please try again.....\n");
					}
				else{
					status = initfs(num_blocks,num_inodes,file_system);
					if (status ==0)
						printf("\nFile system successfully initialized\n");
					else
						printf("\nFile system initialization failed\n");
					}
        	}
			
			else if (strcmp(command,"cpin")==0){
				printf("cpin was requested\n");
				char *  p    = strtok (arg, " ");
				const char *from_filename = p;
				p = strtok (NULL, " ");
				const char *to_filename = p;
				if (strlen(to_filename)>14){
					printf("Target file name  %s is too long:%i, maximum length is 14",to_filename,strlen(to_filename));
					status=1;
					}
				else
					status = cpin(from_filename,to_filename,file_system);
				if (status ==0)
					printf("\nFile  successfully copied\n");
				else
					printf("\nFile copy failed\n");
					
      }
        	
			else if (strcmp(command,"cpout")==0){
				//printf("cpout was requested\n");
				char *  p    = strtok (arg, " ");
				const char *from_filename = p;
				p = strtok (NULL, " ");
				const char *to_filename = p;
				status = cpout(from_filename,to_filename,file_system);
				if (status ==0)
					printf("\nFile %s successfully copied\n",from_filename);
				else
					printf("\nFile copy failed\n");		
      }
	  
			else if (strcmp(command,"Rm")==0){
				printf("Rm was requested\n");
				char *  p    = strtok (arg, " ");
				const char *filename = p;
				status = Rm(filename,file_system);
				if (status ==0)
					printf("\nFile  successfully removed\n");
				else
					printf("\nFile removal failed\n");		
      }
      
      else if (strcmp(command,"mkdir")==0){
				printf("mkdir was requested\n");
				char *  p    = strtok (arg, " ");
				const char *filename = p;
				status = new_mkdir(filename,file_system);
				if (status ==0)
					printf("\nDirectory %s successfully created\n",filename);
				else
					printf("\nDirectory creation failed\n");		
      }
			
			else if (strcmp(command,"cpread")==0){
				printf("cpout was requested\n");
				char *  p    = strtok (arg, " ");
				const char *from_filename = p;
				p = strtok (NULL, " ");
				const char *to_filename = p;
				status = cpread(from_filename,to_filename,file_system);
				if (status ==0)
					printf("\nFile  successfully copied\n");
				else
					printf("\nFile copy failed\n");
					
        	}
      else 
      	printf("Not valid command. Available commands: initfs, cpin, cpout, Rm, exit, q, mkdir. Please try again\n");
			
        }
}

int initfs(int num_blocks, int num_inodes, FILE* file_system)
{	
	i_node_struct i_node;
	i_node_flags flags;
	char buff[block_size];
	super_block_struct block1;
	file_struct directory_entry;
	memset(buff,0,block_size);
	printf("\nInitialize file_system with %i number of blocks and %i number of i-nodes,size of i-node %i %i %i\n",num_blocks,num_inodes,sizeof(i_node),sizeof(flags),sizeof(block1));
	//FILE *file_system = NULL;
	int written=0;
	int i;
	rewind(file_system);
	//Initialize file system by writing zeros to all blocks
	for (i=0;i<num_blocks;i++){
		written += fwrite(buff,1,block_size,file_system);
	}
	printf("\nBytes (%i) written, block size %i, buffer:%s", written,block_size,buff);
	//rewind(file_system);
	
	//Initialize Super block struct
	block1.isize=0;
	block1.fsize=0;
	block1.nfree=1;
	block1.free[0] = 0;
	fseek(file_system,block_size,SEEK_SET);
	fwrite(&block1,block_size,1,file_system);
	int number_inode_blocks = num_inodes/16; //Number of i-nodes in one block is 16
	int start_free_block=2+number_inode_blocks+1; //Place root directory block right after i-nodes
	int next_free_block;
	
	// Initialize free blocks
	for (next_free_block=start_free_block;next_free_block<num_blocks; next_free_block++ ){
			//printf("\nCall add_free_block for block: %i",next_free_block);
			add_free_block(next_free_block, file_system);
	}
	
	/*
	fseek(file_system, 0L, SEEK_END); //Move to the end of the file
	int file_size = ftell(file_system);
	printf("\nFile size after free block init is : %i", file_size);
	*/
	
	
	//Initialize free i-node list
	
	//First load superblock from filesystem
	fseek(file_system,block_size,SEEK_SET);
	fread(&block1,sizeof(block1),1,file_system);
	block1.ninode=99;
	int next_free_inode=1; //First i-node is reserved for Root Directory
	for (i=0;i<=99;i++){
		block1.inode[i]=next_free_inode;
		//printf("\nInitialize i-node (%i) (%i)\n",i,next_free_inode);
		next_free_inode++;
	}
	//Go to beginning of the second block
	fseek(file_system,block_size,SEEK_SET);
	fwrite(&block1,block_size,1,file_system);
	
	
	
	// Need to Initialize only first i-node and root directory file will point to it
	i_node_struct first_inode;
	first_inode.flags=0;       //Initialize
	first_inode.flags |=1 <<15; //Set first bit to 1 - i-node is allocated
	first_inode.flags |=1 <<14; // Set 2-3 bits to 10 - i-node is directory
	first_inode.flags |=0 <<13;
	first_inode.nlinks=0;
	first_inode.uid=0;
	first_inode.gid=0;
	first_inode.size0=0;
	first_inode.size1=16*2; //File size is two records each is 16 bytes.
	first_inode.addr[0]=0;
	first_inode.addr[1]=0;
	first_inode.addr[2]=0;
	first_inode.addr[3]=0;
	first_inode.addr[4]=0;
	first_inode.addr[5]=0;
	first_inode.addr[6]=0;
	first_inode.addr[7]=0;
	first_inode.actime[0]=0;
	first_inode.actime[1]=0;
	first_inode.modtime[0]=0;	
	first_inode.modtime[1]=0;	
	
	
	// Create root directory file and initialize with "." and ".." Set offset to 1st i-node 
	directory_entry.inode_offset = 1; 
	strcpy(directory_entry.file_name, ".");
	int dir_file_block = start_free_block-1;      // Allocate block for file_directory
	fseek(file_system,dir_file_block*block_size,SEEK_SET); //move to the beginning of first available block
	fwrite(&directory_entry,16,1,file_system);
	strcpy(directory_entry.file_name, "..");
	fwrite(&directory_entry,16,1,file_system);
	//fwrite("TEST",4,1,file_system);
	
	//Point first inode to file directory
	printf("\nDirectory in the block %i",dir_file_block);
	first_inode.addr[0]=dir_file_block;
	write_inode(1,first_inode,file_system);
	
	return 0;
}

void add_free_block(int block_num, FILE* file_system){
	super_block_struct block1;
	free_blocks_struct copy_to_block;
	fseek(file_system, block_size, SEEK_SET);
	fread(&block1,sizeof(block1),1,file_system);
	if (block1.nfree == 100){ //free array is full - copy content of superblock to new block and point to it
			//printf("\nCopy free list to block %i",block_num);
			copy_to_block.nfree=100;
			copy_int_array(block1.free, copy_to_block.free, 100);
			fseek(file_system,block_num*block_size,SEEK_SET);
			fwrite(&copy_to_block,sizeof(copy_to_block),1,file_system);
			block1.nfree = 1;
			block1.free[0] = block_num;
	}
	else {
			block1.free[block1.nfree] = block_num;
			block1.nfree++;	
			}
	fseek(file_system, block_size, SEEK_SET);
	fwrite(&block1,sizeof(block1),1,file_system);
}
	
void copy_int_array(unsigned short *from_array, unsigned short *to_array, int buf_len){
	int i;
	for (i=0;i<buf_len;i++){
		to_array[i] = from_array[i];
	}
}
	

int cpin(const char* from_filename,const char* to_filename,FILE* file_system){
	printf("\nInside cpin, copy from %s to %s \n",from_filename, to_filename);
	i_node_struct to_file_inode;
	int to_file_inode_num, status_ind, num_bytes_read, free_block_num, file_node_num;
	unsigned long file_size;
	FILE* from_file_fd;
	unsigned char read_buffer[block_size];
	
	file_node_num = get_inode_by_file_name(to_filename,file_system);
	if (file_node_num !=-1){
		printf("\nFile %s already exists. Choose different name",to_filename);
		return -1;
	}
	
	if( access( from_filename, F_OK ) != -1 ) {
			// file exists
			printf("\nCopy From File %s exists. Trying to open...\n",from_filename);
			from_file_fd = fopen(from_filename, "rb");
			fseek(from_file_fd, 0L, SEEK_END); //Move to the end of the file
			file_size = ftell(from_file_fd);
			if (file_size == 0){
				printf("\nCopy from File %s doesn't exists. Type correct file name\n",from_filename);
				return -1;
				}
			else{
				printf("\nCopy from File size is %i\n",file_size);
				rewind(file_system);
				}
			} 
		else {
			// file doesn't exist
				printf("\nCopy from File %s doesn't exists. Type correct file name\n",from_filename);
				return -1;
			}
	
	if (file_size > pow(2,25)){
		printf("\nThe file is too big %s, maximum supported size is 32MB",file_size);
		return 0;
	}
	//Add file name to directory
	to_file_inode_num = add_file_to_dir(to_filename,file_system);
	//Initialize and load inode of new file
	to_file_inode = init_file_inode(to_file_inode_num,file_size, file_system);
	status_ind = write_inode(to_file_inode_num, to_file_inode, file_system);
	
	//Copy content of from_filename into to_file_name
	int num_blocks_read=1;
	int total_num_blocks=0;
	fseek(from_file_fd, 0L, SEEK_SET);	 // Move to beginning of the input file
	int block_order=0;
	FILE* debug_cpin = fopen("cpin_debug.txt","w");
	while(num_blocks_read == 1){
			// Read one block at a time from source file
			num_blocks_read = fread(&read_buffer,block_size,1,from_file_fd);
			total_num_blocks+= num_blocks_read;
			free_block_num = get_free_block(file_system);
			//Debug print into file
			fprintf(debug_cpin,"\nBlock allocated %i, Order num %i",free_block_num, total_num_blocks);
			fflush(debug_cpin);
			if (free_block_num == -1){
				printf("\nNo free blocks left. Total blocks read so far:%i",total_num_blocks);
				return -1;
			}
			//printf("\nBefore Add block %i",block_order);
			//fflush(stdout);
			if (file_size > block_size*8)
				add_block_to_inode(block_order,free_block_num, to_file_inode_num, file_system);	
			else
				add_block_to_inode_small_file(block_order,free_block_num, to_file_inode_num, file_system);	
			//printf("\nAfter Add block %i",block_order);
			//fflush(stdout);
			//printf("\nFree block allocated: %i",free_block_num);
			// Write one block at a time into target file 
			fseek(file_system, free_block_num*block_size, SEEK_SET);
			fwrite(&read_buffer, sizeof(read_buffer), 1, file_system);
			block_order++;
			//printf("\nEnd of loop %i",block_order);
			//fflush(stdout);
			
	}
	fclose(debug_cpin);
	return 0;
}

int Rm(const char* filename,FILE* file_system){
			printf("\nInside Rm, remove file %s",filename);
			int file_node_num, file_size, block_number_order, next_block_number; 
			i_node_struct file_inode;
			unsigned char bit_14; //Plain file or Directory bit
			file_node_num = get_inode_by_file_name(filename,file_system);
			if (file_node_num ==-1){
				printf("\nFile %s not found",filename);
				return -1;
			}
			file_inode = read_inode(file_node_num, file_system);
			bit_14 = MID(file_inode.flags,14,15);
			if (bit_14 == 0){ //Plain file
					printf("\nRemove Plain file");
					file_size = get_inode_file_size(file_node_num, file_system);
					block_number_order = file_size/block_size;
					if (file_size%block_size !=0)
							block_number_order++;  //Take care of truncation of integer devision
					
					block_number_order--; //Order starts from zero
					while(block_number_order>0){
						if (file_size > block_size*8) 
							next_block_number = get_block_for_big_file(file_node_num,block_number_order,file_system);
						else
							next_block_number = file_inode.addr[block_number_order];
							
						//printf("\nAdd block %i to free block list",next_block_number);
						add_block_to_free_list(next_block_number, file_system);
						block_number_order--;
					}
			}
			file_inode.flags=0; //Initialize inode flag and make it unallocated
			write_inode(file_node_num, file_inode, file_system);
			remove_file_from_directory(file_node_num, file_system);
			fflush(file_system);
			return 0;
}
void add_block_to_inode(int block_order_num, int block_num, int to_file_inode_num, FILE* file_system){  //Assume large file
			i_node_struct file_inode;
			int logical_block, prev_logical_block, word_in_block, free_block_num, word_in_sec, second_block_num, word_in_third;
			unsigned short block_num_tow = block_num;
			unsigned short sec_ind_block;
			file_inode = read_inode(to_file_inode_num, file_system);
			logical_block = block_order_num/256;
			prev_logical_block = (block_order_num-1)/256;
			word_in_block = block_order_num % 256;
			//printf("\nInside add block. Logical block :%i and block-order-num:%i, free-block:%i",logical_block, block_order_num, free_block_num );
			//fflush(stdout);
			if (word_in_block == 0 && logical_block <= 7){ //Adding new element to array and need to allocate block
					free_block_num = get_free_block(file_system);
					file_inode.addr[logical_block]=free_block_num;
					//printf("\nAllocate new block for 1..7. Logical block :%i and block-order-num:%i, free-block:%i",logical_block, block_order_num, free_block_num );
					//fflush(stdout);
			}
			
			if (logical_block < 7){ //Write Block location into indirect blok
					fseek(file_system, file_inode.addr[logical_block]*block_size+word_in_block*2, SEEK_SET);
					fwrite(&block_num_tow,sizeof(block_num_tow),1,file_system);
			} 
			
			
			if (logical_block >= 7) { //Write Block location into double indirect block
					// Read block number from second indirect block
					
					if (logical_block > prev_logical_block){ //Allocate secondary indirect block
							sec_ind_block = get_free_block(file_system);
							fseek(file_system, file_inode.addr[7]*block_size+(logical_block-7)*2, SEEK_SET);
							fwrite(&sec_ind_block,sizeof(sec_ind_block),1,file_system);
							fflush(file_system);
					}

					fseek(file_system, file_inode.addr[7]*block_size+(logical_block-7)*2, SEEK_SET);
					fread(&sec_ind_block,sizeof(sec_ind_block),1,file_system);
					
					// Write target block number into second indirect block
					fseek(file_system, sec_ind_block*block_size+word_in_block*2, SEEK_SET);
					fwrite(&block_num_tow,sizeof(block_num_tow),1,file_system);
			}
			
			write_inode(to_file_inode_num, file_inode, file_system);
			
}

void add_block_to_inode_small_file(int block_order_num, int block_num, int to_file_inode_num, FILE* file_system){  
		//Assume small file
		i_node_struct file_inode;
		unsigned short block_num_tow = block_num;
		file_inode = read_inode(to_file_inode_num, file_system);
		file_inode.addr[block_order_num] = block_num_tow;			
		write_inode(to_file_inode_num, file_inode, file_system);
		return;
}

int get_free_block(FILE* file_system){
	//printf("\nInside get free block");
	//fflush(stdout);
	super_block_struct block1;
	free_blocks_struct copy_from_block;
	int free_block;
	
	fseek(file_system, block_size, SEEK_SET);
	fread(&block1,sizeof(block1),1,file_system);
	block1.nfree--;
	free_block = block1.free[block1.nfree];
	if (free_block ==0){ 											// No more free blocks left
			printf("(\nNo free blocks left");
			fseek(file_system, block_size, SEEK_SET);
			fwrite(&block1,sizeof(block1),1,file_system);
			fflush(file_system);
			return -1;
	}
	
	// Check if need to copy free blocks from linked list
	if (block1.nfree == 0) {
			fseek(file_system, block_size*block1.free[block1.nfree], SEEK_SET);
			fread(&copy_from_block,sizeof(copy_from_block),1,file_system);
			block1.nfree = copy_from_block.nfree;
			copy_int_array(copy_from_block.free, block1.free, 100);
			block1.nfree--;
			free_block = block1.free[block1.nfree];
	}
	
	fseek(file_system, block_size, SEEK_SET);
	fwrite(&block1,sizeof(block1),1,file_system);
	fflush(file_system);
	//printf("\nEnd of get free block");
	//fflush(stdout);
	return free_block;
}

void add_block_to_free_list(int freed_block_number, FILE* file_system){
	super_block_struct block1;
	free_blocks_struct copy_to_block;
	
	fseek(file_system, block_size, SEEK_SET);
	fread(&block1,sizeof(block1),1,file_system);
	/******************
	if (freed_block_number < 24885){
		printf("\nFreeing block %i, block1.nfree are :%i",freed_block_number, block1.nfree);
		printf("\nBlock content %i, %i, %i",block1.free[block1.nfree], block1.free[block1.nfree-1],block1.free[block1.nfree-2]);
		}
	**************************/
		
	if (block1.nfree < 100){
			//printf("\nAdd block %i to free list place %i",freed_block_number, block1.nfree);
			block1.free[block1.nfree] = freed_block_number;
			block1.nfree++;
	}
	else{	// nfree == 100
			copy_int_array(block1.free, copy_to_block.free, 100);
			copy_to_block.nfree = 100;
			fseek(file_system, block_size*freed_block_number, SEEK_SET);
			fwrite(&copy_to_block,sizeof(copy_to_block),1,file_system);
			block1.nfree =1;
			block1.free[0] = freed_block_number;
		}
	fseek(file_system, block_size, SEEK_SET);
	fwrite(&block1,sizeof(block1),1,file_system);
	fflush(file_system);
	
	return;
}

int new_mkdir(const char* filename,FILE* file_system){
	i_node_struct directory_inode, free_node;
	file_struct directory_entry;
	int to_file_inode_num, to_file_first_block, flag, status_ind, file_node_num;
	
	file_node_num = get_inode_by_file_name(filename,file_system);
	if (file_node_num !=-1){
		printf("\nDirectory %s already exists. Choose different name",filename);
		return -1;
	}
	
	//printf("\nAfter read super block");
	int found = 0;
	to_file_inode_num=1;
	while(found==0){
		to_file_inode_num++;
		free_node = read_inode(to_file_inode_num,file_system);
		flag = MID(free_node.flags,15,16);  
		if (flag == 0){
			//printf("\nFree inode found:%i",to_file_inode_num );
			found = 1;
		}
	}
	
	directory_inode = read_inode(1,file_system);
	
	// Move to the end of directory file
	printf("\nDirectory node block number is %i",directory_inode.addr[0]);
	fseek(file_system,(block_size*directory_inode.addr[0]+directory_inode.size1),SEEK_SET);
	// Add record to file directory
	directory_entry.inode_offset = to_file_inode_num; 
	strcpy(directory_entry.file_name, filename);
	fwrite(&directory_entry,16,1,file_system);
	
	
	//Update Directory file inode to increment size by one record
	directory_inode.size1+=16;
	write_inode(1,directory_inode,file_system);
	
	//Initialize new directory file-node
	free_node.flags=0;       //Initialize
	free_node.flags |=1 <<15; //Set first bit to 1 - i-node is allocated
	free_node.flags |=1 <<14; // Set 2-3 bits to 10 - i-node is directory
	free_node.flags |=0 <<13;
	
	status_ind = write_inode(to_file_inode_num, free_node, file_system);
	
	return 0;
	
	
}

int add_file_to_dir(const char* to_filename,FILE* file_system){
	// printf("\nInside add_file_to_dir, create %s was requested\n",to_filename);

	i_node_struct directory_inode, free_node;
	file_struct directory_entry;
	int to_file_inode_num, to_file_first_block, flag;

	//printf("\nAfter read super block");
	int found = 0;
	to_file_inode_num=1;
	while(found==0){
		to_file_inode_num++;
		free_node = read_inode(to_file_inode_num,file_system);
		flag = MID(free_node.flags,15,16);  
		if (flag == 0){
			//printf("\nFree inode found:%i",to_file_inode_num );
			found = 1;
		}
	}
	
	directory_inode = read_inode(1,file_system);
	
	// Move to the end of directory file
	fseek(file_system,(block_size*directory_inode.addr[0]+directory_inode.size1),SEEK_SET);
	// Add record to file directory
	directory_entry.inode_offset = to_file_inode_num; 
	strcpy(directory_entry.file_name, to_filename);
	fwrite(&directory_entry,16,1,file_system);
	
	
	//Update Directory file inode to increment size by one record
	directory_inode.size1+=16;
	write_inode(1,directory_inode,file_system);
	
	return to_file_inode_num;
	
}


i_node_struct init_file_inode(int to_file_inode_num, unsigned int file_size, FILE* file_system){
	//printf("\nInside init_file_inode\n");
	i_node_struct to_file_inode;
	unsigned short bit0_15;
	unsigned char bit16_23;
	unsigned short bit24;
	
	bit0_15 = LAST(file_size,16);
	bit16_23 = MID(file_size,16,24);
	bit24 = MID(file_size,24,25);
	
	to_file_inode.flags=0;       //Initialize
	to_file_inode.flags |=1 <<15; //Set first bit to 1 - i-node is allocated
	to_file_inode.flags |=0 <<14; // Set 2-3 bits to 10 - i-node is plain file
	to_file_inode.flags |=0 <<13;
	if (bit24 == 1){                  // Set most significant bit of file size
		to_file_inode.flags |=1 <<0;
	}
	else{
		to_file_inode.flags |=0 <<0;
	}
	if (file_size<=7*block_size){
			to_file_inode.flags |=0 <<12; //Set 4th bit to 0 - small file
	}
	else{
			to_file_inode.flags |=1 <<12; //Set 4th bit to 0 - large file
	}
	to_file_inode.nlinks=0;
	to_file_inode.uid=0;
	to_file_inode.gid=0;
	to_file_inode.size0=bit16_23; // Middle 8 bits of file size
	to_file_inode.size1=bit0_15; //Leeast significant 16 bits of file size
	to_file_inode.addr[0]=0;
	to_file_inode.addr[1]=0;
	to_file_inode.addr[2]=0;
	to_file_inode.addr[3]=0;
	to_file_inode.addr[4]=0;
	to_file_inode.addr[5]=0;
	to_file_inode.addr[6]=0;
	to_file_inode.addr[7]=0;
	to_file_inode.actime[0]=0;
	to_file_inode.actime[1]=0;
	to_file_inode.modtime[0]=0;	
	to_file_inode.modtime[1]=0;
	return to_file_inode;
}


i_node_struct read_inode(int to_file_inode_num, FILE* file_system){
	//printf("\nInside read_inode\n");
	i_node_struct to_file_inode;
	fseek(file_system,(block_size*2+inode_size*(to_file_inode_num-1)),SEEK_SET); //move to the beginning of inode number to_file_inode_num
	fread(&to_file_inode,inode_size,1,file_system);
	return to_file_inode;
}

int write_inode(int inode_num, i_node_struct inode, FILE* file_system){
	//printf("\nInside write_inode\n");
	fseek(file_system,(block_size*2+inode_size*(inode_num-1)),SEEK_SET); //move to the beginning of inode with inode_num
	fwrite(&inode,inode_size,1,file_system);
	return 0;
}


super_block_struct read_super_block(FILE* file_system){
	printf("\nInside read_super_block\n");
	super_block_struct block1;
	fseek(file_system,block_size,SEEK_SET); //move to the beginning of super block
	printf("\nInside read_super_block - before fread, size of %i\n",sizeof(block1));
	fread(&block1,sizeof(block1),1,file_system);
	printf("\nInside read_super_block - before return\n");
	printf("\nSuperblock nfree %i",block1.nfree);
	return block1;
}

int write_super_block(super_block_struct block1,FILE* file_system){
	printf("\nInside write_super_block\n");
	fseek(file_system,block_size,SEEK_SET); //move to the beginning of super block
	fwrite(&block1,sizeof(block1),1,file_system);
	return 0;
}


int cpout(const char* from_filename,const char* to_filename,FILE* file_system){
	//printf("\nInside cpout, copy from %s to %s \n",from_filename, to_filename);
	int file_node_num; 
	file_node_num = get_inode_by_file_name(from_filename,file_system);
	if (file_node_num ==-1){
		printf("\nFile %s not found",from_filename);
		return -1;
	}
	FILE* write_to_file;
	unsigned char buffer[block_size];
	int next_block_number, block_number_order, number_of_blocks,file_size;
	write_to_file = fopen(to_filename,"w");
	block_number_order=0;
	file_size = get_inode_file_size(file_node_num, file_system);
	printf("\nFile size %i",file_size);
	int number_of_bytes_last_block = file_size%block_size;
	unsigned char last_buffer[number_of_bytes_last_block];
	if (number_of_bytes_last_block ==0){
		number_of_blocks = file_size/block_size;
		}
	else
		number_of_blocks = file_size/block_size +1; //The last block is not full
	
	//printf("\nNumber of blocks %i, last block bytes %i",number_of_blocks,number_of_bytes_last_block);
	while(block_number_order < number_of_blocks){
			if (file_size > block_size*8)
				next_block_number = get_block_for_big_file(file_node_num,block_number_order,file_system);
			else
				next_block_number = get_block_for_small_file(file_node_num,block_number_order,file_system);
			
			fseek(file_system, next_block_number*block_size, SEEK_SET);
			if ((block_number_order <(number_of_blocks-1)) || (number_of_bytes_last_block ==0)){
				fread(buffer,sizeof(buffer),1,file_system);
				fwrite(buffer,sizeof(buffer),1,write_to_file);
				}
			else {
				fread(last_buffer,sizeof(last_buffer),1,file_system);
				fwrite(last_buffer,sizeof(last_buffer),1,write_to_file);
				}
			
			block_number_order++;
			
	}
	
	fclose(write_to_file);
	return 0;
}

int cpread(const char* from_filename,const char* to_filename,FILE* file_system){
//Debug function
	printf("\nInside cpread, copy from %s to %s \n",from_filename, to_filename);
	int file_node_num=2; //TODO: support file name to identify inode number
	FILE* debug_file;
	int next_block_number, block_number_order;
	debug_file = fopen(to_filename,"w");
	block_number_order=0;
	while(block_number_order < 8672){
			next_block_number = get_block_for_big_file(file_node_num,block_number_order,file_system);
			block_number_order++;
			fprintf(debug_file,"\nBlock allocated %i, Order num %i",next_block_number, block_number_order);
			
	}
	
	fclose(debug_file);
	return 0;
}

unsigned short get_block_for_big_file(int file_node_num,int block_number_order, FILE* file_system){
	i_node_struct file_inode;
	unsigned short block_num_tow;
	unsigned short sec_ind_block;

	file_inode = read_inode(file_node_num,file_system);
	int logical_block = block_number_order/256;
	int word_in_block = block_number_order % 256;
	//printf("\nIndirect address:%i, num in array %i",file_inode.addr[logical_block], logical_block);
	if (logical_block <7){
					//printf("\nSet offset to: %i",file_inode.addr[logical_block]*block_size+word_in_block*2);
					fseek(file_system, file_inode.addr[logical_block]*block_size+word_in_block*2, SEEK_SET);
					fread(&block_num_tow,sizeof(block_num_tow),1,file_system);
	}
	else{
					// Read block number of second indirect block
					fseek(file_system, file_inode.addr[7]*block_size+(logical_block-7)*2, SEEK_SET);
					fread(&sec_ind_block,sizeof(sec_ind_block),1,file_system);
					
					//printf("\nFirst Block %i, Second block %i",file_inode.addr[7],sec_ind_block);
					// Read target block number from second indirect block
					fseek(file_system, sec_ind_block*block_size+word_in_block*2, SEEK_SET);
					fread(&block_num_tow,sizeof(block_num_tow),1,file_system);
	}
	
	return block_num_tow;
	
}

unsigned short get_block_for_small_file(int file_node_num,int block_number_order, FILE* file_system){
	i_node_struct file_inode;
	file_inode = read_inode(file_node_num,file_system);
	return (file_inode.addr[block_number_order]);
}
int init_free_blocks_chain(int next_free_block,int num_blocks,FILE* file_system){
	// Initialize free list chain
	// Return block number of first link in the chain to store in super-block free[0]
	int n_free_block=next_free_block+1;
	int i;
	free_blocks_struct free_block;
	for (i=99;i>0 ;i--){
		if (n_free_block<num_blocks){
			free_block.free[i]=n_free_block;
		}
		else {
			free_block.free[i]=0; //We run out of free blocks
		}
		n_free_block++;
	}
	free_block.nfree = 99;
	if (n_free_block >= num_blocks){
		//printf("\nBefore last writing free block chain,offset= %i\n",next_free_block);
		free_block.free[0] = 0;
		fseek(file_system,block_size*next_free_block,SEEK_SET);
		fwrite(&free_block,202,1,file_system);
		return 0;
	}
	else{
		//printf("\nBefore witing free block chain,offset= %i\n",next_free_block);
		free_block.free[0] = n_free_block;
		fseek(file_system,block_size*next_free_block,SEEK_SET);
		fwrite(&free_block,202,1,file_system);
		return init_free_blocks_chain(n_free_block,num_blocks,file_system);
	}
	}
	
unsigned int get_inode_file_size(int to_file_inode_num, FILE* file_system){
	i_node_struct to_file_inode;
	unsigned int file_size;
	
	unsigned short bit0_15;
	unsigned char bit16_23;
	unsigned short bit24;
  
	to_file_inode = read_inode(to_file_inode_num, file_system);
	
	bit24 = LAST(to_file_inode.flags,1);
	bit16_23 = to_file_inode.size0;
	bit0_15 = to_file_inode.size1;
		
	file_size = (bit24<<24) | ( bit16_23 << 16) | bit0_15;
	return file_size;
}

int get_inode_by_file_name(const char* filename,FILE* file_system){
	int inode_number;
	i_node_struct directory_inode;
	file_struct directory_entry;
	
	directory_inode = read_inode(1,file_system);
	
	// Move to the end of directory file
	//fseek(file_system,(block_size*3+sizeof(directory_entry)*2),SEEK_SET); //Move to third record in directory
	fseek(file_system,(block_size*directory_inode.addr[0]),SEEK_SET);
	int records=(block_size-2)/sizeof(directory_entry);
	int i;
	for(i=0;i<records; i++){
		fread(&directory_entry,sizeof(directory_entry),1,file_system);
		//printf("\nRead record %s",directory_entry.file_name);
		//printf("\nInput name %s, and length %i",filename,strlen(filename));
		//if (strncmp(filename,directory_entry.file_name,strlen(filename)) == 0)
		if (strcmp(filename,directory_entry.file_name) == 0)
			return directory_entry.inode_offset;
	}
	printf("\nFile %s not found", filename);
	return -1;
}

void remove_file_from_directory(int file_node_num, FILE*  file_system){
	//printf("\nInside remove_file_from_directory");
	i_node_struct directory_inode;
	file_struct directory_entry;
	
	directory_inode = read_inode(1,file_system);
	
	// Move to the beginning of directory file (3rd record)
	fseek(file_system,(block_size*directory_inode.addr[0]+sizeof(directory_entry)*2),SEEK_SET); //Move to third record in directory
	int records=(block_size-2)/sizeof(directory_entry);
	//printf("\nNumber of records in directory %i",records);
	int i;
	for(i=0;i<records; i++){
		fread(&directory_entry,sizeof(directory_entry),1,file_system);
		//printf("\nNode number in directory %i",directory_entry.inode_offset);
		if (directory_entry.inode_offset == file_node_num){
			fseek(file_system, (-1)*sizeof(directory_entry), SEEK_CUR); //Go one record back
			directory_entry.inode_offset = 0;
			memset(directory_entry.file_name,0,sizeof(directory_entry.file_name));
			fwrite(&directory_entry,sizeof(directory_entry),1,file_system);      
			return;
			}
	}
	//printf("\nFile node %i was not found", file_node_num);
	return;
}