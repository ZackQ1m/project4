#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {"cp", "touch", "mkdir", "ls", "pwd", "cat", "grep", "chmod", "diff", "cd", "exit", "help", "sendmsg"};

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

void sendmsg(char *user, char *target, char *msg) {
    // Send a request to the server to send the message (msg) to the target user (target)
    struct message req;
    int server;

    // Fill the message structure
    strcpy(req.source, user);
    strcpy(req.target, target);
    strcpy(req.msg, msg);

    // Open the server FIFO for writing
    server = open("serverFIFO", O_WRONLY);
    if (server < 0) {
        perror("Failed to open serverFIFO");
        return;
    }

    // Write the message struct to the server FIFO
    if (write(server, &req, sizeof(req)) <= 0) {
        perror("Failed to write to serverFIFO");
    }

    close(server);
}

void *messageListener(void *arg) {
    // Read user's own FIFO in an infinite loop for incoming messages
    struct message incoming;
    int userFIFO;

    // Open user's FIFO for reading
    char fifoName[60];
    snprintf(fifoName, sizeof(fifoName), "%s", (char *)arg);

    userFIFO = open(fifoName, O_RDONLY);
    if (userFIFO < 0) {
        perror("Failed to open user FIFO");
        pthread_exit((void *)1);
    }

    while (1) {
        if (read(userFIFO, &incoming, sizeof(incoming)) > 0) {
            // Print the incoming message
            printf("Incoming message from [%s]: %s\n", incoming.source, incoming.msg);
        }
    }

    close(userFIFO);
    pthread_exit((void *)0);
}

int isAllowed(const char *cmd) {
    for (int i = 0; i < N; i++) {
        if (strcmp(cmd, allowed[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    pid_t pid;
    char **cargv;
    char *path;
    char line[256];
    int status;
    pthread_t listenerThread;

    if (argc != 2) {
        printf("Usage: ./rsh <username>\n");
        exit(1);
    }
    signal(SIGINT, terminate);

    strcpy(uName, argv[1]);

    // Create the message listener thread
    if (pthread_create(&listenerThread, NULL, messageListener, (void *)uName) != 0) {
        perror("Failed to create message listener thread");
        exit(1);
    }

    while (1) {
        fprintf(stderr, "rsh>");

        if (fgets(line, 256, stdin) == NULL) continue;
        if (strcmp(line, "\n") == 0) continue;

        line[strlen(line) - 1] = '\0';

        char cmd[256];
        char line2[256];
        strcpy(line2, line);
        strcpy(cmd, strtok(line, " "));

        if (!isAllowed(cmd)) {
            printf("NOT ALLOWED!\n");
            continue;
        }

        if (strcmp(cmd, "sendmsg") == 0) {
            // Handle the `sendmsg` command
            char *target = strtok(NULL, " ");
            if (target == NULL) {
                printf("sendmsg: you have to specify target user\n");
                continue;
            }

            char *msg = strtok(NULL, "");
            if (msg == NULL) {
                printf("sendmsg: you have to enter a message\n");
                continue;
            }

            sendmsg(uName, target, msg);
            continue;
        }

        if (strcmp(cmd, "exit") == 0) break;

        if (strcmp(cmd, "cd") == 0) {
            char *targetDir = strtok(NULL, " ");
            if (strtok(NULL, " ") != NULL) {
                printf("-rsh: cd: too many arguments\n");
            } else {
                chdir(targetDir);
            }
            continue;
        }

        if (strcmp(cmd, "help") == 0) {
            printf("The allowed commands are:\n");
            for (int i = 0; i < N; i++) {
                printf("%d: %s\n", i + 1, allowed[i]);
            }
            continue;
        }

        cargv = (char **)malloc(sizeof(char *));
        cargv[0] = (char *)malloc(strlen(cmd) + 1);
        path = (char *)malloc(9 + strlen(cmd) + 1);
        strcpy(path, cmd);
        strcpy(cargv[0], cmd);

        char *attrToken = strtok(line2, " ");
        attrToken = strtok(NULL, " ");
        int n = 1;
        while (attrToken != NULL) {
            n++;
            cargv = (char **)realloc(cargv, sizeof(char *) * n);
            cargv[n - 1] = (char *)malloc(strlen(attrToken) + 1);
            strcpy(cargv[n - 1], attrToken);
            attrToken = strtok(NULL, " ");
        }
        cargv = (char **)realloc(cargv, sizeof(char *) * (n + 1));
        cargv[n] = NULL;

        posix_spawnattr_t attr;
        posix_spawnattr_init(&attr);

        if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0) {
            perror("spawn failed");
            exit(EXIT_FAILURE);
        }

        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            exit(EXIT_FAILURE);
        }

        posix_spawnattr_destroy(&attr);
    }

    return 0;
}