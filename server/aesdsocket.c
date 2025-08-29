/**
 * @file aesdsocket.c
 * @brief Multi-threaded socket server for AESD Assignment 6.
 *
 * This program implements a multi-threaded TCP socket server that listens on port 9000.
 * It supports an optional daemon mode (-d).
 * It receives data from clients, appends it to /var/tmp/aesdsocketdata,
 * and then sends the entire file content back to the client.
 * It handles SIGINT and SIGTERM for graceful shutdown.
 *
 * Author: Failcceed
 * Date: Aug 28
 */

// These two preprocessors give access to advanced tools. SLIST_FOREACH_SAFE doesn't exist in the standard C library.
#define _DEFAULT_SOURCE // It's a preprocessor used to turn on certain features. here it's used to make sys/queue.h fully funcitonal
// this header defines the SLIST_FOREACH_SAFE
#define _GNU_SOURCE // similar to the above, used to enable non standard extension features used with the gnu library

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>     // time
#include <sys/time.h> //sys/time.h

// --- Constants ---
#define SERVER_PORT 9000 // define server port
#define RECV_BUFFER_SIZE 1024
#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"
#define LISTEN_BACKLOG 10     // Max pending connections for listen()
#define FILE_PERMISSIONS 0644 // rw-r--r-- for data file
#define TIMESTAMP_INTERVAL_SEC 10

// --- Global Variables ---
// Flag to signal graceful exit, set by signal_handler.
volatile sig_atomic_t exit_requested = 0; // volitate to prevent optimization, sig_atomic_t type garantees atmoic operations,
                                          // meaning the operation completes as a single unit uniterrupted, doring reading and writing
static timer_t timerid;

// concurrent programming without proper synchronization is like having mutliple people edit the same doc without coordination
// Global mutex for synchronizing access to DATA_FILE_PATH
pthread_mutex_t file_mutex;

// Mutex for protecting the thread list
pthread_mutex_t thread_list_mutex;

// --- Data Structures ---
// Structure to pass data to each client handling thread.
struct client_thread_data
{
    pthread_t thread_id;               // ID of the spawned thread
    int client_fd;                     // File descriptor for the client connection
    struct sockaddr_in client_address; // Client's address information
    volatile bool thread_exit_flag;    // Flag to request thread exit
    bool thread_complete;              // Flag to indicate thread completion
    SLIST_ENTRY(client_thread_data)
    entries; // For linked list
};

// Head of the singly linked list for active threads.
SLIST_HEAD(slisthead, client_thread_data)
thread_head = SLIST_HEAD_INITIALIZER(thread_head);

// --- Function Prototypes ---
void signal_handler(int signo);
void daemonize_process();
void *handle_client_connection_thread(void *arg);
void cleanup_resources(int server_socket_fd);
void cleanup_completed_threads();
void wait_for_all_threads();
void timer_handler(int sig, siginfo_t *si, void *uc);
int timer_setup(void);
void cleanup_timestamp_timer();

/**
 * @brief Signal handler for SIGINT and SIGTERM.
 *
 * Sets the global exit_requested flag to 1 to initiate graceful shutdown.
 * Also signals all active threads to exit.
 *
 * @param signo The signal number caught.
 */
void signal_handler(int signo)
{
    if (signo == SIGINT || signo == SIGTERM)
    {
        syslog(LOG_INFO, "Caught signal %d, initiating graceful exit.", signo);
        exit_requested = 1;

        // Signal all threads to exit
        pthread_mutex_lock(&thread_list_mutex);
        struct client_thread_data *thread_data;
        SLIST_FOREACH(thread_data, &thread_head, entries)
        {
            thread_data->thread_exit_flag = true;
        }
        pthread_mutex_unlock(&thread_list_mutex);
    }
}
void timer_handler(int sig, siginfo_t *si, void *uc)
{
    (void)sig; // Suppress unused parameter warning
    (void)si;  // Suppress unused parameter warning
    (void)uc;  // Suppress unused parameter warning
    // Why take them as input if we will supress them?

    /*
    **Create a New Signal Handler:** Write a function that will be called by your timer. Inside this function, perform the following sequence:
    - [/]  Get the current time using `time()`.
    - [/]  Convert the time to a `struct tm` using `localtime()`.
    - [/]  Format the `struct tm` into the required "timestamp:time" string using `strftime()` and your discovered format string.
    - [/]  **Lock the `file_mutex`**.
    - [/]  Open the data file (`/var/tmp/aesdsocketdata`) in append mode.
    - [/]  Write the formatted timestamp string to the file.
    - [/]  Close the file.
    - [/]  **Unlock the `file_mutex`**.
    */

    // get current time
    time_t current_time;
    struct tm *local_time;
    char time_buffer[100];

    current_time = time(NULL);
    if (current_time == ((time_t)-1))
    {
        syslog(LOG_ERR, "Failed to get current time: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    local_time = localtime(&current_time);
    if (!local_time)
    {
        syslog(LOG_ERR, "Failed to convert time to local time: %s", strerror(errno));
    }

    // format the time into RFC 2822 format %a, %d %b %Y %T %z"
    if (strftime(time_buffer, sizeof(time_buffer), "timestamp:%a, %d %b %Y %T %z\n", local_time) == 0)
    {
        syslog(LOG_ERR, "Failed to format time string");
        return;
    }

    // lock the file mutex
    pthread_mutex_lock(&file_mutex);
    int fd = open(DATA_FILE_PATH, O_WRONLY | O_APPEND | O_CREAT, FILE_PERMISSIONS);
    if (fd == -1)
    {
        syslog(LOG_ERR, "Failed to open file %s for writing for client: %s", DATA_FILE_PATH, strerror(errno));
    }

    // Write the timestamp to the file
    ssize_t bytes_written = write(fd, time_buffer, strlen(time_buffer));
    if (bytes_written == -1)
    {
        syslog(LOG_ERR, "Failed to write timestamp to file %s: %s",
               DATA_FILE_PATH, strerror(errno));
    }

    // Close the file
    close(fd);

    // Unlock mutex
    pthread_mutex_unlock(&file_mutex);
}

// declare the timer

int timer_setup(void)
{
    // setting sigevent timer
    struct sigevent sev;
    struct itimerspec timer_spec;

    // timer handler registeration
    struct sigaction sa_timer;
    memset(&sa_timer, 0, sizeof(sa_timer));
    sa_timer.sa_sigaction = timer_handler;
    sigemptyset(&sa_timer.sa_mask);
    sa_timer.sa_flags = SA_SIGINFO;

    if (sigaction(SIGRTMIN, &sa_timer, NULL) == -1)
    {
        syslog(LOG_ERR, "Failed to register timer handlers: %s", strerror(errno));
        return -1;
    }

    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;

    // Creating the timer
    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1)
    {
        syslog(LOG_ERR, "Failed to create timer: %s", strerror(errno));
        return -1;
    }

    // Configure timer to fire every 10 seconds
    timer_spec.it_value.tv_sec = TIMESTAMP_INTERVAL_SEC;
    timer_spec.it_value.tv_nsec = 0;
    timer_spec.it_interval.tv_sec = TIMESTAMP_INTERVAL_SEC;
    timer_spec.it_interval.tv_nsec = 0;

    if (timer_settime(timerid, 0, &timer_spec, NULL) == -1)
    {
        // log error, delete timer, return -1
        syslog(LOG_ERR, "Failed to set timer: %s", strerror(errno));
        timer_delete(timerid);
        return -1;
    }

    syslog(LOG_INFO, "Timestamp timer set to fire every 10 seconds.");
    return 0;
}

/**
 * @brief Converts the current process into a daemon.
 */
void daemonize_process()
{
    pid_t pid = fork();

    if (pid < 0)
    {
        syslog(LOG_ERR, "fork() failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0)
    {
        syslog(LOG_ERR, "setsid() failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (chdir("/") < 0)
    {
        syslog(LOG_ERR, "chdir('/') failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    int null_fd = open("/dev/null", O_RDWR);
    if (null_fd < 0)
    {
        syslog(LOG_ERR, "open('/dev/null') failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (dup2(null_fd, STDIN_FILENO) == -1 ||
        dup2(null_fd, STDOUT_FILENO) == -1 ||
        dup2(null_fd, STDERR_FILENO) == -1)
    {
        syslog(LOG_ERR, "dup2() failed: %s", strerror(errno));
        close(null_fd);
        exit(EXIT_FAILURE);
    }

    close(null_fd);
}

/**
 * @brief Thread function to handle communication with a single client.
 *
 * This function receives data from the client, appends it to DATA_FILE_PATH,
 * and then sends the entire file content back to the client.
 *
 * @param arg Pointer to client_thread_data structure
 * @return NULL
 */
void *handle_client_connection_thread(void *arg)
{
    struct client_thread_data *thread_data = (struct client_thread_data *)arg;
    int client_fd = thread_data->client_fd;
    char client_ip_str[INET_ADDRSTRLEN];

    // Convert client IP to string
    inet_ntop(AF_INET, &(thread_data->client_address.sin_addr), client_ip_str, INET_ADDRSTRLEN);

    syslog(LOG_INFO, "Accepted connection from %s", client_ip_str);

    char temp_buffer[RECV_BUFFER_SIZE];
    char *full_data_buffer = NULL;
    size_t total_received_len = 0;
    int newline_found = 0;
    ssize_t bytes_received;

    // --- Receive data from client until newline is found or error ---
    while (!newline_found && !thread_data->thread_exit_flag)
    {
        memset(temp_buffer, 0, RECV_BUFFER_SIZE);
        bytes_received = recv(client_fd, temp_buffer, RECV_BUFFER_SIZE, 0);

        if (bytes_received < 0)
        {
            if (errno == EINTR && thread_data->thread_exit_flag)
            {
                break; // Interrupted by signal, exit gracefully
            }
            syslog(LOG_ERR, "recv() failed for client %s: %s", client_ip_str, strerror(errno));
            break;
        }
        else if (bytes_received == 0)
        {
            syslog(LOG_INFO, "Client %s disconnected.", client_ip_str);
            break;
        }

        // Dynamically reallocate buffer to hold new data
        char *new_buffer = realloc(full_data_buffer, total_received_len + bytes_received + 1);
        if (!new_buffer)
        {
            syslog(LOG_ERR, "Memory reallocation failed for client %s", client_ip_str);
            break;
        }
        full_data_buffer = new_buffer;

        // Copy new data to the end of the aggregated buffer
        memcpy(full_data_buffer + total_received_len, temp_buffer, bytes_received);
        total_received_len += bytes_received;
        full_data_buffer[total_received_len] = '\0'; // Null-terminate for safety

        // Check if newline character is present in the received chunk
        if (memchr(temp_buffer, '\n', bytes_received))
        {
            newline_found = 1;
        }
    }

    // If we received data and found newline, process it
    if (newline_found && total_received_len > 0 && !thread_data->thread_exit_flag)
    {
        // --- Append received data to file (synchronized) ---
        pthread_mutex_lock(&file_mutex);

        int fd = open(DATA_FILE_PATH, O_WRONLY | O_APPEND | O_CREAT, FILE_PERMISSIONS);
        if (fd != -1)
        {
            ssize_t bytes_written = write(fd, full_data_buffer, total_received_len);
            if (bytes_written == -1)
            {
                syslog(LOG_ERR, "Failed to write to file %s for client %s: %s",
                       DATA_FILE_PATH, client_ip_str, strerror(errno));
            }
            else if (bytes_written != (ssize_t)total_received_len)
            {
                syslog(LOG_WARNING, "Partial write to file %s for client %s. Expected %zu, wrote %zd.",
                       DATA_FILE_PATH, client_ip_str, total_received_len, bytes_written);
            }
            close(fd);
        }
        else
        {
            syslog(LOG_ERR, "Failed to open file %s for writing for client %s: %s",
                   DATA_FILE_PATH, client_ip_str, strerror(errno));
        }

        pthread_mutex_unlock(&file_mutex);

        // --- Read entire file content and send back to client (synchronized) ---
        if (!thread_data->thread_exit_flag)
        {
            pthread_mutex_lock(&file_mutex);

            fd = open(DATA_FILE_PATH, O_RDONLY);
            if (fd != -1)
            {
                char read_buffer[RECV_BUFFER_SIZE];
                ssize_t bytes_read;

                while ((bytes_read = read(fd, read_buffer, RECV_BUFFER_SIZE)) > 0 &&
                       !thread_data->thread_exit_flag)
                {
                    ssize_t bytes_sent = send(client_fd, read_buffer, bytes_read, 0);
                    if (bytes_sent == -1)
                    {
                        syslog(LOG_ERR, "send() failed for client %s: %s", client_ip_str, strerror(errno));
                        break;
                    }
                    else if (bytes_sent != bytes_read)
                    {
                        syslog(LOG_WARNING, "Partial send to client %s. Expected %zd, sent %zd.",
                               client_ip_str, bytes_read, bytes_sent);
                    }
                }

                if (bytes_read == -1)
                {
                    syslog(LOG_ERR, "read() failed while sending data to client %s: %s",
                           client_ip_str, strerror(errno));
                }
                close(fd);
            }
            else
            {
                syslog(LOG_ERR, "Failed to open file %s for reading for client %s: %s",
                       DATA_FILE_PATH, client_ip_str, strerror(errno));
            }

            pthread_mutex_unlock(&file_mutex);
        }
    }

    // --- Cleanup for this client connection ---
    if (full_data_buffer)
    {
        free(full_data_buffer);
    }
    close(client_fd);
    syslog(LOG_INFO, "Closed connection from %s", client_ip_str);

    // Mark thread as complete
    pthread_mutex_lock(&thread_list_mutex);
    thread_data->thread_complete = true;
    pthread_mutex_unlock(&thread_list_mutex);

    return NULL;
}

/**
 * @brief Clean up completed threads and remove them from the list
 */
void cleanup_completed_threads()
{
    pthread_mutex_lock(&thread_list_mutex);

    struct client_thread_data *thread_data = SLIST_FIRST(&thread_head);
    while (thread_data != NULL)
    {
        struct client_thread_data *next_thread = SLIST_NEXT(thread_data, entries);
        if (thread_data->thread_complete)
        {
            pthread_join(thread_data->thread_id, NULL);                           // waits for the thread to finish, cleans up, prevents zombie
            SLIST_REMOVE(&thread_head, thread_data, client_thread_data, entries); //
            free(thread_data);
        }
        thread_data = next_thread;
    }

    pthread_mutex_unlock(&thread_list_mutex);
}

/**
 * @brief Wait for all remaining threads to complete
 */
void wait_for_all_threads()
{
    while (!SLIST_EMPTY(&thread_head))
    {
        struct client_thread_data *thread_data = SLIST_FIRST(&thread_head);
        SLIST_REMOVE_HEAD(&thread_head, entries);
        pthread_join(thread_data->thread_id, NULL);
        free(thread_data);
    }
}

/**
 * @brief Cleans up all allocated resources before program exit.
 */
void cleanup_resources(int server_socket_fd)
{
    syslog(LOG_INFO, "Performing cleanup...");

    // Clean up timer
    cleanup_timestamp_timer();

    // Wait for all threads to complete
    wait_for_all_threads();

    if (server_socket_fd != -1)
    {
        close(server_socket_fd);
    }
    unlink(DATA_FILE_PATH); // Delete the data file
    pthread_mutex_destroy(&file_mutex);
    pthread_mutex_destroy(&thread_list_mutex);
    closelog();
}

/**
 * @brief Clean up the timestamp timer
 */
void cleanup_timestamp_timer()
{
    if (timerid != 0) // check if the timer exists
    {
        if (timer_delete(timerid) != 0) // delete timer  then log status
        {
            syslog(LOG_ERR, "Failed to delete timer: %s", strerror(errno));
        }
        else
        {
            syslog(LOG_ERR, "timer deleted successfully");
        }
    }
}

/**
 * @brief Main entry point for the aesdsocket server.
 */
int main(int argc, char *argv[])
{
    int server_socket_fd = -1;
    int client_socket_fd;
    struct sockaddr_in server_address;
    struct sockaddr_storage client_address_storage;
    socklen_t client_addr_size;
    int reuse_addr_opt = 1;
    int daemon_mode = 0;

    // --- 1. System Logging Setup ---
    openlog("aesdsocket", LOG_PID | LOG_PERROR, LOG_USER);

    // --- 2. Signal Handler Registration ---
    struct sigaction sa_shutdown;
    memset(&sa_shutdown, 0, sizeof(sa_shutdown));
    sa_shutdown.sa_handler = signal_handler;
    sigemptyset(&sa_shutdown.sa_mask);
    sa_shutdown.sa_flags = 0;

    if (sigaction(SIGINT, &sa_shutdown, NULL) == -1 || sigaction(SIGTERM, &sa_shutdown, NULL) == -1)
    {
        syslog(LOG_ERR, "Failed to register signal handlers: %s", strerror(errno));
        closelog();
        return EXIT_FAILURE;
    }

    // --- 3. Parse Command-line Arguments ---
    if (argc == 2 && strcmp(argv[1], "-d") == 0)
    {
        daemon_mode = 1;
        syslog(LOG_INFO, "Starting aesdsocket in daemon mode.");
    }

    // --- 4. Initialize Mutexes ---
    if (pthread_mutex_init(&file_mutex, NULL) != 0)
    {
        syslog(LOG_ERR, "pthread_mutex_init for file_mutex failed: %s", strerror(errno));
        closelog();
        return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&thread_list_mutex, NULL) != 0)
    {
        syslog(LOG_ERR, "pthread_mutex_init for thread_list_mutex failed: %s", strerror(errno));
        pthread_mutex_destroy(&file_mutex);
        closelog();
        return EXIT_FAILURE;
    }

    // --- 5. Socket Initialization ---
    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd == -1)
    {
        syslog(LOG_ERR, "Socket creation failed: %s", strerror(errno));
        cleanup_resources(server_socket_fd);
        return EXIT_FAILURE;
    }

    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr_opt, sizeof(reuse_addr_opt)) == -1)
    {
        syslog(LOG_ERR, "setsockopt(SO_REUSEADDR) failed: %s", strerror(errno));
        cleanup_resources(server_socket_fd);
        return EXIT_FAILURE;
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(SERVER_PORT);

    if (bind(server_socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        syslog(LOG_ERR, "Binding to port %d failed: %s", SERVER_PORT, strerror(errno));
        cleanup_resources(server_socket_fd);
        return EXIT_FAILURE;
    }

    // --- 6. Daemonization (if requested) ---
    if (daemon_mode)
    {
        daemonize_process();
    }

    // --- 6.5. Setup Timestamp Timer ---
    if (timer_setup() != 0)
    {
        syslog(LOG_ERR, "Failed to setup timestamp timer: %s", strerror(errno));
        cleanup_resources(server_socket_fd);
        return EXIT_FAILURE;
    }

    // --- 7. Start Listening for Connections ---
    if (listen(server_socket_fd, LISTEN_BACKLOG) == -1)
    {
        syslog(LOG_ERR, "Listen failed: %s", strerror(errno));
        cleanup_resources(server_socket_fd);
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "Server listening on port %d...", SERVER_PORT);

    // --- 8. Main Server Loop: Accept and Handle Connections ---
    while (!exit_requested)
    {
        // Clean up completed threads periodically
        cleanup_completed_threads();

        client_addr_size = sizeof(client_address_storage);
        client_socket_fd = accept(server_socket_fd, (struct sockaddr *)&client_address_storage, &client_addr_size);

        if (client_socket_fd == -1)
        {
            if (errno == EINTR)
            {
                if(exit_requested){
                    break;  // exiting, so break
                }
                continue;
            }
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            continue;
        }

        // Create new thread data structure
        struct client_thread_data *thread_data = malloc(sizeof(struct client_thread_data));
        if (!thread_data)
        {
            syslog(LOG_ERR, "Failed to allocate memory for thread data");
            close(client_socket_fd);
            continue;
        }

        thread_data->client_fd = client_socket_fd;
        thread_data->client_address = *((struct sockaddr_in *)&client_address_storage);
        thread_data->thread_exit_flag = false;
        thread_data->thread_complete = false;

        // Create new thread
        int thread_result = pthread_create(&thread_data->thread_id, NULL,
                                           handle_client_connection_thread, thread_data);
        if (thread_result != 0)
        {
            syslog(LOG_ERR, "Failed to create thread: %s", strerror(thread_result));
            free(thread_data);
            close(client_socket_fd);
            continue;
        }

        // Add thread to linked list
        pthread_mutex_lock(&thread_list_mutex);
        SLIST_INSERT_HEAD(&thread_head, thread_data, entries);
        pthread_mutex_unlock(&thread_list_mutex);
    }

    // --- 9. Graceful Shutdown and Cleanup ---
    cleanup_resources(server_socket_fd);
    syslog(LOG_INFO, "Server gracefully shut down.");

    return EXIT_SUCCESS;
}
