#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 9000
#define BUFFER_SIZE 1024
#define DATA_FILE "/var/tmp/aesdsocketdata"

// Global variables 
volatile sig_atomic_t exit_requested = 0;	// Variable set in signal_handler()


void signal_handler(int signo){
	// 1. it handles two signals SIGINT and SIGTERM
	// 2. it sets a global flag 
	if(signo == SIGINT || signo == SIGTERM){
	syslog(LOG_INFO, "Caught signal, exiting");
	exit_requested = 1; // Raise flag to initiate signal handling routine
	

	}	
	

}


int main(int argc, char *argv[]){
	
	
	// Open a log
	openlog("aesdsocket", LOG_PID, LOG_USER);
	
	// Register the signal_handle() function to deal with signal interrupts
	if(signal(SIGINT, signal_handler) == SIG_ERR || signal(SIGTERM, signal_handler) == SIG_ERR){
		syslog(LOG_ERR, "Failed to register signal handlers: %s" , strerror(errno));
		exit(EXIT_FAILURE);	// Exit with failure
	}


	
	// check if two arguments are passed by the user argc == 2 and 
	// and the second argument is daemon "-d" 
	int daemon_mode = 0;	// 
	if (argc == 2 && strcmp(argv[1], "-d") == 0){
		daemon_mode =1;
	 syslog(LOG_INFO, "Starting aesdsocket with daemon mode: %d", daemon_mode);

	}

	// setup socket, bind and listen
	// Create a stream socket that uses IPv4
	int socketfd = socket(AF_INET, SOCK_STREAM, 0);		// was missing a semi colon
	// socket() outputs -1 on failure
	if (socketfd == -1){
		syslog(LOG_ERR, "Socket creation failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
		}
	// Add socket reuse option (after socket creation)
	int opt = 1;
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
	    syslog(LOG_ERR, "setsockopt failed: %s", strerror(errno));
	    close(socketfd);
	    exit(EXIT_FAILURE);
	}
	// Prepare socket structure
	
	struct sockaddr_in server_addr; //create an istance of the address stucture , why do we need that?
	memset(&server_addr,0, sizeof(server_addr));	 // Initialize the structure with zeros.
							// We can't use server_addr = 0; as we would do with other data types
							//  Because structures are composed of differnt data types.
	server_addr.sin_family = AF_INET; 		// Options are AF_INET or AF_INET6 for IPv4 and IPv6
	server_addr.sin_addr.s_addr = INADDR_ANY; 	// Bind to all available interfaces
	server_addr.sin_port = htons(PORT);		// Standardizing the port value for transmission.
	
	
	

	// Why did we cast server_addr to a pointer? Is it because Bind needs a pointer to a structure? 
	if (bind( socketfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1){
		syslog(LOG_ERR, "Binding to the port %d failed: %s", PORT,strerror(errno)); 
		close(socketfd);
		closelog();	// Close after open
		exit(EXIT_FAILURE);	
	}

	// forK() and create a child, parent logic
	if (daemon_mode){
		pid_t pid = fork();
		if (pid < 0){	//fork() returns -1 on failure
			// fork failed
			// Initiate error routine 
			// 1. log error
			// 2. close socket to prevent memory leak
			// 3. closelog
			// 4. exit with failure
			syslog(LOG_ERR, "fork failed: %s",strerror(errno));
			close(socketfd);
			closelog();
			exit(EXIT_FAILURE);
			
		} else if ( pid > 0){ // parent process
			exit(EXIT_SUCCESS);
		}
		
		// Child continues
		if(setsid() < 0){
			// Failed to become session leader
			syslog(LOG_ERR, "setsid() failed: %s", strerror(errno));  
			exit(EXIT_FAILURE);
		}
		
		// Change working directory
		chdir("/");
		
		// Fully detach 
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}
	// Redirect to /dev/null
	int null_fd = open("/dev/null", O_RDWR);
	// Check if file opened
	if (null_fd < 0){
		syslog(LOG_ERR, "open(/dev/null/) failed: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	dup2(null_fd, STDIN_FILENO);
	dup2(null_fd, STDOUT_FILENO);
	dup2(null_fd, STDERR_FILENO);
	close(null_fd);  

	// Explain the backlog and how to choose a reasonable number
	if (listen(socketfd, 5) == -1){
		syslog(LOG_ERR, "Listen failed: %s", strerror(errno));
		close(socketfd);						
		closelog();     // Close after open
		exit(EXIT_FAILURE);
		}
	printf("server: waiting for connections...\n");
	// Create accept for incoming connections
	while(!exit_requested){
		// loop until you get an interrupt signal
		struct sockaddr_storage their_addr;
		socklen_t addr_size; // a data type called socklen_t.
		// size of their address
		addr_size = sizeof(their_addr);
		
		int new_fd = accept(socketfd, (struct sockaddr *) &their_addr, &addr_size);
		if (new_fd == -1){
			syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
			continue;	// not exit, keep the server running
			
			}
		
		char ip4[INET_ADDRSTRLEN];
		
		struct sockaddr_in *client_addr = (struct sockaddr_in *)&their_addr;
		inet_ntop(AF_INET, &(client_addr->sin_addr), ip4, INET_ADDRSTRLEN);
		syslog(LOG_INFO, "Server: got connection from %s\n", ip4);

		// Declare a temporary buffer and traching variables inside the connection loop
		char temp[BUFFER_SIZE];
		char *full_buf = NULL;
		size_t total_len = 0;
		int newline_found = 0;
		
		// Continue receiving data until newline is reached
		while(!newline_found){
			// Receive data
			memset(temp, 0, BUFFER_SIZE);
			ssize_t bytes_received = recv(new_fd, temp, BUFFER_SIZE, 0);
			// if there are no bytes received then send a log error and stop receiving
			if( bytes_received < 0){
				syslog(LOG_ERR, "recv() failed: %s", strerror(errno));
				break;		// error - stop sending
			
			} else if( bytes_received == 0){
				   syslog(LOG_INFO, "Client disconnected unexpectedly");
 				   break;
			}
			
			
			// Reallocate full_buf to hold new data
			char *new_buf = realloc(full_buf, total_len + bytes_received +1);
			if(!new_buf){
					syslog(LOG_ERR, "Memory allocation failed");
					free(full_buf);
					full_buf = NULL;
					break;
					}
				full_buf = new_buf;
			// Copy new data to the end of full_buf
			memcpy(full_buf + total_len, temp, bytes_received);
			total_len += bytes_received;
			full_buf[total_len] = '\0';
			
			// Check if \n is in the chunk of data
			if (memchr(temp, '\n', bytes_received)){
				newline_found = 1;
				}
		}
			
		// Open file for writing 
		int fd  = open(DATA_FILE, O_WRONLY | O_APPEND | O_CREAT, 0644); // Open file for writing 
		// Check if the file's opened
		if (fd == -1) {
			syslog(LOG_ERR, "Failed to open file for writing: %s", strerror(errno));
			free(full_buf);
			close(new_fd);
			continue;
		}
		
		ssize_t bytes_written = write(fd, full_buf, total_len);
		if (bytes_written == -1){
			syslog(LOG_ERR, "Failed to write to file: %s", strerror(errno));
			// Don't continue sending if we fail to write
			free(full_buf);
			close(new_fd);
			continue;
			}
		close(fd);	// Done writing

//---------------------------------------------------------------------------------------------//
//-------------------------------- Sending all data back to client ----------------------------//
//---------------------------------------------------------------------------------------------//

		fd = open(DATA_FILE, O_RDONLY);
		if(fd == -1){
			syslog(LOG_ERR, "Failed to open file for reading: %s", strerror(errno));
			free(full_buf);
			close(new_fd);
			continue;
			}

		// Create a buffer for holding read data
		// keep sending data while the bytes read is greater than 0
		// log an error if the data isn't sent
		char read_buf[BUFFER_SIZE];
		ssize_t bytes_read;
		memset(read_buf, 0, BUFFER_SIZE); 
		while((bytes_read = read(fd, read_buf, BUFFER_SIZE)) > 0){
			ssize_t bytes_sent = send(new_fd, read_buf, bytes_read, 0);
			if ( bytes_sent == -1){
				syslog(LOG_ERR, "send() failed: %s", strerror(errno));
				break;
			}
		}
		
		if (bytes_read == -1){
			syslog(LOG_ERR, "read() failed: %s", strerror(errno));
		}
		close(fd);	// Done reading 
		
		// Clean up 
		free(full_buf);
		close(new_fd);
		syslog(LOG_INFO, "Closed connection from %s", ip4);

	}
	close(socketfd);	// Cleanup
	unlink(DATA_FILE); 
	closelog();		// Final syslog close 
	
	return 0;		
}
