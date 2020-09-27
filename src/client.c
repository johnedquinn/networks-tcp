/* 
Rob Reutiman, John Quinn, Patrick Bald
rreutima, jquinn13, pbald
*/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#define MAX_LINE 4096

/*
 * @func   upload
 * @desc   uploads file to server
 * --
 * @param  s      Socket number
 * @param  fname  File name
 */
void upload(int s, char* fname) {

	int size = 0;

	// Receive Server Acknowledgement
	uint16_t nack;
	if ((size = recv(s, &nack, sizeof(nack), 0)) < 0) {
		perror("Error Receiving Server Acknowledgement");
		return;
	}

	// Get File Information
	struct stat fstat;
	if (stat(fname, &fstat) < 0) {
		fprintf(stderr, "Unable to gather file information\n");
		return;
	}

	// Get File Size
	uint32_t h_fsize = fstat.st_size;
	uint32_t fsize = htonl(h_fsize);
	int hashfsize = h_fsize;

	// Send File Size
	int sent = 0;
	if((sent = send(s, &fsize, sizeof(fsize), 0)) == -1) {
		perror("Client send error!");
		exit(1);
	}

	// Open File
	FILE *fp = fopen(fname, "r");
	if (!fp) {
		fprintf(stderr, "Unable to open file\n");
	}

	// Read Content and Send
	char buff[BUFSIZ]; int read = 1;
	while (h_fsize > 0 && read != 0) {
  	bzero((char *)&buff, sizeof(buff));
		// Read Content
		read = fread(buff, 1, h_fsize, fp);
		if (read < 0) {
			fprintf(stderr, "Error reading file\n");
			return;
		}
		h_fsize -= read;

		// Send Content
		if((size = send(s, buff, read, 0)) == -1) {
			perror("Client send error!");
			exit(1);
		}
	}

	// Close File
	fclose(fp);

	// Perform MD5 Hash
	char command[BUFSIZ]; char md5hash[BUFSIZ];
	sprintf(command, "md5sum %s", fname);
	fp = popen(command, "r");
	fread(md5hash, 1, sizeof(md5hash), fp);
	pclose(fp);
	char *cmd5 = strtok(md5hash, " \t\n");
	fprintf(stdout, "Calculated: %s\n", cmd5);

	// Receive Throughput
	char ntp[BUFSIZ]; int TP_STR_SIZE = 9;
	if ((size = recv(s, &ntp, TP_STR_SIZE, 0)) < 0) {
		perror("Error Receiving Server Acknowledgement");
		return;
	}
	float tp;
	sscanf(ntp, "%f", &tp);
	
	// Receive MD5 Hash
	char smd5[BUFSIZ];
	if ((size = recv(s, &smd5, sizeof(smd5), 0)) < 0) {
		perror("Error Receiving Server Acknowledgement");
		return;
	}

	// Check MD5 Hash
	double secs = hashfsize / tp / 1000000;
	if (!strcmp(cmd5, smd5)) {
		fprintf(stdout, "%d bytes transferred in %lf seconds: %f Megabytes/sec\n", hashfsize, secs, tp);
		fprintf(stdout, "MD5 Hash: %s (matches)\n", cmd5);
	} else
		fprintf(stderr, "Unable to perform UPLOAD\n");

}

/*
 * @func   download
 * @desc   downloads file to server
 * --
 * @param  s      Socket number
 * @param  fname  File name
 */
void download(int s, char* fname) { // --------------------------------- DOWNLOAD

	int size = 0;

	// Receive File Size
	uint32_t nfsize;
	if ((size = recv(s, &nfsize, sizeof(nfsize), 0)) < 0) {
		fprintf(stderr, "Error Receiving Server Acknowledgement");
		return;
	}
	int fsize = ntohl(nfsize);

	// Check Size
	if (fsize < 0) {
		fprintf(stdout, "File Doesn't Exist\n");
		return;
	}

	// Receive MD5
	char nmd5[BUFSIZ];
	if ((size = recv(s, &nmd5, sizeof(nmd5), 0)) < 0) {
		perror("Error Receiving Server MD5");
		return;
	}

	// Initialize File
	FILE * fp = fopen(fname, "w");
	if (!fp) {
		fprintf(stderr, "Unable to create file\n");
		return;
	}

  // Receive File
	char file_content[BUFSIZ]; int rcv_size;
	struct timeval begin, end;
	gettimeofday(&begin, NULL);
	int bsize = fsize;
	while (bsize > 0) {
		// Receive Content
		int temp = (bsize > sizeof(file_content)) ? sizeof(file_content) : bsize;
		bzero((char *)&file_content, sizeof(file_content));
		if((rcv_size = recv(s, file_content, temp, 0)) == -1){
			fprintf(stderr, "Error recieving file name\n");
			return;
		}
		bsize -= rcv_size;
		fflush(stdout);

		// Write Content to File
		if (fwrite(file_content, 1, rcv_size, fp) != rcv_size) {
			fprintf(stderr, "Error writing to file\n");
			return;
		}
	}
	gettimeofday(&end, NULL);
	float time = ((double) (end.tv_usec - begin.tv_usec) / 1000000 + (double) (end.tv_sec - begin.tv_sec));
	fclose(fp);

  // Compute Throughput
  float tp = (double)fsize / (double)time / 1000000;

	// Perform MD5 Hash
	char command[BUFSIZ]; char md5hash[BUFSIZ];
	sprintf(command, "md5sum %s", fname);
	fp = popen(command, "r");
	fread(md5hash, 1, sizeof(md5hash), fp);
	pclose(fp);
	char *cmd5 = strtok(md5hash, " \t\n");
	
	// Check MD5 Hash
	double secs = fsize / tp / 1000000;
	if (!strcmp(cmd5, nmd5)) {
		fprintf(stdout, "%d bytes transferred in %lf seconds: %f Megabytes/sec\n", fsize, secs, tp);
		fprintf(stdout, "MD5 Hash: %s (matches)\n", cmd5);
	} else
		fprintf(stderr, "Unable to perform DOWNLOAD\n");
	
}

/*
 * @func   makedir
 * @desc   makes directory on server
 * --
 * @param  s      Socket number
 * @param  dname  Directory name
 */
void makedir(int s, char *dname) { // ------------------------------- MAKEDIR

	// Get Result Back
	uint32_t nresult;
	if (recv(s, &nresult, sizeof(nresult), 0) < 0) {
		fprintf(stderr, "Error Receiving Server Status");
		return;
	}
	int result = ntohl(nresult);
	
	// Print MKDIR Result
	if (result == -2) {
		fprintf(stdout, "The directory already exists on server\n");
	} else if (result == -1) {
		fprintf(stdout, "Error in making directory\n");
	} else if (result == 1) {
		fprintf(stdout, "The directory was successfully made\n");
	} else {
		fprintf(stdout, "Unknown response from server\n");
	}

}

void cd(int s){ // -------------------------------------------- CD

  uint32_t status;
  
  if((recv(s, &status, sizeof(status), 0)) < 0){
    perror("Error recieving cd return status\n");
    return;
  }

  int converted_status = ntohl(status);

  if(converted_status == -2){
    printf("The directory does not exist on server\n");
  } else if (converted_status == -1){
    printf("Error in changing directory\n");
  } else if (converted_status > 0){
    printf("Changed current directory\n");
  }

}


/*
 * @func   ls
 * @desc   list items in directory
 * --
 * @param  s      Socket number
 */
void ls(int s){ // ------------------------------------------------ LS 
  // recieve directory size
  uint32_t size;
	if (recv(s, &size, sizeof(size), 0) < 0) {
		printf("Error Receiving directory size\n");
		return;
	}
  uint32_t converted_size = ntohl(size);

  char buf[BUFSIZ];

  recv(s, buf, converted_size, 0);
  printf("%s", buf); fflush(stdout);
 
}

/*
 * @func   head
 * @desc   list first 10 lines of a file
 * --
 * @param  s      Socket number
 */
void head(int s){ 

  uint32_t size;

  if(recv(s, &size, sizeof(size), 0) < 0) 
    perror("Error receiving size from server.");

  size = ntohs(size);
  printf("Got this size: %u\n", size);

  if(size > 0) {
    char data[MAX_LINE] = "";
    uint32_t bytes_read = 0;
    int data_bytes = MAX_LINE;

    while(bytes_read < size) {
      if(size - bytes_read < MAX_LINE) {
        data_bytes = size - bytes_read;
      }

      if(recv(s, data, data_bytes, 0) < 0) {
        perror("Error receiving file data from server.");
      }
      printf("%d\n", data_bytes);


      printf("%s\n", data);
      fflush(stdout);
      bytes_read += data_bytes;
    }
    printf("Bytes read: %u\n", bytes_read);
  } else {
    printf("File does not exist on the server.\n");
  }
}

void rm(int s){

  short int status;
  if(recv(s, &status, sizeof(status), 0) < 0) {
    perror("Error recieving rm status\n");
    exit(1);
  }

  status = ntohs(status);

  if(status > 0) {

    // Prompt user for deletion confirmation
    printf("Are you sure? ");
    char input[MAX_LINE];
    fgets(input, MAX_LINE, stdin);

    // Send user decision
    if(send(s, &input, strlen(input) + 1, 0) == -1) {
      perror("Server Send Error"); 
      exit(1);
    }

    if(!strncmp(input, "Yes", 3)) {
      // Wait for deletion confirmation
      short int deleted;
      if(recv(s, &deleted, sizeof(deleted), 0) < 0) {
        perror("Error recieving deletion confirmation\n");
        exit(1);
      }

      deleted = ntohs(deleted);

      if(deleted != 1) {
        perror("Error deleting file from server");
      }

    } else {
      printf("Delete abandoned by the user.\n");
    }

  } else {
    printf("File does not exist on the server.\n");
  }
}

void removeDir(int s){ // ------------------------------------ RMDIR

  int recv_size = 0;
  uint32_t nstatus;
  if((recv_size = recv(s, &nstatus, sizeof nstatus, 0)) < 0){
    perror("Error recieving confirmation status\n");
  }

  int status = ntohl(nstatus);
  if (status == 1){

    // get confirmation from user
    char usr[BUFSIZ];
    printf("Enter Yes/No: "); fflush(stdout);
    fgets(usr, BUFSIZ, stdin);

    int sent_1 = 0;
    int len = strlen(usr) + 1;
    int converted_len = htonl(len);
    // printf("Len: %d, Converted len: %d\n", len, converted_len);
    if((sent_1 = send(s, &converted_len, sizeof converted_len, 0)) < 0){
      perror("Error sending user length confirmation\n");
      exit(1);
    }

    if((sent_1 = send(s, usr, strlen(usr) + 1, 0)) < 0){
      perror("Error sending user confirmation\n");
      exit(1);
    }

    if(!strncmp(usr, "Yes", 3)){

      // get server deletion confirmation
      int confirm_status;
      int recv_size_2;
      if((recv_size_2 = recv(s, &confirm_status, sizeof confirm_status, 0)) < 0){
        perror("Error recieving deletion status\n");
        exit(1);
      }
			confirm_status = ntohl(confirm_status);

      if(confirm_status == 0){
        printf("Directory deleted\n");
      } else 
      printf("Failed to delete directory\n");

    } else if (!strncmp(usr, "No", 2)){
      printf("Delete abondoned by user\n");
    }
    return;

  } else if (status == -1){
    printf("The directory does not exist on the server\n");
    return;
  } else if (status == -2){
    printf("The directory is not empty\n");
    return;
  }

}

int main(int argc, char * argv[]) { // ----------------------------- main
  /* Variables */
  struct hostent *hp;
  struct sockaddr_in sin;
  char *host;
  char buf[MAX_LINE];
  int s;
  int SERVER_PORT;

  /* Parse command line arguments */
  if(argc == 3) {
    host = argv[1];
    SERVER_PORT = atoi(argv[2]);
  } else {
    fprintf(stderr, "usage: simplex-talk host\n");
    exit(1);
  }

  /* Translate host name into peer's IP address */
  hp = gethostbyname(host);
  if(!hp) {
    fprintf(stderr, "simplex-talk: unknown host: %s\n", host);
    exit(1);
  }

  /* Build address data structure */
  bzero((char *)&sin, sizeof(sin));
  sin.sin_family = AF_INET;
  bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
  sin.sin_port = htons(SERVER_PORT);

  /* Create Socket */
  if((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("simplex-talk: socket"); 
    exit(1);
  }

  /* Connect to server */
  printf("Connecting to %s on port %d\n", host, SERVER_PORT);

  if(connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    perror("simplex-talk: connect");
    close(s); 
    exit(1);

  }
  printf("Connection established\n> ");

  /* Client Shell: Send commands to server */
  while(fgets(buf, sizeof(buf), stdin)) {

		// Grab Command
    char* cmd = strtok(buf, " \n");
    char* name;
    uint16_t len;

    /* Send intial operation */
    // printf("command: %s!\n", cmd);

    if(send(s, cmd, strlen(cmd) + 1, 0) == -1) {
      perror("client send error!"); 
      exit(1);
    }

    /* Send command specific data */
    if(!strcmp(cmd, "DN") || !strcmp(cmd, "UP") || !strcmp(cmd, "HEAD") || !strcmp(cmd, "RM") || !strcmp(cmd, "MKDIR") || !strncmp(cmd, "RMDIR", 5) || !strcmp(cmd, "CD")) {
      
      // get file name and length for appropriate commands
		  name  = strtok(NULL, "\t\n\0 ");
		  len   = strlen(name);

      /* Send length of name */
      u_int16_t l = htons(len + 1);
      if(send(s, &l, sizeof(l), 0) == -1) {
        perror("client send error!");
        exit(1);
      }

      /* Send name */
      if(send(s, name, strlen(name) + 1, 0) == -1) {
        perror("client send error!"); 
        exit(1);
      }
      
    } else if(!strncmp(cmd, "QUIT", 4)) {
      /* Quit */
      close(s);
      return 0;
    }

    /* Command specific client operations */
    /* UP */
    if(!strcmp(cmd, "UP")) {
			upload(s, name);
    }

    /* DN */
    else if(!strcmp(cmd, "DN")) {
			download(s, name);
    }

    /* HEAD */
    else if(!strcmp(cmd, "HEAD")) {
      head(s);
    }

    /* RM */
    else if(!strcmp(cmd, "RM")) {
      rm(s);
    }

    /* MKDIR */
    else if(!strcmp(cmd, "MKDIR")) {
			makedir(s, name);
    }

    /* LS */ 
    else if(!strcmp(cmd, "LS")){
      ls(s);
    }

    /* RMDIR */
    else if(!strncmp(cmd, "RMDIR", 5)) {
      removeDir(s);
    }

    /* CD */
    else if(!strcmp(cmd, "CD")) {
      cd(s);
    }


    printf("> ");
    fflush(stdout);
  	bzero((char *)&buf, sizeof(buf));
  }

  close(s); 
  return 0; 
}
