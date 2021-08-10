#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#define BUF_SIZE 200
#define STEP_SIZE 20
#define FINISH_CODE "432"
#define ACK_CODE "312"

#define PORTNO 51712

int sockfd, newsockfd;

int thr_error (const char *msg) {
	perror(msg);
	exit(1);
}

void shut_down (int sig_num) {
	printf("Shutting down server\n");
	close(newsockfd);
	close(sockfd);
	exit(1);
}

int send_file (int newsockfd, char *filename) {
	int fd = open(filename, O_RDONLY);
	if (fd < 0) { 
		write(newsockfd, "-1", strlen("-1"));
		return -1;
	}
	printf("Sending file '%s'\n", filename);
	int f_size = lseek(fd, 0, SEEK_END);
	int offset = 0, n_steps = f_size/STEP_SIZE, to_read = STEP_SIZE;
	char buffer[STEP_SIZE+1];
	if (f_size%STEP_SIZE != 0) n_steps++;
	// printf("Filename %s, file size = %d, n_steps = %d\n", filename, f_size, n_steps);

	// Send no. of reads it will take to get the entire file to client
	printf("N_steps = %d\n", n_steps);
	char msg[100];
	sprintf(msg, "%d", n_steps);
	int w_bytes = write(newsockfd, msg, strlen(msg));
	if (w_bytes < 0) { perror("Error writing no. of steps for file to socket"); }
	bzero(buffer, STEP_SIZE);
	int a_bytes = read(newsockfd, buffer, STEP_SIZE);
	if (strcmp(buffer, ACK_CODE) != 0) perror("Did not receive acknowledgement from client");
	fflush(stdout);

	// Read and send file in windows with incremental offset
	int cc = 0;
	for (offset = 0; offset < f_size; offset += STEP_SIZE) {
		if (offset+STEP_SIZE > f_size) {
			to_read = f_size - offset;
		}
		bzero(buffer, STEP_SIZE);
		lseek(fd, offset, SEEK_SET);
		read(fd, buffer, to_read);
		w_bytes = write(newsockfd, buffer, strlen(buffer));
		if (w_bytes < 0) { perror("Error writing file to socket"); }
		// printf("\nData %d:\n%s\n", cc, buffer);

		// Receive acknowledgement
		bzero(buffer, STEP_SIZE);
		a_bytes = read(newsockfd, buffer, STEP_SIZE);
		if (strcmp(buffer, ACK_CODE) != 0) perror("Did not receive acknowledgement from client");
		fflush(stdout);

		cc++;
	}
	w_bytes = write(newsockfd, FINISH_CODE, strlen(buffer));
	// printf("Ran %d times\n", cc);
	printf("Finished sending file '%s'\n", filename);
	return 1;
}

int main (int *argc, char *argv[]) {
	// int sockfd;
	signal(SIGINT, shut_down);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in serv_addr;
	int portno = PORTNO;

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	int bind_ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if (bind_ret < 0) { thr_error("Bind error on server"); }
	listen(sockfd, 10);

	int active = 1;
	while ( active == 1 ) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &client_addr, &client_len);
		if (newsockfd < 0) { thr_error("Error while accepting connection"); }

		printf("Connection accepted from client\n");
		char *buffer = (char *) malloc (sizeof(char) * BUF_SIZE);
		int r_bytes, w_bytes;
		// printf("Waiting for write to file\n");

		int fl = 0;
		while (fl == 0) { 
			// printf("\tFile loop running\n");
			r_bytes = read(newsockfd, buffer, (size_t) BUF_SIZE);
			if (r_bytes < 0) { perror("Erorr reading from file"); fl = 1; }
			else if (r_bytes == 0) { printf("Client disconnected\n"); fflush(stdout); fl = 1; }
			// printf("Received message %s\n", buffer);
			fflush(stdout);
			if (strcmp(buffer, "__exit__") == 0) { fl = 1; }

			send_file(newsockfd, buffer);
			/*
			char msg[100];
			sprintf(msg, "Acknowledged file '%s'\n", buffer);
			w_bytes = write(newsockfd, msg, strlen(msg));
			if (w_bytes < 0) { perror("Error writing filename to socket"); }
			*/

			bzero(buffer, BUF_SIZE);
		}
		close(newsockfd);
	}

	close(sockfd);

	printf("Connection closed\n");
	return 0;
}
