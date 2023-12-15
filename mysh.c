#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <limits.h>
#include <linux/limits.h>
#include <glob.h>

#define INITIAL_SIZE 10
#define BUFFER_SIZE 100
#define PATH_MAX_LENGTH 4096

int globalCount = 0;
int RUNNING = 0;
int init_run = 0;
int count = 0;
int cCount = 0;
int fd_global[2];
sem_t semaphore;
int isExit = 0;
int lineCount = 0;
int status_Size = 20;
int *statusArray;

#define MAX_ARGS 128

char **tokenize();

void which();
void cd();
void pwd();
void isProgram();
void redirect();
void pipeTokens();

int findPipeOrRedirect(char *myTokens[], int tokenCount)
{

    for (int i = 0; i < tokenCount; i++)
    {

        if (strcmp(myTokens[i], "|") == 0)
        {
            pipeTokens(myTokens, tokenCount);
            return 1;
        }
        else if (strcmp(myTokens[i], ">") == 0)
        {
            redirect(myTokens, tokenCount);
            return 1;
        }
        else if (strcmp(myTokens[i], "<") == 0)
        {
            redirect(myTokens, tokenCount);
            return 1;
        }
    }

    return 0;
}

void decisionTree();

void createChild(char *myTokens[], int tokenCount, int lineCount)
{

    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        printf("ERROR: Could not create pipe");
        statusArray[lineCount] = 0;
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid == -1)
    {
        printf("ERROR: Could not create child process");
        statusArray[lineCount] = 0;
    }

    if (pid == 0)
    {

        if (strcmp(myTokens[0], "then") == 0)
        {

            if (lineCount == 0)
            {
                printf("'then' command is not applicable");
                exit(EXIT_FAILURE);
            }
            else
            {

                if (statusArray[lineCount - 1] == 1)
                {

                    int miniIndex = 0;
                    char *tempArr[tokenCount];

                    for (int i = 0; i < tokenCount; i++)
                    {

                        tempArr[i] = myTokens[i + 1];
                    }

                    decisionTree(tempArr, tokenCount - 1, pipefd, lineCount);

                    exit(EXIT_SUCCESS);
                }
                else
                {
                    exit(EXIT_FAILURE);
                }
            }
        }
        else if (strcmp(myTokens[0], "else") == 0)
        {

            if (lineCount == 0)
            {
                printf("'else' command is not applicable");
                exit(EXIT_FAILURE);
            }
            else
            {

                if (statusArray[lineCount - 1] == 0)
                {

                    int miniIndex = 0;
                    char *tempArr[tokenCount];

                    for (int i = 0; i < tokenCount; i++)
                    {

                        tempArr[i] = myTokens[i + 1];
                    }

                    decisionTree(tempArr, tokenCount - 1, pipefd, lineCount);

                    exit(EXIT_SUCCESS);
                }
                else
                {
                    exit(EXIT_FAILURE);
                }
            }
        }

        decisionTree(myTokens, tokenCount, pipefd, lineCount);

        exit(EXIT_SUCCESS);
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status))
    {

        int exitStatus = WEXITSTATUS(status);
        if (exitStatus == 0)
        {
            statusArray[lineCount] = 1;
        }
        else
        {
            statusArray[lineCount] = 0;
        }
    }

    if (strcmp(myTokens[0], "cd") == 0)
    {

        if (chdir(myTokens[1]) != 0)
        {
            printf("ERROR: Not a valid directory / folder: '%s' \n", myTokens[1]);
            statusArray[lineCount] = 0;
        }
        else
        {
            statusArray[lineCount] = 1;
        }
    }
}

void decisionTree(char **myTokens, int tokenCount, int pipefd[], int lineCount)
{

    if (findPipeOrRedirect(myTokens, tokenCount) == 1)
    {
        redirect(myTokens, tokenCount);
        printf("ERROR: Process not killed\n");
    }
    else if (strcmp(myTokens[0], "cd") == 0)
    {
        // Check if cd was called properly (only 1 arg)
        if (myTokens[2] != NULL)
        {
            printf("Error: Called 'cd' with too many arguments\n");
            statusArray[lineCount] = 0;
            exit(EXIT_FAILURE);
        }

        if (myTokens[1] == NULL)
        {
            printf("cd: missing argument\n");
            statusArray[lineCount] = 0;
            exit(EXIT_FAILURE);
        }

        close(pipefd[0]);

        char *newDir = myTokens[1];

        write(pipefd[1], newDir, strlen(newDir) + 1);

        close(pipefd[1]);
    }
    else if (strcmp(myTokens[0], "pwd") == 0)
    {

        if (myTokens[1] == NULL)
        {
            pwd();
        }
        else
        {
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(myTokens[0], "which") == 0)
    {
        which(myTokens[1]);
    }
    else if (strcmp(myTokens[0], "cd") != 0)
    {
        char *args[tokenCount];
        int index = 0;

        for (int i = 0; myTokens[i + 1] != NULL; i++)
        {
            args[i] = myTokens[i + 1];
            index = i;
        }

        args[index + 1] = NULL;
        if (myTokens[1] == NULL)
        {
            args[0] = NULL;
        }

        isProgram(myTokens, tokenCount, args);
    }
}

void redirect(char *tokens[], int tokenCount)
{

    char *args[tokenCount];
    int argIndex = 0;
    int hasArg = 0;

    // Iterate through tokens
    for (int i = 0; tokens[i] != NULL; ++i)
    {
        if (strcmp(tokens[i], "<") == 0)
        {
            // Input redirection
            int inputFile = open(tokens[i + 1], O_RDONLY);
            if (inputFile == -1)
            {
                perror("Error opening input file");
                exit(EXIT_FAILURE);
            }
            dup2(inputFile, STDIN_FILENO);
            close(inputFile);
            i++; // Skip the filename token
        }
        else if (strcmp(tokens[i], ">") == 0)
        {
            // Output redirection
            int outputFile = open(tokens[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);
            if (outputFile == -1)
            {
                printf("ERROR opening output file");
                exit(EXIT_FAILURE);
            }
            dup2(outputFile, STDOUT_FILENO);
            close(outputFile);
            i++; // Skip the filename token
        }
        else
        {
            if (i != 0)
            {

                args[argIndex] = tokens[i];

                argIndex++;
                hasArg++;
            }
        }
    }

    args[hasArg] = NULL;

    isProgram(tokens, tokenCount, args);
}

void reallocStatus()
{

    status_Size *= 2;

    int *newArray = (int *)realloc(statusArray, status_Size * sizeof(int));

    if (newArray == NULL)
    {

        printf("FAILED TO REALLOC STATUS ARRAY");
        return;
    }

    for (int i = 0; i < status_Size / 2; i++)
    {

        newArray[i] = statusArray[i];
    }

    free(statusArray);

    statusArray = newArray;
}

void pipeTokens(char **tokens, int tokenCount)
{

    char *tokensLeft[tokenCount];
    char *tokensRight[tokenCount];
    tokens[tokenCount] = NULL;
    int pipeIndex = 0;
    int count = 0;

    for (int i = 0; i < tokenCount; i++)
    {

        if (strcmp(tokens[i], "|") != 0)
        {
            tokensLeft[i] = tokens[i];
        }
        else
        {
            pipeIndex = i;
            break;
        }
    }

    tokensLeft[pipeIndex] = NULL;

    for (int j = pipeIndex + 1; tokens[j] != NULL; j++)
    {

        tokensRight[count] = tokens[j];
        count++;
    }

    tokensRight[count] = NULL;

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1)
    {
        printf("ERORR: Could not create pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid_child1 = fork();

    if (pid_child1 == -1)
    {
        printf("FORK ERROR");
        exit(EXIT_FAILURE);
    }

    if (pid_child1 == 0)
    {

        close(pipe_fd[0]);

        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);

        redirect(tokensLeft, pipeIndex);

        printf("EXECV FAILED: CHILD1");
        exit(EXIT_FAILURE);
    }

    waitpid(pid_child1, NULL, 0);

    pid_t pid_child2 = fork();

    if (pid_child2 == -1)
    {

        printf("FORK ERROR");
        exit(EXIT_FAILURE);
    }

    if (pid_child2 == 0)
    {

        close(pipe_fd[1]);

        dup2(pipe_fd[0], STDIN_FILENO);
        close(pipe_fd[0]);

        redirect(tokensRight, count);

        printf("EXECV FAILED: CHILD2");
        exit(EXIT_FAILURE);
    }

    close(pipe_fd[0]);
    close(pipe_fd[1]);

    waitpid(pid_child2, NULL, 0);
}

/*void testChild(){
    pid_t pid = fork();

    if (pid == 0){
        printf("OK CHILD MADE");
        return;
    }


}*/

int batchMode(char *file)
{
    int fd = open(file, O_RDONLY);

    if (fd == -1)
    {
        printf("Unable to open file: %s\n", file);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    char *line = NULL;
    int lineSize = 0;
    char **myTokens;

    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0)
    {
        for (int i = 0; i < bytesRead; ++i)
        {
            if (buffer[i] == '\n' || buffer[i] == '\0')
            {
                if (lineSize > 0)
                {
                    myTokens = tokenize(line);

                    createChild(myTokens, globalCount, lineCount);

                    globalCount = 0;
                    lineCount++;

                    if (lineCount >= status_Size)
                    {
                        reallocStatus();
                    }

                    free(line);
                    myTokens = NULL;
                    line = NULL;
                    lineSize = 0;
                }
            }
            else
            {
                line = realloc(line, lineSize + 2);
                line[lineSize++] = buffer[i];
                line[lineSize] = '\0';
            }
        }
    }

    // Process any remaining content after the loop
    if (lineSize > 0)
    {
        myTokens = tokenize(line);

        createChild(myTokens, globalCount, lineCount);

        free(line);
        globalCount = 0;
        lineCount++;
    }

    if (bytesRead == -1)
    {
        perror("Error reading file");
    }

    close(fd);
}

void freeTokens(char **tokens, int count)
{
    for (int i = 0; i < count; ++i)
    {
        free(tokens[i]);
    }
    free(tokens);
}

void printReady()
{
    fflush(stdout);
    printf("mysh> ");
    fflush(stdout);
}

void cd(char **myTokens)
{

    // Check if cd was called properly (only 1 arg)
    if (myTokens[2] != NULL)
    {
        printf("Error: Called 'cd' with too many arguments\n");
        exit(EXIT_FAILURE);
        return;
    }

    if (myTokens[1] == NULL)
    {
        fprintf(stderr, "cd: missing argument\n");
        exit(EXIT_FAILURE);
        return;
    }

    if (chdir(myTokens[1]) != 0)
    {
        perror("cd");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

void pwd()
{
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("%s\n", cwd);
    }
    else
    {
        perror("getcwd");
    }
}

int matchWildcard(const char *pattern, const char *str)
{
    while (*pattern)
    {
        if (*pattern == '*')
        {
            while (*str)
            {
                if (matchWildcard(pattern + 1, str))
                {
                    return 1;
                }
                ++str;
            }
            return matchWildcard(pattern + 1, str);
        }
        else if (*str && (*pattern == *str || *pattern == '?'))
        {
            ++pattern;
            ++str;
        }
        else
        {
            return 0;
        }
    }
    return !*str;
}

int isExecutable(const char *path)
{
    struct stat st;

    if (stat(path, &st) == 0)
    {

        return S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR);
    }

    return 0;
}

int searchFileInDirectory(const char *dirPath, const char *program, char *foundPath)
{
    DIR *dir = opendir(dirPath);
    if (dir == NULL)
    {
        return 0;
    }

    struct dirent *entry;
    struct stat entry_stat;
    int found = 0;

    while ((entry = readdir(dir)) != NULL && !found)
    {
        // Skip the . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        // Construct the full path of the current entry
        char fullPath[PATH_MAX];
        snprintf(fullPath, PATH_MAX, "%s/%s", dirPath, entry->d_name);

        // Get file status
        if (stat(fullPath, &entry_stat) == -1)
        {
            continue; // Cannot stat entry, skip it
        }

        // Check if the entry is a directory
        if (S_ISDIR(entry_stat.st_mode))
        {
            // Recursively search in the sub-directory
            found = searchFileInDirectory(fullPath, program, foundPath);
        }
        else if (strcmp(entry->d_name, program) == 0)
        {
            // Check if the current entry is the file we're searching for
            strcpy(foundPath, fullPath);
            found = 1;
        }
    }

    closedir(dir);
    return found;
}

void listFilesMatchingPattern(const char *basePath, const char *pattern)
{
    DIR *dir;
    struct dirent *entry;
    struct stat path_stat;

    if (!(dir = opendir(basePath)))
    {
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char path[1000];
        snprintf(path, sizeof(path), "%s/%s", basePath, entry->d_name);

        if (stat(path, &path_stat) != 0)
        {
            // Error handling if stat fails
            continue;
        }

        if (S_ISDIR(path_stat.st_mode))
        {
            listFilesMatchingPattern(path, pattern);
        }
        else
        {
            if (matchWildcard(pattern, entry->d_name))
            {
                printf("%s\n", path);
            }
        }
    }
    closedir(dir);
}

void searchFilesMatchingPattern(const char *basePath, const char *pattern)
{
    DIR *dir;
    struct dirent *entry;
    struct stat path_stat;

    if (!(dir = opendir(basePath)))
    {
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", basePath, entry->d_name);

        if (stat(path, &path_stat) != 0)
        {
            continue; // Error handling if stat fails
        }

        if (S_ISDIR(path_stat.st_mode))
        {
            searchFilesMatchingPattern(path, pattern);
        }
        else
        {
            if (matchWildcard(pattern, entry->d_name))
            {
                printf("%s\n", path);
            }
        }
    }
    closedir(dir);
}

// Modify the 'which' function to handle wildcard patterns
void which(char *pattern)
{
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("getcwd");
        return;
    }

    glob_t glob_result;
    glob(pattern, GLOB_NOCHECK | GLOB_TILDE, NULL, &glob_result);
    for (size_t i = 0; i < glob_result.gl_pathc; i++)
    {
        searchFilesMatchingPattern(cwd, glob_result.gl_pathv[i]);
    }
    globfree(&glob_result);
}

void isProgram(char **myTokens, int tokenCount, char *args[])
{

    const char *command = myTokens[0];

    const char *paths[] = {"/bin", "/usr/bin", "/usr/local/bin", NULL};

    char *newArgs[tokenCount + 1];
    int index;

    if (args[0] != NULL)
    {

        for (int i = 1; args[i - 1] != NULL; i++)
        {
            newArgs[i] = args[i - 1];
            index = i;
        }

        newArgs[index + 1] = NULL;
    }
    else
    {

        newArgs[1] = NULL;
    }

    for (int i = 0; paths[i] != NULL; ++i)
    {

        char full_path[256];
        snprintf(full_path, sizeof(full_path), "%s/%s", paths[i], command);

        newArgs[0] = full_path;

        execv(full_path, newArgs);
    }

    printf("ERROR: Execv failed\n");
    exit(EXIT_FAILURE);
}

void addToken(char ***tokens, int *size, int *count, const char *token)
{
    // Check if resizing is needed
    if (*count >= *size)
    {
        *size *= 2;
        *tokens = (char **)realloc(*tokens, *size * sizeof(char *));
        if (*tokens == NULL)
        {
            fprintf(stderr, "Memory allocation error\n");
            return;
        }
    }

    // Allocate memory for the token and copy it
    (*tokens)[*count] = (char *)malloc(strlen(token) + 1);
    if ((*tokens)[*count] == NULL)
    {
        fprintf(stderr, "Memory allocation error\n");
        return;
    }
    strcpy((*tokens)[*count], token);

    // Increment the count
    (*count)++;
}

// void freeTokens(char **tokens, int count) {
//     for (int i = 0; i < count; ++i) {
//         free(tokens[i]);
//     }
//     free(tokens);
// }

char **tokenize(char *line)
{
    char **tokens; // Declare outside the loop
    int size, count;

    tokens = (char **)malloc(INITIAL_SIZE * sizeof(char *));
    if (tokens == NULL)
    {
        printf("Memory allocation error\n");
        return NULL;
    }

    size = INITIAL_SIZE;
    count = 0;

    char buffer[BUFFER_SIZE];

    // Read user input
    if (line == NULL)
    {
        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        {
            printf("Error reading input\n");
            freeTokens(tokens, count); // Free memory before returning
            return NULL;
        }

        // Tokenize the input
        char *token = strtok(buffer, " \t\n");

        while (token != NULL)
        {
            addToken(&tokens, &size, &count, token);
            token = strtok(NULL, " \t\n");
        }
    }
    else
    {
        char *token = strtok(line, " \t\n");

        while (token != NULL)
        {
            addToken(&tokens, &size, &count, token);
            token = strtok(NULL, " \t\n");
        }
    }

    globalCount = count;

    return tokens;
}

void interactiveMode()
{
    printf("Welcome to my shell!\n");

    while (1)
    { // Changed to a simple infinite loop
        printReady();

        char inputLine[BUFFER_SIZE];
        if (fgets(inputLine, sizeof(inputLine), stdin) == NULL)
        {
            if (feof(stdin))
            {                 // Check for end of file
                printf("\n"); // Print a newline for a clean exit
                break;        // Exit the loop if end of file is reached
            }
            else
            {
                perror("Error reading input");
                continue; // Handle other input errors
            }
        }

        // Remove the newline character at the end of the input
        inputLine[strcspn(inputLine, "\n")] = '\0';

        // Check for empty input
        if (inputLine[0] == '\0')
        {
            continue; // Skip to the next iteration if input is empty
        }

        char **myTokens = tokenize(inputLine); // Use the inputLine here
        if (myTokens == NULL || myTokens[0] == NULL)
        {
            fprintf(stderr, "Invalid input or memory allocation error\n");
            continue; // Skip to the next iteration
        }

        // Handle 'exit' command immediately in the parent process
        if (strcmp(myTokens[0], "exit") == 0)
        {
            fprintf(stderr, "mysh: exiting\n");
            freeTokens(myTokens, globalCount); // Free tokens before exiting
            break;                             // Break the loop to exit the shell!
        }

        // Fork a child process to handle other commands
        createChild(myTokens, globalCount, lineCount);
        lineCount++;

        freeTokens(myTokens, globalCount); // Free the tokens after processing
    }
}

void testfunctions()
{

    // Test execution of non-built-in programs

    printf("Testing execution of 'ls': \n");
    batchMode("testls.txt");

    // Test execution of which command

    printf("Testing execution of 'which': \n");
    batchMode("testwhich.txt");
}

int main(int argc, char *argv[])
{
    // Initialize status array for command status tracking
    statusArray = (int *)malloc(status_Size * sizeof(int));

    // Test cases
    if (argc > 1 && strcmp(argv[1], "_testcase_") == 0)
    {
        printf("Running test cases...\n");

        // Test 1: Execution of an external command (ls)
        printf("Test 1: Executing 'ls'\n");
        char *test1[] = {"ls", NULL};
        createChild(test1, 1, 0);

        // Test 2: Using 'cd' to change directory
        printf("Test 2: Changing directory using 'cd'\n");
        char *test2[] = {"cd", "..", NULL};
        createChild(test2, 2, 0);

        // Test 3: Using 'pwd' to print current directory
        printf("Test 3: Printing current directory using 'pwd'\n");
        char *test3[] = {"pwd", NULL};
        createChild(test3, 1, 0);

        // Test 4: Redirection with 'echo'
        printf("Test 4: Redirecting 'echo' output to a file\n");
        char *test4[] = {"echo", "Hello World", ">", "testfile.txt", NULL};
        createChild(test4, 4, 0);

        // Test 5: Using 'which' with a wildcard pattern
        printf("Test 5: Finding files using 'which' with a wildcard\n");
        char *test5[] = {"which", "*.txt", NULL};
        createChild(test5, 2, 0);

        // Test 6: Piping between two commands
        printf("Test 6: Piping between 'echo' and 'echo'\n");
        char *test6[] = {"echo", "hi", "|", "echo", "test", NULL};
        createChild(test6, 5, 0);

        // Test 7: Then command
        printf("Test 7: Testing the 'then' token functionallity with a successful pwd command call beforehand\n");
        char *test7[] = {"pwd", NULL};
        createChild(test7, 1, 0);

        char *test7x[] = {"then", "echo", "hi", NULL};
        createChild(test7x, 3, 1);

        // Clean up
        free(statusArray);
        return 0;
    }

    // Regular shell execution
    if (argc > 1)
    {
        batchMode(argv[1]);
    }
    else
    {
        interactiveMode();
    }

    // Clean up
    free(statusArray);
    return 0;
}
