
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

struct message {
    char source[50];
    char target[50];
    char msg[200]; // message body
};

void terminate(int sig) {
    printf("Exiting....\n");
    fflush(stdout);
    exit(0);
}

int main() {
    int server;
    int target;
    int dummyfd;
    struct message req;
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, terminate);

    // Open server FIFO for reading and writing
    server = open("serverFIFO", O_RDONLY);
    if (server < 0) {
        perror("Failed to open serverFIFO for reading");
        exit(EXIT_FAILURE);
    }
    dummyfd = open("serverFIFO", O_WRONLY); // Keeps FIFO open for writing
    if (dummyfd < 0) {
        perror("Failed to open serverFIFO for writing");
        close(server);
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Read requests from serverFIFO
        if (read(server, &req, sizeof(req)) <= 0) {
            perror("Error reading from serverFIFO");
            continue;
        }

        printf("Received a request from %s to send the message '%s' to %s.\n",
               req.source, req.msg, req.target);

        // Open target FIFO and write the whole message struct to the target FIFO
        char targetFIFO[60];
        snprintf(targetFIFO, sizeof(targetFIFO), "%s", req.target);
        target = open(targetFIFO, O_WRONLY);
        if (target < 0) {
            perror("Failed to open target FIFO");
            continue;
        }

        if (write(target, &req, sizeof(req)) <= 0) {
            perror("Error writing to target FIFO");
        } else {
            printf("Message sent to %s successfully.\n", req.target);
        }

        close(target); // Close target FIFO after writing the message
    }

    close(server);
    close(dummyfd);
    return 0;
}