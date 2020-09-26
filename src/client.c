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
	if((size = send(s, &fsize, sizeof(fsize), 0)) == -1) {
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
void download(int s, char* fname) {

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
void makedir(int s, char *dname) {

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

  int status;

  if((recv(s, &status, sizeof(status), 0)) < 0){
    perror("Error recieving cd return status\n");
    return;
  }

  if(status == -2){
    printf("The directory does not exist on server\n");
  } else if (status == -1){
    printf("Error in changing directory\n");
  } else if (status > 0){
    printf("Changed current directory\n");
  }

}

void ls(int s){ // ------------------------------------------------ LS 
  // recieve directory size
  uint32_t size;
	if (recv(s, &size, sizeof(size), 0) < 0) {
		printf("Error Receiving directory size\n");
		return;
	}
  uint32_t converted_size = ntohl(size);
  printf("Converted Size: %d\n", converted_size);

  char buf[BUFSIZ];
  int read = converted_size;
  int total_read = 0;
  while(read > 0){
    int recv_size;
    if((recv_size = recv(s, buf, converted_size, 0)) == -1){
      perror("error receiving ls listing\n");
      return;
    }
    printf("Recieved size: %d\n", recv_size);
    read -= recv_size;
    total_read += recv_size;
    printf("New converted size = %d\n", read);
    fprintf(stdout, "%s", buf);
  fflush(stdout);
  }
  printf("Total bytes read: %d\n", total_read);
  // printf("\n");
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
    if(send(s, cmd, strlen(cmd) + 1, 0) == -1) {
      perror("client send error!"); 
      exit(1);
    }

    /* Send command specific data */
    if(!strcmp(cmd, "DN") || !strcmp(cmd, "UP") || !strcmp(cmd, "HEAD") || !strcmp(cmd, "RM") || !strcmp(cmd, "MKDIR") || !strcmp(cmd, "RMDIR") || !strcmp(cmd, "CD")) {
      
      // get file name and length for appropriate commands
		  name  = strtok(NULL, "\t\n\0 ");
		  len   = strlen(name);

      /* Send length of name */
<<<<<<< HEAD
      uint16_t l = htons(len + 1);
			fprintf(stdout, "Sending file name converted length: %d, bytes: %lu\n", l, sizeof(l));
=======
      u_int16_t l = htons(len + 1);
>>>>>>> ab08d56a3122e94cb2f3b23ca6b1a09b8a97fe68
      if(send(s, &l, sizeof(l), 0) == -1) {
        perror("client send error!");
        exit(1);
      }

      /* Send name */
      fprintf(stdout, "Sending file name: %s, bytes: %lu\n", name, strlen(name) + 1);
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
<<<<<<< HEAD
    printf("Command: %s\n", cmd); fflush(stdout);
=======

>>>>>>> ab08d56a3122e94cb2f3b23ca6b1a09b8a97fe68
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

      uint32_t size;
	    if(recv(s, &size, sizeof(size), 0) < 0) perror("Error receiving size from server.");
      size = ntohs(size);

      printf("Recieved Size: %lu\n", (unsigned long) size);

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
          printf("%s", data);
          bytes_read += data_bytes;
          printf("%lu\n", (unsigned long) bytes_read);
        }

      } else {
        printf("File does not exist on the server.");
      }

    }

    /* RM */
    else if(!strcmp(cmd, "RM")) {

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
    else if(!strcmp(cmd, "RMDIR")) {

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
