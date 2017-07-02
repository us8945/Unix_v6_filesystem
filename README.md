# Unix V6 file-system implementation
### Requirements:
Unix V6 file system has limitation of 16MB on file size. Use single unused bit of the flags field to increase file size support to 32MB. Develop command line interface that will allow Unix user access to the file system of foreign operating system, which is modified version of Unix Version 6. 
In other words, create and maintain content of file system up to the size of 32MB and provide user interface to manipulate and access the content.

### Compilation instructions:  
gcc fsaccess.c -ofs

### Run time: 
./fs file_system_name

### User input/commands:
- initfs 5000 400 - initialize file system with 5000 blocks and 400 i-nodes. 65535 is maximum number of blocks. See below for explanation
- cpin external_file internal_file - copy content of external_file into internal_file. Internal file name size should be less than 14 chr
- cpout  internal_file external_file  - copy content of internal_file into external_file
- mkdir dir_name - create directory with dir_name. Directory name size should be less than 14 chr
- Rm internal_file - remove file and mark its blocks as free
- q - exit
- exit - exit (same as "q")

### Additional notes
- File system size is limited to 65535 blocks of 512 Bytes each. This is due to maximum number that can be stored in unsigned short which is 2 bytes long
- File name is limited to 14 characters

See docs/Example_commands_run.txt - for usage example

### Usage Example:
$ ./fs file_for_submission

File system file_for_submission doesn't exists. You need to run initfs command
MyFileSystem>>initfs 5000 32
Init file_system was requested: file_for_submission

Initialize file_system with 5000 number of blocks and 32 number of i-nodes,size of i-node 32 4 416

Bytes (2560000) written, block size 512, buffer:
Directory in the block 4
File system successfully initialized
MyFileSystem>>cpin copy_from_file.pdf pdf_file
cpin was requested

Inside cpin, copy from copy_from_file.pdf to pdf_file

File pdf_file not found
Copy From File copy_from_file.pdf exists. Trying to open...

Copy from File size is 120672

File  successfully copied
MyFileSystem>>mkdir Uri_dir
mkdir was requested

File Uri_dir not found
Directory node block number is 4
Directory Uri_dir successfully created
MyFileSystem>>mkdir Uri_dir2
mkdir was requested

File Uri_dir2 not found
Directory node block number is 4
Directory Uri_dir2 successfully created
MyFileSystem>>cpin fsaccess_v8.c source_code
cpin was requested

Inside cpin, copy from fsaccess_v8.c to source_code

File source_code not found
Copy From File fsaccess_v8.c exists. Trying to open...

Copy from File size is 37529

File  successfully copied
MyFileSystem>>exit
Number of executed commands is..6
