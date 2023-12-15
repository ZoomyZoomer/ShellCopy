#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_OUTPUT_LENGTH 4096

// Function to execute a command in the shell and read the output
void executeCommand(int in_fd, int out_fd, const char* command, char* output) {
    write(in_fd, command, strlen(command)); // Send command to shell

    // Read the output of the command
    ssize_t n_read = read(out_fd, output, MAX_OUTPUT_LENGTH - 1);
    if (n_read > 0) {
        output[n_read] = '\0'; // Null-terminate the string
    } else {
        output[0] = '\0'; // Empty string if nothing was read
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <path to shell executable>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int in_pipe[2], out_pipe[2];
    if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) {
        perror("Failed to create pipes");
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("Failed to fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        // Child process: Execute the shell
        dup2(in_pipe[0], STDIN_FILENO);  // Redirect stdin to read from in_pipe
        dup2(out_pipe[1], STDOUT_FILENO); // Redirect stdout to write to out_pipe
        close(in_pipe[1]);   // Close unused write end of in_pipe
        close(out_pipe[0]);  // Close unused read end of out_pipe

        execl(argv[1], argv[1], (char*) NULL); // Execute the shell
        perror("execl failed");
        exit(EXIT_FAILURE);
    }

    // Parent process: Send commands and read responses
    close(in_pipe[0]);   // Close unused read end of in_pipe
    close(out_pipe[1]);  // Close unused write end of out_pipe

    char output[MAX_OUTPUT_LENGTH];

    // Example test cases
    executeCommand(in_pipe[1], out_pipe[0], "cd /valid/directory\n", output);
    printf("Output for 'cd /valid/directory': %s\n", output);

    executeCommand(in_pipe[1], out_pipe[0], "cd /invalid/directory\n", output);
    printf("Output for 'cd /invalid/directory': %s\n", output);

    executeCommand(in_pipe[1], out_pipe[0], "pwd\n", output);
    printf("Output for 'pwd': %s\n", output);

    // Test 'which' command
    executeCommand(in_pipe[1], out_pipe[0], "which *.c\n", output);
    printf("Output for 'which *.c': %s\n", output);

    // Test wildcard usage with 'ls'
    executeCommand(in_pipe[1], out_pipe[0], "ls *.txt\n", output);
    printf("Output for 'ls *.txt': %s\n", output);

    // Test 'echo' command
    executeCommand(in_pipe[1], out_pipe[0], "echo Hello, world!\n", output);
    printf("Output for 'echo Hello, world!': %s\n", output);

    // Test redirection (assuming 'echo' and file creation work in your shell)
    executeCommand(in_pipe[1], out_pipe[0], "echo Test > testfile.txt\n", output);
    printf("Output for 'echo Test > testfile.txt': %s\n", output);

    // Test reading a file (assuming 'cat' is available in your shell)
    executeCommand(in_pipe[1], out_pipe[0], "cat testfile.txt\n", output);
    printf("Output for 'cat testfile.txt': %s\n", output);

    // Test pipe (assuming 'grep' is available in your shell)
    executeCommand(in_pipe[1], out_pipe[0], "echo 'This is a test' | grep 'test'\n", output);
    printf("Output for piping: %s\n", output);

    // Clean up
    close(in_pipe[1]);
    close(out_pipe[0]);
    waitpid(pid, NULL, 0);

    return EXIT_SUCCESS;
}
