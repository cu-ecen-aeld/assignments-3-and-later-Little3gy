# aesd-assignments
This repo contains public starter source code, scripts, and documentation for Advanced Embedded Software Development (ECEN-5713) and Advanced Embedded Linux Development assignments University of Colorado, Boulder.

## Setting Up Git

Use the instructions at [Setup Git](https://help.github.com/en/articles/set-up-git) to perform initial git setup steps. For AESD you will want to perform these steps inside your Linux host virtual or physical machine, since this is where you will be doing your development work.

## Setting up SSH keys

See instructions in [Setting-up-SSH-Access-To-your-Repo](https://github.com/cu-ecen-aeld/aesd-assignments/wiki/Setting-up-SSH-Access-To-your-Repo) for details.

## Specific Assignment Instructions

Some assignments require further setup to pull in example code or make other changes to your repository before starting.  In this case, see the github classroom assignment start instructions linked from the assignment document for details about how to use this repository.

## Testing

The basis of the automated test implementation for this repository comes from [https://github.com/cu-ecen-aeld/assignment-autotest/](https://github.com/cu-ecen-aeld/assignment-autotest/)

The assignment-autotest directory contains scripts useful for automated testing  Use
```
git submodule update --init --recursive
```
to synchronize after cloning and before starting each assignment, as discussed in the assignment instructions.

As a part of the assignment instructions, you will setup your assignment repo to perform automated testing using github actions.  See [this page](https://github.com/cu-ecen-aeld/aesd-assignments/wiki/Setting-up-Github-Actions) for details.

Note that the unit tests will fail on this repository, since assignments are not yet implemented.  That's your job :) 

## Assignment 5: Socket Server Application

This assignment involves creating a simple socket server application in C.

### Project Description

The application, `aesdsocket`, is a TCP socket server that listens on port 9000. It accepts multiple connections, receiving data from clients and appending it to a file located at `/var/tmp/aesdsocketdata`. After each write, the full content of the file is sent back to the client. The server gracefully handles `SIGINT` and `SIGTERM` signals for a clean shutdown, and it can be run as a daemon.

### Key Technologies

*   **C:** The application is written in C.
*   **Linux Sockets API:** Uses the standard Berkeley sockets API for network communication.
*   **POSIX Signals:** Implements signal handling for graceful shutdown.
*   **Daemonization:** Can run as a background process.

### How to Build and Run

1.  **Navigate to the `server` directory:**
    ```bash
    cd server
    ```

2.  **Build the application:**
    ```bash
    make
    ```

3.  **Run the server:**
    ```bash
    ./aesdsocket
    ```

4.  **Run the server as a daemon:**
    ```bash
    ./aesdsocket -d
    ```

5.  **Test the server:**
    You can use `netcat` (or `nc`) to connect to the server:
    ```bash
    nc localhost 9000
    ```
    Type a message and press Enter. The server will send the message back to you.

6.  **Clean up:**
    ```bash
    make clean
    ``` 
