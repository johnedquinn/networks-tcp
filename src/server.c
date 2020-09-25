/* 
Rob Reutiman, John Quinn, Patrick Bald
rreutima, jquinn13, pbald
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <dirent.h>

#define MAX_LINE 4096
#define MAX_PENDING 5

int check_file(char* filename){
  struct stat path_stat;
  stat(filename, &path_stat);

  if(S_ISREG(path_stat.st_mode)) return 1;

  fprintf(stderr, "%s is not a file on the server.\n", filename);
  return 0;
}

int is_directory(const char *path) {

    struct stat path_stat;
    if(stat(path, &path_stat) < 0)
        return EXIT_FAILURE;

    int isDir = S_ISDIR(path_stat.st_mode);
    if(isDir)
        return EXIT_SUCCESS;

    return EXIT_FAILURE;
}

void get_len_and_filename(int new_s, uint16_t *len, char name[]){
	int size = 0;

  // recieve length of filename
  uint16_t file_len;
  if((size = recv(new_s, &file_len, sizeof(file_len), 0)) < 0){
    perror("Error receiving file length");
    exit(1);
  }
  *len = ntohs(file_len);
	fprintf(stdout, "Old FName Leng: (%d); FName Len: (%d); Bytes: (%d)\n", file_len, *len, size);
  
  // recieve filename
  if ((size = recv(new_s, name, *len, 0)) < 0){
    perror("Error recieving file name");
    exit(1);
  }
  fprintf(stdout, "Filename: (%s); Bytes Received: (%d)\n", name, size);
}

void download(int new_s){

  char name[MAX_LINE];
  unsigned short len;
  get_len_and_filename(new_s, &len, name);
  
  if(check_file(name) == -1){
    int sent;
    short int status = -1;
    if((sent = send(new_s, &status, sizeof(status), 0)) == -1){
      perror("Error sending back -1\n");
      exit(1);
    }
  }
  
}

/*
 * @func   upload
 * @desc   NA
 * @param  new_s
 */
void upload(int new_s) {

	fprintf(stdout, "@@@@@ UPLOAD @@@@@\n");
	int size = 0;

  // Get Filename Length and Filename
  char fname[BUFSIZ]; uint16_t len;
  get_len_and_filename(new_s, &len, fname);

  // Send Acknowledgment
  fprintf(stdout, "Sending Acknowledgment\n");
  short int status = 0;
  if ((size = send(new_s, &status, sizeof(status), 0)) == -1) {
    perror("Server Send Error"); 
    exit(1);
  }
	if (size != 2) fprintf(stdout, "ACK: RECV ONLY (%d) BYTES\n", size);

  // Receive Size of File
	uint32_t n_fsize = 0;
	int recvd = 0;
  if((recvd = recv(new_s, &n_fsize, sizeof(uint32_t), 0)) == -1){
    perror("Error receiving size of file");
    exit(1);
  }
	fprintf(stdout, "Received Bytes: (%d)\n", recvd);
	fprintf(stdout, "Initial File Size: (%d)\n", n_fsize);
	uint32_t fsize = ntohl(n_fsize);
	int tpfsize = fsize;
	fprintf(stdout, "Converted File Size: (%d)\n", fsize);

  // Initialize File
  fprintf(stdout, "Creating file: (%s)\n", fname);
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
  	bzero((char *)&file_content, sizeof(file_content));
    int rcv_size;
    if((rcv_size = recv(new_s, file_content, fsize, 0)) == -1){
      perror("Error recieving file name");
      exit(1);
    }
		fprintf(stdout, "File Content: (%s)\n", file_content);
		fprintf(stdout, "Initial FSize: (%d); Received: (%d)\n", fsize, rcv_size);
    fsize -= rcv_size;

    // Write Content to File
    if (fwrite(file_content, 1, rcv_size, fp) != rcv_size) {
      perror("Error writing to file");
      exit(1);
    }
  }
	gettimeofday(&end, NULL);
	float time = ((double) (end.tv_usec - begin.tv_usec) / 1000000 + (double) (end.tv_sec - begin.tv_sec));
	fprintf(stdout, "Time Taken: %f\n", time);
  fclose(fp);

  // @TODO: Get rid of hard-code throughput
  // Compute Throughput
  float tp = tpfsize / time / 1000000;
	fprintf(stdout, "TP: (%f MBps)\n", tp);

  // Compute MD5 Hash @TODO: Perform error checking for file size
  char command[BUFSIZ]; char md5hash[BUFSIZ];
  sprintf(command, "md5sum %s", fname);
  fp = popen(command, "r");
  fread(md5hash, 1, sizeof(md5hash), fp);
  pclose(fp);

  // Send Throughput
  uint32_t ntp = htonl((uint32_t)tp);
  if(send(new_s, &ntp, sizeof(ntp), 0) == -1) {
    perror("Server Send Error"); 
    exit(1);
  }
	fprintf(stdout, "TP: Net TP: (%d); Bytes Sent: (%d)\n", ntp, size);
  
  // Send MD5 Hash
  if((size = send(new_s, md5hash, strlen(md5hash) + 1, 0)) == -1) {
    perror("Server Send Error"); 
    exit(1);
  }
  fprintf(stdout, "MD5: Sending Hash: (%s); Bytes Sent(%d)\n", md5hash, size);
  

}

void ls(int new_s){

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
    fflush(stdout);
    return;
  }

  uint32_t dir_size = 0;
  char tmp[BUFSIZ];
  int nread = 0;
  while((nread = fread(tmp, 1, sizeof tmp, fp1)) > 0){
    dir_size += nread;
  }

  pclose(fp1);

  // printf("directory file length: %d\n", dir_size);
  uint32_t dir_string_size = htonl(dir_size);
  // send lenght of dir string
  if(send(new_s, &dir_string_size, sizeof dir_string_size, 0) < 0){
    printf("Error sending back dir string size\n");
  }

  FILE* fp2 = popen("ls -l", "r");

  // @TODO loop through and send directory listing
  char buf[BUFSIZ];
  nread = 0;
  // printf("Sending directory listing...\n");
  while((nread = fread(buf, 1, dir_size, fp2)) > 0){
    printf("%s", buf);
    send(new_s, buf, strlen(buf), 0);
  }

  pclose(fp2);

}

void rm(int new_s){ // ---------------------------------------- RM

  // Get Filename Length and Filename
  char fname[BUFSIZ]; uint16_t len;
  get_len_and_filename(new_s, &len, fname); 

  // check if file exists or not
  // send back : 1 to confirm, -1 if dne

  // wait for confirmation from client to delete or not

}

// @func  main
// @desc  Main driver for server
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
  while(1) {
    if((new_s = accept(s, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
      perror("simplex-talk: accept"); 
      exit(1);
    }

    while(1) {
      if((len = recv(new_s, buf, sizeof(buf), 0)) == -1) {
        perror("Server Received Error!"); 
        exit(1);
      }
      if(len == 0) break;

      printf("TCP Server Received: %s\n", buf);
      if (!strncmp(buf, "DN", 2)) {
        download(new_s);
      } else if (!strncmp(buf, "UP", 2)) {
        upload(new_s);
      } else if (!strncmp(buf, "HEAD", 4)) {

        u_int16_t len = 0;
        char name[MAX_LINE] = "";
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
            size += sizeof(curr);
            if(curr == '\n') lines++;

          }

          if(size == 0) {
            // Send Negative Confirmation
            fprintf(stdout, "Sending Negative Confirmation\n");
            short int status = -1;
            if(send(new_s, &status, sizeof(status), 0) == -1) {
              perror("Server Send Error"); 
              exit(1);
            }
            break;
          }

          // Send Size
          fprintf(stdout, "Sending size to client: %lu\n", (unsigned long) size);
          size = htons(size);
          if(send(new_s, &size, sizeof(size), 0) == -1) {
            perror("Server Send Error"); 
            exit(1);
          }

          rewind(fp);
          bzero(&buffer, sizeof(buffer));

          // Send Data
          fprintf(stdout, "Sending data to client\n");


          int bytes_sent = 0;
          int data_bytes = MAX_LINE;
          while(bytes_sent < size) {
            if(size - bytes_sent < MAX_LINE) {
              data_bytes = size - bytes_sent;
            }
            if(fgets(buffer, data_bytes, fp) != 0) {
              if(send(new_s, buffer, strlen(buffer), 0) == -1) {
                perror("Server Send Error"); 
                exit(1);
              }
            }
            bytes_sent += data_bytes;
          }
          
          fflush(stdout);

        } else {
          // Send Negative Confirmation
          fprintf(stdout, "Sending Negative Confirmation\n");
          short int status = -1;
          if(send(new_s, &status, sizeof(status), 0) == -1) {
            perror("Server Send Error"); 
            exit(1);
          }
        }
        
      } else if (!strncmp(buf, "RM", 2)) {
        rm(new_s);
      } else if (!strncmp(buf, "LS", 2)) {
        ls(new_s);
      } else if (!strncmp(buf, "MKDIR", 5)) {
      } else if (!strncmp(buf, "RMDIR", 5)) {
      } else if (!strncmp(buf, "CD", 2)) {
        cd(new_s);
      } else if (!strncmp(buf, "QUIT", 4)) {
      } else {
        //@TODO: server_options();
        fprintf(stderr, "Option Doesn't Exist: %s!\n", buf);
      }
    }
      printf("Client finished, close the connection!\n");
      close(new_s);
  }

  close(s);
  return EXIT_SUCCESS;
}

