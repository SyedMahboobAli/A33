#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#define BUF_SIZE 300
#define CMD_SIZE 200
#define FN_SIZE 300
#define FINISH_CODE "432"
#define ACK_CODE "312"

#define PORTNO 51712


int sockfd;
int thr_error (const char *msg) {
	perror(msg);
	exit(1);
}

int parse_buffer (char *buffer, char *files[], int *n_files) {
	char *delim = " ", *tokens[100], *tok, *cmd = (char *) malloc (sizeof(char) * CMD_SIZE);
	tok = strtok(buffer, delim);
	strcpy(cmd, tok);
	// printf("Received cmd: %s\n", cmd);
	if (strcmp(cmd, "get") != 0) {
		printf("Invalid command. Please try again\n");
		return -1;
	}

	tok = strtok(NULL, delim);
	int i = 0;
	while (tok != NULL) {
		files[i] = (char *) malloc (sizeof(char) * FN_SIZE);
		strcpy(files[i], tok);
		tok = strtok(NULL, delim);
		// printf("File %d: %s\n", i, files[i]);
		i++;
	}

	*n_files = i;

	return 1;
}

void write_to_file (char *filename, char *buf, int n_bytes) {
	int wfd = open(filename, O_RDWR | O_CREAT | O_APPEND, 0600);
	if (wfd < 0) { thr_error("Unable to open output file"); exit(1); }
	write(wfd, buf, n_bytes);
	close(wfd);
}

int receive_file (int sockfd, char *filename) {
	char *buffer = (char *) malloc (sizeof(char) * BUF_SIZE);
	char *outfile = (char *) malloc (sizeof(char) * BUF_SIZE);

	int start_i = 0;
	for (int i = 0; i < strlen(filename); i++) {
		if (filename[i] == '/') start_i = i+1;
	}
	sprintf(outfile, "%s", &filename[start_i]);

	remove(outfile);

	// Reading total number of reads it will take
	int w_bytes;
	int r_bytes = read(sockfd, buffer, (size_t) BUF_SIZE);
	if (r_bytes < 0) { thr_error("Error getting data from socket"); }
	int n_reads = atoi(buffer);
	if (n_reads == -1) { 
		printf("File '%s' does not exist\n", filename);
		return -1;
	}
	w_bytes = write(sockfd, ACK_CODE, strlen(ACK_CODE));
	if (w_bytes < 0) { thr_error("Error sending acknowledgement to socket"); }

	float progress;
	int bars;

	// Reading for n_reads times sent by server and
	// writing to outfile simultaneously
	for (int i = 0; i < n_reads; i++) {
		r_bytes = read(sockfd, buffer, (size_t) 40);
		if (r_bytes < 0) { thr_error("Error getting data from socket"); }

		// Send acknowledgement for received bytes
		w_bytes = write(sockfd, ACK_CODE, strlen(ACK_CODE));
		if (w_bytes < 0) { thr_error("Error sending acknowledgement to socket"); }

		write_to_file(outfile, buffer, r_bytes);
		fflush(stdout);
		
		progress = (((float)(i+1)) / (float) n_reads) * 100;
		bars = (int) progress/4;
		printf("Progress:  %3.3f%%\t", progress);

		printf("[");
		for (int i = 0; i < 25; i++) {
			if (i < bars) printf("=");
			else printf(" ");
		}
		printf("]");
		printf("\r");
	}
	printf("Progress: 100.000%% \t");
	printf("[");
	for (int i = 0; i < 25; i++) {
		printf("=");
	}
	printf("]\n");

	bzero(buffer, (size_t) BUF_SIZE * sizeof(char));
	r_bytes = read(sockfd, buffer, (size_t) BUF_SIZE);
	// printf("Received finished msg: %s\n", buffer);
	if ((r_bytes < 0) || (strcmp(buffer, FINISH_CODE) != 0)) { 
		printf("Received finished code %s\n", buffer);
		thr_error("Error in getting finished acknowledgement"); 
	}

	printf("Finished writing to file %s\n", outfile);
	return 1;
}

int request_files(char *files[], int n_files) {
	int r_bytes, w_bytes;
	char *buffer = (char *) malloc (sizeof(char) * BUF_SIZE);
	for (int i = 0; i < n_files; i++) { 
		printf("Requesting file '%s'\n", files[i]);
		w_bytes = write(sockfd, files[i], strlen(files[i]));
		if (w_bytes < 0) { thr_error("Error writing filename to socket"); }

		bzero(buffer, BUF_SIZE);
		receive_file(sockfd, files[i]);
	}
	return 1;
}

int main (int argc, char *argv[]) {
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) { thr_error("Unable to create socket from client side"); }

	struct sockaddr_in server_addr;
	struct hostent *server;
	int portno = PORTNO;

	server = gethostbyname("localhost");
	if (server == NULL) { thr_error("localhost not found by client"); }
	bzero((char *) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	bcopy((char *) server->h_addr, (char *) &server_addr.sin_addr.s_addr, server->h_length);
	server_addr.sin_port = htons(portno);

	int connection = connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
	if (connection < 0) { thr_error("Unable to establish connection with server. Please check port number defined in the files"); }

	printf("Successfully established connection\n");

	int fl = 0;
	size_t buf_size = BUF_SIZE;
	char *buffer = (char *) malloc (sizeof(char) * BUF_SIZE);
	char *files[100];
	int n_files = 0;
	while (fl == 0) {
		printf("client> ");
		int chars = getline(&buffer, &buf_size, stdin);
		if (chars != 0) {
			// printf("Received in buffer: %s\n", buffer);
			if (strcmp(buffer, "\n") == 0) continue;
			buffer[chars-1] = '\0';
			if (strcmp(buffer, "exit") == 0) { fl = 1; }
			else {
				if (parse_buffer(buffer, files, &n_files) != -1)
					request_files(files, n_files);
			}
		}
	}

	printf("Exiting\n");

	close(sockfd);

	return 0;
}
