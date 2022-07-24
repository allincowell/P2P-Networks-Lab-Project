#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <vector>
#include <algorithm>
#include <ctime>
#include <pthread.h>
using namespace std;


#define SIZE 1000
#define TIMEOUT 120
#define PORT 6969

typedef struct {
	char * name;
	char * ip;
	int port;
	int socket_fd;		
	float latest_interaction_time;	
	struct sockaddr_in peeraddr;
} peer;

int total_peers; /* Total number of peers */
peer * peerlist; /* Details of all peers */

char buf[SIZE]; /* message buffer */

void printPeers() {

	printf("\nNumber of peers : %d\n\n", total_peers);
	for(int i = 0; i < total_peers; i ++) 
		printf("Peer #%d, Name : %s, IP : %s\n\n",
				i + 1, peerlist[i].name, peerlist[i].ip);
}

void print_error(const char *msg) {
	printf("ERROR : %s.\n", msg);
	exit(EXIT_FAILURE);
}

/* Checks input format correctness */
bool wrongInput(char buf[]) {

	int i = 0; 

	while(buf[i] != '\0') {
		if(buf[i] == '/')
			return false;

		i ++;
	}

	return true;
}

/* Finds out index of peer in peerlist based on name or IP address */
int get_peer_index(char *key, int search_type) {

	for (int i = 0; i < total_peers && search_type == 0; i ++)
		if(strcmp(peerlist[i].ip, key) == 0) 
			return i;

	for (int i = 0; i < total_peers && search_type == 1; i ++) 
		if(strcmp(peerlist[i].name, key) == 0) 
			return i;
	
	return -1;
}

/* Function to implement peer timeout. This is run on a separate thread */ 
void * timeout(void * param){
	
	while(1){
		/* Clear timed out peers */
		for(int i = 0; i < total_peers; i ++){
			
			if(peerlist[i].socket_fd == -1)
				continue;

			float idle_time = 
				(float)(clock() - peerlist[i].latest_interaction_time) / CLOCKS_PER_SEC;
			
			if(idle_time >= TIMEOUT) {
				printf("\nConnection with %s timed out\n", peerlist[i].name);
				peerlist[i].latest_interaction_time = -1;
				peerlist[i].socket_fd = -1;
			}
		}
	}

	pthread_exit(0);
}

int main() {

	int server_fd; /* parent socket */
	struct sockaddr_in server_add; /* server's addr */
	
	/* Setup server scoket*/
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) 
		print_error("socket() failed");

	/* Ensure reuse of address */
	int option = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, 
				(const void *)&option , sizeof(int));

	/* Create the datastruct for server */
	bzero((char *) &server_add, sizeof(server_add));
	server_add.sin_family = AF_INET;
	server_add.sin_addr.s_addr = htonl(INADDR_ANY);
	server_add.sin_port = htons(PORT);

	/* Bind to a well-known port */
	int sock_stat = 
		bind(server_fd, (struct sockaddr *) &server_add, sizeof(server_add));
	
	if (sock_stat < 0) 
		print_error("bind() failed");

	/* Listen to socket */
	if (listen(server_fd, 5) < 0) /* allowing max of 5 peer requests */ 
		print_error("listen() failed");

	printf("P2P Server Running at port: %d\n", PORT);
	
	
	/* Accept the user_info data structure and store it */
	FILE * peer_details = fopen("peer_details.txt", "r");

	if(peer_details == NULL)
		print_error("peer_details.txt not found");

	fscanf(peer_details, "%d", &total_peers);
	
	peerlist = (peer *) malloc(total_peers * sizeof(peer));
	
	for(int i = 0; i < total_peers; i ++){
		peerlist[i].name = (char *) malloc(30 * sizeof(char));
		peerlist[i].ip = (char *) malloc(30 * sizeof(char));
	}


	for(int i = 0; i < total_peers; i ++) {
		peerlist[i].socket_fd = -1;
		peerlist[i].latest_interaction_time = -1;
	
		fscanf(peer_details, "%s", peerlist[i].name);

		fscanf(peer_details, "%s", peerlist[i].ip);
		
		peerlist[i].port = htons(PORT);
	}

	fclose(peer_details);
	
	printPeers();

	printf("\nYou can now send and receive messages.\n\n");

	/************************************************************************/

	/* Initializing file descriptors list and adding the 
	known file descriptors-stdin and server_fd to them*/
	vector<int> input_file_descriptors;
	
	fd_set readfds; /* fd_set for files to read */
	
	input_file_descriptors.push_back(STDIN_FILENO);
	input_file_descriptors.push_back(server_fd);

	/* Create thread to monitor peer timeout */
	pthread_t tid;
	pthread_create(&tid,NULL,timeout,NULL);

	while(1) {          

		/* Loop until there is something to read */
		do {
			FD_ZERO(&readfds);

			for(int i = 0; i < input_file_descriptors.size(); i ++)
				FD_SET(input_file_descriptors[i], &readfds);

		} while(select(*max_element(input_file_descriptors.begin(), input_file_descriptors.end()) + 1,
						&readfds, NULL, NULL, NULL) == -1);

		/* Handle new connection request */
		if(FD_ISSET(server_fd,&readfds)) {     
			
			int new_peer_fd; /* new peer socket */
			struct sockaddr_in clientaddr; /* client addr */
			socklen_t clientlen = sizeof(clientaddr); /* byte size of client's address */

			new_peer_fd = 
				accept(server_fd, (struct sockaddr *) &clientaddr, &clientlen);
			
			char * hostaddrp = inet_ntoa(clientaddr.sin_addr); /* dotted decimal host addr string */
			
			if (hostaddrp == NULL)
				print_error("inet_ntoa() failed");
			
			int peer_no = get_peer_index(hostaddrp, 0);
			
			if(peer_no == -1)
				printf("Unknown host trying to connect. Not in peer list!\n");

			else {
				peerlist[peer_no].socket_fd = new_peer_fd;
				peerlist[peer_no].latest_interaction_time = (float)(clock());
				
				input_file_descriptors.push_back(new_peer_fd);
			}
		}

		/* Handle input from stdin */
		if(FD_ISSET(STDIN_FILENO,&readfds)) {
			
			bzero(buf, SIZE);
			int n = read(STDIN_FILENO, buf, SIZE);

			if(n < 0)
				print_error("read() failed");
			
			else if(strcmp(buf, "\n") == 0) ;
				/* Do nothing. Ignore line feeds */
			
			else if(wrongInput(buf)) 
				printf("Message format : peer_name/msg\n");
			

			else if(n > 0) {
				char * peer_name = (char *) malloc(30 * sizeof(char));
				char * msg = (char *) malloc(970 * sizeof(char));
				
				peer_name = strtok(buf,"/");
				msg = strtok(NULL,"/");
				
				int peer_no = get_peer_index(peer_name, 1);
				
				if(peer_no == -1)
					printf("Unknown peer name : %s\n", peer_name);
				else {
					if(peerlist[peer_no].latest_interaction_time == -1) {          
						
						int new_peer_fd = socket(AF_INET, SOCK_STREAM, 0);
						
						if (new_peer_fd < 0) 
							print_error("socket() failed");
						
						struct hostent *server = 
							gethostbyname(peerlist[peer_no].ip);
						
						bzero((char *) &peerlist[peer_no].peeraddr, 
								sizeof(peerlist[peer_no].peeraddr));
						
						peerlist[peer_no].peeraddr.sin_family = AF_INET;
						bcopy((char *) server -> h_addr, 
						(char *) &peerlist[peer_no].peeraddr.sin_addr.s_addr, server -> h_length);
						peerlist[peer_no].peeraddr.sin_port = (peerlist[peer_no].port);
						
						/* Create a connection with the peer */
						int sock_stat = 
							connect(new_peer_fd, (struct sockaddr*) &peerlist[peer_no].peeraddr, 
										sizeof(peerlist[peer_no].peeraddr));
						
						if (sock_stat < 0) 
							printf("\tCan't connect to peer : %s\n", peerlist[peer_no].name);
						
						else {

							peerlist[peer_no].socket_fd = new_peer_fd;
							input_file_descriptors.push_back(new_peer_fd);
						}
					}

					if(peerlist[peer_no].socket_fd != -1) {
						
						/* Send meesage on connection estabilishment */
						int n = write(peerlist[peer_no].socket_fd, msg, strlen(msg));
						
						if (n < 0) 
							print_error("write() failed");
						
						bzero(msg, strlen(msg));
						peerlist[peer_no].latest_interaction_time = (float)(clock());
					}

					else 
						printf("\tCan't send message to peer : %s\n", peerlist[peer_no].name);
				}
			}
		}

		/* Messages received from peers */
		for(int i = 0; i < total_peers; i ++)
			if(FD_ISSET(peerlist[i].socket_fd, &readfds)){
				
				bzero(buf,SIZE);
				int n = read(peerlist[i].socket_fd, buf, SIZE);

				if (n < 0) 
					print_error("read() failed");
				
				peerlist[i].latest_interaction_time = (float)(clock());
				
				if(n == 0)
					close(peerlist[i].socket_fd);
				else 
					printf("\t%s : %s", peerlist[i].name, buf);
			}
	}

	pthread_join(tid,NULL);
}



