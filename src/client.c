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
	short ack = ntohs(nack);
	fprintf(stdout, "ACK: Received ACK; Val: (%d); Bytes Size: (%d)\n", ack, size);

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
	fflush(stdout);
	fprintf(stdout, "FSIZE: Sent Bytes: (%d); Sent FSize: (%d)\n", sent, fsize);

	// Open File
	FILE *fp = fopen(fname, "r");
	if (!fp) {
		fprintf(stderr, "Unable to open file\n");
	}

	// Read Content and Send
	char buff[BUFSIZ]; int read = 1;
	fprintf(stdout, "SEND:\n");
	while (h_fsize > 0 && read != 0) {
  	bzero((char *)&buff, sizeof(buff));
		// Read Content
		read = fread(buff, 1, h_fsize, fp);
		if (read < 0) {
			fprintf(stderr, "Error reading file\n");
			return;
		}
		fprintf(stdout, "  Buffer File Content: (%s)\n", buff);
		fprintf(stdout, "  Fsize initially: (%d); Read: (%d)\n", h_fsize, read);
		h_fsize -= read;

		// Send Content
		if((size = send(s, buff, read, 0)) == -1) {
			perror("Client send error!");
			exit(1);
		}
		fprintf(stdout, "  Bytes Sent: (%d)\n", size);
	}

	// Close File
	fclose(fp);

	// Receive Throughput
	uint32_t ntp;
	if ((size = recv(s, &ntp, sizeof(ntp), 0)) < 0) {
		perror("Error Receiving Server Acknowledgement");
		return;
	}
	float tp = (float) ntohl(ntp);
	fprintf(stdout, "TP: Net TP: (%d); TP: (%f); Bytes Rcvd: (%d)\n", ntp, tp, size);
	
	// Receive MD5 Hash
	char md5[BUFSIZ];
	if ((size = recv(s, &md5, sizeof(md5), 0)) < 0) {
		perror("Error Receiving Server Acknowledgement");
		return;
	}
	fprintf(stdout, "MD5: RCV MD5: (%s); Bytes: (%d)\n", md5, size);

	// Perform MD5 Hash
	char command[BUFSIZ]; char md5hash[BUFSIZ];
	sprintf(command, "md5sum %s", fname);
	fp = popen(command, "r");
	fread(md5hash, 1, sizeof(md5hash), fp);
	pclose(fp);

	// Check MD5 Hash
	if (!strcmp(md5, md5hash)) {
		fprintf(stdout, "%d bytes transferred in X seconds: %f Megabytes/sec\n", hashfsize, tp);
		fprintf(stdout, "MD5 Hash: %s (matches)\n", md5hash);
	} else
		fprintf(stderr, "Unable to perform UPLOAD\n");


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
		  fprintf(stdout, "Command: %s; Name: %s; Name Length: %d\n", cmd, name, len);

      /* Send length of name */
      uint16_t l = htons(len + 1);
			fprintf(stdout, "Sending file name converted length: %d, bytes: %lu\n", l, sizeof(l));
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
    printf("Command: %s\n", cmd); fflush(stdout);
    /* UP */
    if(!strcmp(cmd, "UP")) {
			upload(s, name);
    }

    /* DN */
    else if(!strcmp(cmd, "DN")) {

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
