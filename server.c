#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>

struct message {
    char source[50];
    char target[50];
    char msg[200];
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

    if (access("serverFIFO", F_OK) == -1) {
        if (mkfifo("serverFIFO", 0666) < 0) {
            perror("Failed to create serverFIFO");
            exit(EXIT_FAILURE);
        }
    }

    server = open("serverFIFO", O_RDONLY);
    if (server < 0) {
        perror("Failed to open serverFIFO for reading");
        exit(EXIT_FAILURE);
    }

    dummyfd = open("serverFIFO", O_WRONLY);
    if (dummyfd < 0) {
        perror("Failed to open serverFIFO for writing");
        close(server);
        exit(EXIT_FAILURE);
    }

    while (1) {
        if (read(server, &req, sizeof(req)) <= 0) {
            perror("Error reading from serverFIFO");
            continue;
        }

        printf("Received a request from %s to send the message %s to %s.\n",
               req.source, req.msg, req.target);
        fflush(stdout);

        target = open(req.target, O_WRONLY);
        if (target < 0) {
            continue;
        }

        if (write(target, &req, sizeof(req)) <= 0) {
            perror("Error writing to target FIFO");
        }

        close(target);
    }

    close(server);
    close(dummyfd);
    return 0;
}