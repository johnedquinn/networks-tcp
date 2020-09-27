## Project Overview
This project aims to create a FTP by manipulating TCP. 

### Project Members
Patrick Bald (pbald)  
John Quinn (jquinn13)  
Rob Reutiman (rreutima)  

## Project Structure
- `pg2server`: Directory containing server source code, makefile, and example files
- `pg2client`: Directory containing client source code, makefile, and example files
- `pg2server/server.c`: Source code for server
- `pg2client/client.c`: Source code for client

## To Run
```terminal
foo@comp1 $ cd pg2server
foo@comp1 $ make
foo@comp1 $ ./myftpd $(PORT)

foo@comp2 $ cd pg2client
foo@comp2 $ make
foo@comp2 $ ./myftp $(COMP1) $(PORT)
```

## Example Commands
```terminal
foo@comp2 $ LS
foo@comp2 $ DN LargeFile.mp4
foo@comp2 $ RM LargeFile.mp4
foo@comp2 $ LS
foo@comp2 $ UP LargeFile.mp4
foo@comp2 $ LS
foo@comp2 $ MKDIR test-dir
foo@comp2 $ CD test-dir
foo@comp2 $ UP upload.txt
foo@comp2 $ RM upload.txt
foo@comp2 $ CD ..
foo@comp2 $ RMDIR test-dir
foo@comp2 $ HEAD dn.txt
foo@comp2 $ QUIT
```
