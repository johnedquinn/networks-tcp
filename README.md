## Project Overview
This project aims to create a FTP by manipulating TCP. 

### Project Members
Patrick Bald  
John (Cheems) Quinn  
Rob Reutiman  

## Project Structure
The source files are held within `src`, and the executables for server and client are held within `pg2server` and `pg2client` respectively.

## To Run
```terminal
foo@comp1 $ make
foo@comp1 $ cd pg2server
foo@comp1 $ ./myftpd $(PORT)

foo@comp2 $ cd pg2client
foo@comp2 $ ./myftp $(COMP1) $(PORT)
```
