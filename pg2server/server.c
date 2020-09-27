/* 
Rob Reutiman, John Quinn, Patrick Bald
rreutima, jquinn13, pbald
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE 4096
#define MAX_PENDING 5


/*
 * @func   check_file
 * @desc   returns if file exists
 * --
 * @param  path  path to file
 */
int check_file(char* filename){
  struct stat path_stat;
  stat(filename, &path_stat);

  if(S_ISREG(path_stat.st_mode)) return 1;

  fprintf(stderr, "%s is not a file on the server.\n", filename);
  return 0;
}

/*
 * @func   is_directory
 * @desc   returns if dir or not
 * --
 * @param  path  path to dir
 */
int is_directory(const char *path) {

    struct stat path_stat;
    if(stat(path, &path_stat) < 0)
        return 0;

    int isDir = S_ISDIR(path_stat.st_mode);
    if(isDir)
        return 1;

    return 0;
}

/*
 * @func   get_len_and_filename
 * @desc   Receives length of fname and fname
 * --
 * @param  new_s  Socket Descriptor
 * @param  *len   Length of fname to be modified
 * @param  name   Holds str name
 */
void get_len_and_filename(int new_s, uint16_t *len, char name[]){ // ----------------- len and filename
	int size = 0;

  // Receive Filename Length
  uint16_t file_len;
  if((size = recv(new_s, &file_len, sizeof(file_len), 0)) < 0){
    perror("Error receiving file length");
    exit(1);
  }
  *len = ntohs(file_len);
  // Receive Filename
  if ((size = recv(new_s, name, *len, 0)) < 0){
    perror("Error recieving file name");
    exit(1);
  }
}

/*
 * @func   upload
 * @desc   Performs the client requested UP (upload) operation
 * --
 * @param  new_s  Socket Descriptor
 */
void upload(int new_s) {

	int size = 0;

  // Get Filename Length and Filename
  char fname[BUFSIZ]; uint16_t len;
  get_len_and_filename(new_s, &len, fname);

  // Send Acknowledgment
  short int status = 0;
  if ((size = send(new_s, &status, sizeof(status), 0)) == -1) {
    perror("Server Send Error"); 
    exit(1);
  }

  // Receive Size of File
	uint32_t n_fsize = 0;
	int recvd = 0;
  if((recvd = recv(new_s, &n_fsize, sizeof(uint32_t), 0)) == -1){
    perror("Error receiving size of file");
    exit(1);
  }
	uint32_t fsize = ntohl(n_fsize);
	int tpfsize = fsize;

  // Initialize File
  FILE * fp = fopen(fname, "w");
  if (!fp) {
    perror("Unable to create file");
    exit(1);
  }

  // Receive File
  char file_content[BUFSIZ];
	struct timeval begin, end;
	gettimeofday(&begin, NULL);
  while (fsize > 0) {
    // Receive Content
    int temp = (fsize > sizeof(file_content)) ? sizeof(file_content) : fsize;
  	bzero((char *)&file_content, sizeof(file_content));
    int rcv_size;
    if((rcv_size = recv(new_s, file_content, temp, 0)) == -1){
      perror("Error recieving file name");
      exit(1);
    }
    fsize -= rcv_size;

    // Write Content to File
    if (fwrite(file_content, 1, rcv_size, fp) != rcv_size) {
      perror("Error writing to file");
      exit(1);
    }
  }
	gettimeofday(&end, NULL);
	float time = ((double) (end.tv_usec - begin.tv_usec) / 1000000 + (double) (end.tv_sec - begin.tv_sec));
  fclose(fp);

  // Compute Throughput
  float tp = (double)tpfsize / (double)time / 1000000;

  // Compute MD5 Hash @TODO: Perform error checking for file size
  char command[BUFSIZ]; char md5hash[BUFSIZ];
  sprintf(command, "md5sum %s", fname);
  fp = popen(command, "r");
  fread(md5hash, 1, sizeof(md5hash), fp);
  pclose(fp);
	char *smd5 = strtok(md5hash, " \t\n");

  // Send Throughput
  char ntp[BUFSIZ]; int TP_STR_SIZE = 9;
  sprintf(ntp, "%8f", tp);
  if((size = send(new_s, &ntp, TP_STR_SIZE, 0)) == -1) {
    perror("Server Send Error"); 
    exit(1);
  }
  
  // Send MD5 Hash
  if((size = send(new_s, smd5, strlen(md5hash) + 1, 0)) == -1) {
    perror("Server Send Error"); 
    exit(1);
  }

}

/*
 * @func   download
 * @desc   Performs the client requested DN (download) operation
 * --
 * @param  s  Socket Descriptor
 */
void download(int s) { // ---------------------------------------- DOWNLOAD

	int size = 0;

  // Get Filename Length and Filename
  char fname[BUFSIZ]; uint16_t len;
  bzero((char *)&fname, sizeof(fname));
  get_len_and_filename(s, &len, fname);

	// Check if File Exists
	struct stat fstat; int fsize = -1;
	if (stat(fname, &fstat) < 0) {
		fprintf(stderr, "Unable to gather file information\n");
		uint32_t nstat = htonl(fsize);
		if (send(s, &nstat, sizeof(nstat), 0) < 0) {
			fprintf(stderr, "Error sending file status\n");
			return;
		}
		return;
	}

	// Get File Size
	fsize = fstat.st_size;
	uint32_t nfsize = htonl(fsize);

	// Send File Size
	if (send(s, &nfsize, sizeof(nfsize), 0) < 0) {
		fprintf(stderr, "Error sending file status\n");
		return;
	}
	
	// Calculate MD5
	FILE * fp;
  char command[BUFSIZ]; char md5hash[BUFSIZ];
  sprintf(command, "md5sum %s", fname);
  fp = popen(command, "r");
  fread(md5hash, 1, sizeof(md5hash), fp);
  pclose(fp);
	char *smd5 = strtok(md5hash, " \t\n");

	// Send MD5
  if((size = send(s, smd5, strlen(md5hash) + 1, 0)) == -1) {
    perror("Server Send Error"); 
    exit(1);
  }

  // Open File
  fp = fopen(fname, "r");
  if (!fp) {
    fprintf(stderr, "Unable to open file\n");
  }

  // Read Content and Send
	int bsize = 0;
	bsize = fsize;
  char buff[BUFSIZ]; int read = 1;
  while (bsize > 0 && read > 0) {
		memset(buff, 0, sizeof(buff));
    // Read Content
    read = fread(buff, 1, sizeof(buff), fp);
    if (read == 0) {
      fprintf(stderr, "Error reading file\n");
      return;
    }
    bsize -= read;

    // Send Content
    if((size = send(s, buff, read, 0)) == -1) {
      perror("Client send error!");
      return;
    }
  }

  // Close File
  fclose(fp);

}

/*
 * @func   makedir
 * @desc   Performs the client requested MKDIR (make directory) operation
 * --
 * @param  s  Socket Descriptor
 */
void makedir(int s) { // ----------------------------------- MKDIR

	int status, nstatus;

  // Get Directory Name Length and Dir Name
  char dname[BUFSIZ]; uint16_t len;
  bzero((char *)&dname, sizeof(dname));
  get_len_and_filename(s, &len, dname);

	// Check if Dir Doesn't Exist
	struct stat dstat;
	if (stat(dname, &dstat) < 0) {

		// Create Directory
		int make_status = mkdir(dname, 0777);
		if (!make_status) {
			status = 1;
			nstatus = htonl(status);
    	if (send(s, &nstatus, sizeof(nstatus), 0) < 0) {
      	fprintf(stderr, "Client send error!\n");
    	}
			return;
		}

		// Directory Make Error
		status = -1;
		nstatus = htonl(status);
    if (send(s, &nstatus, sizeof(nstatus), 0) < 0) {
     	fprintf(stderr, "Client send error!\n");
    }
		return;	
	}

	// Dir/File Already Exists
	status = -2;
	nstatus = htonl(status);
	if (send(s, &nstatus, sizeof(nstatus), 0) < 0) {
		fprintf(stderr, "Client send error!\n");
	}

}

/*
 * @func   cd
 * @desc   changes server directory
 * --
 * @param  s  socket
 */
void cd(int new_s){ // ------------------------------------------- CD

  char fname[MAX_LINE]; uint16_t len;
  get_len_and_filename(new_s, &len, fname); 

    // if exists: 
  if(is_directory(fname)){
      // try to change directory 
    int cd_status = chdir(fname);

    if(cd_status == 0){
      int c_status = htonl(1);
      if(send(new_s, &c_status, sizeof(c_status), 0) == -1) {
        perror("Server Send Error"); 
        exit(1);
      }

    } else {
      int c_status = htonl(-1);
      if(send(new_s, &c_status, sizeof(c_status), 0) == -1){
        perror("Server send error");
        exit(1);
      }
    }
  } 
  else { // dir does not exist
  int c_status = htonl(-2);
    if(send(new_s, &c_status, sizeof(c_status), 0) == -1){
      perror("Server send error");
      exit(1);
    } 
  }
}


/*
 * @func   ls
 * @desc   Performs the client requested LS (list contents) operation
 * --
 * @param  s  Socket Descriptor
 */
void ls(int new_s) {

  DIR* dir = opendir(".");
  if(!dir) {
    fprintf(stdout, "Unable to open current directory\n");
    return;
  }

  // @TODO compute size of each file and sum
  FILE* fp1;
  fp1 = popen("ls -l", "r");
  if(!fp1){
    fprintf(stdout, "Unable to run popen on directory\n");
    return;
  }

  int dir_size = 0;
  char tmp[BUFSIZ];
  int nread = 0;
  while((nread = fread(tmp, 1, BUFSIZ, fp1)) > 0){
    dir_size += nread;
  }
  pclose(fp1);

  uint32_t dir_string_size = htonl(dir_size) + 1;

  // send length of dir string
  if(send(new_s, &dir_string_size, sizeof dir_string_size, 0) < 0){
    printf("Error sending back dir string size\n");
  }
	
  if(send(new_s, tmp, dir_size + 1, 0) < 0){
    perror("Error sending directory listing\n");
  }
  memset(tmp, 0, BUFSIZ);

}

/*
 * @func   head
 * @desc   Performs the client requested HEAD operation on the requested file
 * --
 * @param  new_s  Socket Descriptor
 */
void head(int new_s) {

  u_int16_t len = 0;
  char name[MAX_LINE];
  bzero((char *)&name, sizeof(name));
  get_len_and_filename(new_s, &len, name);
        
  if(check_file(name)) {

    FILE* fp = fopen(name, "r");
    char curr;
    char buffer[MAX_LINE] = "";
    uint32_t size = 0;
    int lines = 0;

    while(lines < 10) {
      curr = fgetc(fp);
      if(curr == EOF) break;
      size++;
      if(curr == '\n') lines++;
    }

    if(size == 0) {
      // Send Negative Confirmation
      short int status = -1;
      status = htons(status);
      if(send(new_s, &status, sizeof(status), 0) == -1) {
        perror("Server Send Error"); 
        exit(1);
      }
      return;
    }

    // Send Size
    uint32_t new_size = htons(size);
    if(send(new_s, &new_size, sizeof(new_size), 0) == -1) {
      perror("Server Send Error"); 
      exit(1);
    }

    rewind(fp);
    bzero(&buffer, sizeof(buffer));

    // Send Data
    int bytes_sent = 0;
    int data_bytes = MAX_LINE;
    while (bytes_sent < size) {
      if(size - bytes_sent < MAX_LINE) {
        data_bytes = size - bytes_sent;
      }
      if(fread(&buffer, 1, data_bytes, fp) != 0) {
        if(send(new_s, buffer, data_bytes, 0) == -1) {
          perror("Server Send Error"); 
          exit(1);
        }
        bytes_sent += data_bytes;
        bzero((char *)&buffer, sizeof(buffer));
      }
    }
          
    fflush(stdout);
    fclose(fp);

  } else {
    // Send Negative Confirmation
    short int status = -1;
    status = htons(status);
    printf("Sent this %hd\n", status);
    if(send(new_s, &status, sizeof(status), 0) == -1) {
      perror("Server Send Error"); 
      exit(1);
    }
  }
}

/*
 * @func   rm
 * @desc   Performs the client requested RM operation on the requested file
 * --
 * @param  new_s  Socket Descriptor
 */
void rm(int new_s) {

  u_int16_t len = 0;
  char name[MAX_LINE];
  bzero((char *)&name, sizeof(name));
  get_len_and_filename(new_s, &len, name);

  if(check_file(name)) {

    // Send Positive Confirmation
    short int status = 1;
    status = htons(status);
    if(send(new_s, &status, sizeof(status), 0) == -1) {
      perror("Error sending positive confirmation"); 
      exit(1);
    }

    // Recieve Confirmation from Client
    char confirmation[MAX_LINE] = "";
    if(recv(new_s, &confirmation, sizeof(confirmation), 0) < 0) {
      perror("Error recieving rm confirmation\n");
      exit(1);
    }

    if(!strncmp(confirmation, "Yes", 3)) {
      // Delete File
      if(remove(name) != 0) {
        perror("Error removing file");
        exit(1);
      };

      // Send deletion confirmation
      short int deleted = 1;
      deleted = htons(deleted);
      if(send(new_s, &deleted, sizeof(deleted), 0) == -1) {
        perror("Error sending positive confirmation"); 
        exit(1);
      }
    }

  } else {
    // Send Negative Confirmation
    short int status = -1;
    //htons
    if(send(new_s, &status, sizeof(status), 0) == -1) {
      perror("Server Send Error"); 
      exit(1);
    }
  }
}

/*
 * @func   is_empty
 * @desc   returns if dir is empty
 * --
 * @param  path  path to dir
 */
int is_empty(char* dirName){ // --------------------------- directory is empty

  int n = 0;
  struct dirent *d;
  DIR* dir = opendir(dirName);

  if( !dir )
    return 0;

  while((d = readdir(dir)) != NULL){
    if(++n > 2) break; // dont count . and .. 
  }

  closedir(dir);

	return n <= 2;

}

/*
 * @func   removeDir
 * @desc   removes directory
 * --
 * @param  s  socket
 */
void removeDir(int new_s){ // -------------------------------------- RMDIR

  // Get Filename Length and Filename
  char dirName[BUFSIZ]; uint16_t len;
  get_len_and_filename(new_s, &len, dirName);


  // check if directory to be deleted exists
  if(is_directory(dirName)){
    // check if dir is empty
    if(is_empty(dirName)){
      // send back 1
      int sent_1 = 0; int exists_empty = 1;
      int exists_empty_converted = htonl(exists_empty);
      if((sent_1 = send(new_s, &exists_empty_converted, sizeof exists_empty_converted, 0)) < 0){
        perror("Error sending exists empty response\n");
        exit(1);
      }

      // see if client responds with yes or no
      int recv_size = 0;
      char buf[BUFSIZ]; uint32_t recv_len;
      if((recv_size = recv(new_s, &recv_len, sizeof recv_len, 0)) < 0){
        perror("Error recieving client rmdir response\n");
        exit(1);
      }

      if((recv_size = recv(new_s, buf, recv_len, 0)) < 0){
        perror("Error recieving client rmdir response\n");
        exit(1);
      } 

      if(!strncmp(buf, "Yes", 3)){
        int status;
        status = rmdir(dirName);
        // send deletion acknowledgement
        send(new_s, &status, sizeof status, 0);

      }
 
      return;

    } else {
      // send back -1 
      int sent_2 = 0; int exists_full = -2;
			exists_full = htonl(exists_full);
      if((sent_2 = send(new_s, &exists_full, sizeof exists_full, 0)) < 0){
        perror("Error sending exists full response\n");
        exit(1);
      }
    }
  } else {
    // send back -2
    int sent_3 = 0; int dne = -1;
		dne = htonl(dne);
    if((sent_3 = send(new_s, &dne, sizeof dne, 0)) < 0){
      perror("Error sending does not exist response\n");
      exit(1);
    }
  }
}

/*
 * @func   main
 * @desc   main driver
 */
int main(int argc, char* argv[]) {

  // Grab Port from Command Line
  int port;
  if(argc == 2) {
    port = atoi(argv[1]);
  } else {
    fprintf(stderr, "Usage: ./myftp [PORT]\n");
    exit(1);
  }

  struct sockaddr_in sin, client_addr;
  char buf[MAX_LINE];
  int s, new_s;
  int len;

  // Build Address Data Structure
  bzero((char *)&sin, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(port);

  // Set passive option 
  if((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("myftpd: socket");
    exit(1);
  }

  // Set Socket Option
  int opt = 1;
  if((setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(int))) < 0) {
    perror("myftpd: setsocket"); 
    exit(1);
  }

  // Bind Socket
  if((bind(s, (struct sockaddr *) &sin, sizeof(sin))) < 0) {
    perror("myftpd: bind"); 
    exit(1);
  }

  // Listen
  if((listen(s, MAX_PENDING)) < 0) {
    perror("myftpd: listen"); 
    exit(1);
  } 

  /* wait for connection, then receive and print text */
  socklen_t addr_len = sizeof(client_addr);

  printf("Waiting for connections on port %d\n", port);

  while(1) {

    if((new_s = accept(s, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
      perror("simplex-talk: accept"); 
      exit(1);
    }

    printf("Connection established.\n");

    while(1) {
      if((len = recv(new_s, buf, sizeof(buf), 0)) == -1) {
        perror("Server Received Error!"); 
        exit(1);
      }
      if(len == 0) break;

      /* Command specific functions */
      if (!strncmp(buf, "DN", 2)) {
        download(new_s);

      } else if (!strncmp(buf, "UP", 2)) {
        upload(new_s);

      } else if (!strncmp(buf, "HEAD", 4)) {
        head(new_s);

      } else if (!strncmp(buf, "RMDIR", 5)) {
        removeDir(new_s);

      } else if (!strncmp(buf, "LS", 2)) {
        ls(new_s);

      } else if (!strncmp(buf, "MKDIR", 5)) {
	  		makedir(new_s);

      } else if (!strncmp(buf, "RM", 2)) {
        rm(new_s);

      } else if (!strncmp(buf, "CD", 2)) {
        cd(new_s);

      }
    }

    printf("Client finished, close the connection!\n");
    close(new_s);
  }

  close(s);
  return EXIT_SUCCESS;
}

