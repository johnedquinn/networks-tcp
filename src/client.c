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

	// Receive Server Acknowledgement
	int ack;
	if (recv(s, &ack, sizeof(ack), 0) < 0) {
		perror("Error Receiving Server Acknowledgement");
		return;
	}
	fprintf(stdout, "Received Acknowledgement\n");

	// Get File Information
	struct stat fstat;
	if (stat(fname, &fstat) < 0) {
		fprintf(stderr, "Unable to gather file information\n");
		return;
	}

	// Get File Size
	uint32_t h_fsize = fstat.st_size;
	fprintf(stdout, "Initial FSize: %d\n", h_fsize);
	uint32_t fsize = htonl(h_fsize);
	fprintf(stdout, "Converted FSize: %d\n", fsize);

	// Send File Size
	int sent = 0;
	if((sent = send(s, &fsize, sizeof(fsize), 0)) == -1) {
		perror("Client send error!");
		exit(1);
	}
	fflush(stdout);
	fprintf(stdout, "Sent Bytes: %d\n", sent);
	fprintf(stdout, "Sent FSize: %d\n", fsize);

	// Open File
	FILE *fp = fopen(fname, "r");
	if (!fp) {
		fprintf(stderr, "Unable to open file\n");
	}

	// Read Content and Send
	char buff[BUFSIZ]; int read = 1;
	while (fsize > 0 && read != 0) {
		// Read Content
		read = fread(buff, 1, fsize, fp);
		if (read < 0) {
			fprintf(stderr, "Error reading file\n");
			return;
		}
		fprintf(stdout, "Buffer File Content: %s\n", buff);
		fprintf(stdout, "Fsize initially: %d; Read: %d\n", fsize, read);
		fsize -= read;

		// Send Content
		if(send(s, buff, strlen(buff), 0) == -1) {
			perror("Client send error!");
			exit(1);
		}
	}

	// Close File
	fclose(fp);

}

void ls(int s){ // ------------------------------------------------ LS 
  // recieve directory size
  printf("Running LS\n");
  uint32_t size;
	if (recv(s, &size, sizeof(size), 0) < 0) {
		printf("Error Receiving directory size\n");
		return;
	}
  printf("Directory size: %d\n", size);

  int recv_size = 0;
  char buf[BUFSIZ];
  while((recv_size = recv(s, buf, sizeof(buf), 0)) > 0){
    printf("%s", buf);
  }
  fflush(stdout);

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
    char* cmd = strtok(buf, " ");
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
		  len   = strlen(name) - 1;
		  fprintf(stdout, "Command: %s; Name: %s; Name Length: %d\n", cmd, name, len);

      /* Send length of name */
      u_int16_t l = htons(len);
			fprintf(stdout, "Sending file name length: %d\n", l);
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
    printf("Command: %s\n", cmd);
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
      // printf("Running LS command\n");
      // fflush(stdout);
      ls(s);
    }

    /* RMDIR */
    else if(!strcmp(cmd, "RMDIR")) {

    }

    /* CD */
    else if(!strcmp(cmd, "CD")) {

    }


    printf("> ");
    fflush(stdout);
  }

  close(s); 
  return 0; 
}
