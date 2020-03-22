#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#define DIRECTORY "."
#define DIRECTORY_LINE "./"
#define MAX_SIZE 1000         // Maximum address size (not efficient)
#define DIGITS_MAX 11         // Maximum number of digits
#define IGNORE_1 "."          // Used to ignore the current directory address
#define IGNORE_2 ".."         // Used to ignore the previous directory address
#define VERBOSE 0             // DEBUGGING
#define CUSTOM_INF 2147483647 // INFINITY (No need to call library math.h)

#define BLOCK_SIZE_STAT 512
#define BLOCK_SIZE_PRINT 1024

int num_threads;      // number of threads;
pid_t *threads;       // Array of pid of created threads
bool stopped = false; // True if stopped by SIGSTOP

pid_t pgid; //process id for the parent;

struct timeval *startTime; //Time of beggining of program

/* Global variables: Used the dictate the flags */

typedef struct
{
    int allFiles_C;     // -a or --all                          (C)
    int bytesDisplay_C; // -b, --bytes                          (C)
    int blockSize_C;    // -B or --block-size = SIZE            (C)
    int countLink_C;    // -l, --count-links                    // Always activated
    int dereference_C;  // -L, --dereference                    (C)
    int separatedirs_C; // -S, --separate-dirs                  (C)
    int maxDepth_C;     // --max-depth = N                      (C)
} cTags;

cTags tags = {0};

// Function prototypes:

long int seekdirec(char *currentdir, int depth);
int createProcess(char *currentdir, int depth);
void printTags();
long int sizeAttribution(struct stat *temp);
long int dereferenceLink(char *workTable, int depth);
void sigintHandler(int sig);
void initSigaction();
void initSigactionSIGIGN();
void addThread(pid_t pid);
void removeThread();
void emptyThreads();
void printInsantPid(pid_t *pid);
double timeSinceStartTime();
void printThreads();

// Function bodies:

/* Thread handling */
void printThreads()
{
    printf("\nList Threads:\n");
    if (num_threads = !0)
    {
        for (int n = 0; n < num_threads; n++)
        {
            printf("%d - %d\t", n, threads[n]);
        }
    }
    printf("\n");
}

void emptyThreads()
{
    // while (num_threads > 0)
    // {
    //     removeThread();
    // }
    num_threads = 0;
}

void removeThread()
{
    if (num_threads > 0)
    {
        threads[num_threads - 1] = 0;
        num_threads--;
    }
}

void addThread(pid_t pid)
{
    threads[num_threads] = pid;
    num_threads++;
    // printf("\nget thread -> %d\n", threads[num_threads - 1]);
}

/* Log prints*/
//Returns time in miliseconds since begging of program
double timeSinceStartTime()
{
    struct timeval instant;
    gettimeofday(&instant, 0);

    return (double)(instant.tv_sec - startTime->tv_sec) * 1000.0f + (instant.tv_usec - startTime->tv_usec) / 1000.0f;
}

void printInsantPid(pid_t *pid)
{
    printf("%0.2f - %d - ", timeSinceStartTime(), *pid);
}

void printActionInfoCREATE(pid_t *pid, int argc, char *argv[])
{
    printInsantPid(pid);
    printf("CREATE - ");
}

void printActionInfoEXIT(pid_t *pid, int argc, char *argv[])
{
    printInsantPid(pid);
    printf("EXIT - ");
}

void printActionInfoRECV_SIGNAL(pid_t *pid, char *signal)
{
    printInsantPid(pid);
    printf("RECV_SIGNAL - %s\n", signal);
}

void printActionInfoSEND_SIGNAL(pid_t *pid, char *signal, pid_t *destination)
{
    printInsantPid(pid);
    printf("SEND_SIGNAL - %s %d\n", signal, *destination);
}

void printActionInfoRECV_PIP(pid_t *pid, int argc, char *argv[])
{
    printInsantPid(pid);
    printf("RECV_PIP - ");
}

void printActionInfoSEND_PIPE(pid_t *pid, int argc, char *argv[])
{
    printInsantPid(pid);
    printf("SEND_PIPE - ");
}

void printActionInfoENTRY(pid_t *pid, int argc, char *argv[])
{
    printInsantPid(pid);
    printf("ENTRY - ");
}

/* Accesses the link */

int blockSize = 512;

long int dereferenceLink(char *workTable, int depth)
{
    struct stat status;
    long int sizeRep;

    char megaPath[MAX_SIZE];
    readlink(workTable, megaPath, MAX_SIZE - 1);
    if (!lstat(megaPath, &status))
    {
        sizeRep = sizeAttribution(&status);
        if (depth > 0)
        {
            //printf("[%d]\t", depth);
            printf("%ld\t%s\n", sizeRep, workTable);
        }
        return sizeRep;
    }
    else
    {
        perror("(dereferenceLink) Failure to access path : ");
        return -1;
    }
}

/* [Used mostly for debugging] - Prints tags */
void printTags()
{
    printf("Printing variables:\n");
    printf("tags.allFiles_C = %d\n", tags.allFiles_C);
    printf("tags.bytesDisplay_C = %d\n", tags.bytesDisplay_C);
    printf("tags.blockSize_C = %d\n", tags.blockSize_C);
    printf("tags.countLink_C = %d\n", tags.countLink_C);
    printf("tags.dereference_C = %d\n", tags.dereference_C);
    printf("tags.separatedirs_C = %d\n", tags.separatedirs_C);
    printf("tags.maxDepth_C = %d\n", tags.maxDepth_C);
}

/* Attributes sizes based on the activated tags. */
long int sizeAttribution(struct stat *temp)
{

    float value;

    if (tags.bytesDisplay_C)
    {
        value = (temp->st_size);
    }
    else
    {
        value = (temp->st_blocks * BLOCK_SIZE_STAT);
        value /= tags.blockSize_C;

        int out = value;
        if (value > out)
            value++;
    }
    return value;
}

/* Signals */
void sendSignalToAllThreads(int sig)
{
    pid_t pid = getpid();

    if (num_threads > 0)
    {
        for (int n = 0; n < num_threads; n++)
        {
            switch (sig)
            {
            case 2:
                printActionInfoSEND_SIGNAL(&pid, "SIGINT", &threads[n]);
                kill(threads[n], sig);
                break;

            case 15:
                printActionInfoSEND_SIGNAL(&pid, "SIGTERM", &threads[n]);
                kill(threads[n], sig);
                break;

            case 18:
                printActionInfoSEND_SIGNAL(&pid, "SIGCONT", &threads[n]);
                kill(threads[n], sig);
                break;

            case 19:
                printActionInfoSEND_SIGNAL(&pid, "SIGSTOP", &threads[n]);
                kill(threads[n], sig);
                break;

            default:
                break;
            }
        }
    }
}

void sigintHandlerChild(int sig)
{
    // printThreads();
    pid_t pid = getpid();
    printActionInfoRECV_SIGNAL(&pid, "SIGINT");

    if (sig == SIGINT)
    {
        if (!stopped)
        {
            sendSignalToAllThreads(SIGINT);
            stopped = true;
            sendSignalToAllThreads(SIGSTOP);
        }
        else
        {
            sendSignalToAllThreads(SIGINT);
            stopped = false;
            sendSignalToAllThreads(SIGCONT);
        }
    }
    else if (sig == SIGTERM)
    {
        sendSignalToAllThreads(SIGTERM);

        printActionInfoSEND_SIGNAL(&pid, "SIGTERM", &pid);
        kill(pid, SIGTERM);
    }
}

void sigintHandler(int sig)
{
    pid_t pid = getpid();
    // printThreads();

    printActionInfoRECV_SIGNAL(&pid, "SIGINT");

    // sendSignalToAllThreads(SIGINT);
    // sendSignalToAllThreads(SIGSTOP);
    kill(-pgid, SIGSTOP);

    int i = -1;
    while (i != 0 && i != 1)
    {
        printf("0 - Continue \n 1 - Terminate Program\n");
        scanf("%d", &i);
    }

    if (i == 0)
    {
        // sendSignalToAllThreads(SIGINT);
        // sendSignalToAllThreads(SIGCONT);
        kill(-pgid, SIGCONT);
    }
    else
    {
        // sendSignalToAllThreads(SIGTERM);

        // printActionInfoSEND_SIGNAL(&pid, "SIGTERM", &pid);
        // kill(pid, SIGTERM);
        kill(-pgid, SIGTERM);
        kill(pid, SIGTERM);
    }
}

void initSigactionSIGIGN()
{
    // prepare the 'sigaction struct'
    struct sigaction action;
    // action.sa_handler = sigintHandlerChild;
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    // install the handler
    sigaction(SIGINT, &action, NULL);
    // sigaction(SIGTERM, &action, NULL);
}

void initSigaction()
{
    // prepare the 'sigaction struct'
    struct sigaction action;
    action.sa_handler = sigintHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    // install the handler
    sigaction(SIGINT, &action, NULL);
}

/* Creates a process. */
int createProcess(char *currentdir, int depth)
{
    fflush(stdout);
    int n, fd[2];
    pid_t pid;
    char digitsre[DIGITS_MAX];
    memset(digitsre, '\0', DIGITS_MAX);

    if (pipe(fd) < 0)
    {
        fprintf(stderr, "pipe error\n");
        exit(1);
    }

    if ((pid = fork()) < 0)
    {
        fprintf(stderr, "fork error\n");
        exit(2);
    }
    else if (pid > 0)
    {                 /* pai */
        close(fd[1]); /* fecha lado emissor do pipe */

        setpgid(pid, -pgid);
        if (pgid == 0)
        {
            pgid = pid;
        }

        addThread(pid);
        wait(NULL);
        removeThread();

        read(fd[0], digitsre, DIGITS_MAX);
        n = atoi(digitsre);
        close(fd[0]); /* fecha lado receptor do pipe */
    }
    else
    { /* filho */
        // setpgrp();
        initSigactionSIGIGN();
        // emptyThreads();

        close(fd[0]); /* fecha lado receptor do pipe */
        depth--;
        n = seekdirec(currentdir, depth);
        sprintf(digitsre, "%d", n);
        write(fd[1], digitsre, strlen(digitsre));
        close(fd[1]); /* fecha lado emissor do pipe */

        exit(0);
    }

    return n;
}

void print(long int size, char *workTable)
{
    if (tags.bytesDisplay_C)
    {
        float value = size;
        value /= tags.blockSize_C;
        int out = value;
        if (value > out)
            value++;
        long int temp_out = value;
        printf("%ld\t%s\n", temp_out, workTable);
    }
    else
    {
        printf("%ld\t%s\n", size, workTable);
    }
}

/* Reads all files in a given directory and displays identically the way 'du' does */
long int seekdirec(char *currentdir, int depth)
{
    if (VERBOSE)
        printf("    [INFO] Received address '%s' ....... OK!\n", currentdir);

    char workTable[MAX_SIZE];
    memset(workTable, '\0', MAX_SIZE);
    strcpy(workTable, currentdir);

    DIR *d;
    struct dirent *dira;
    struct stat status;
    long int size = 0;

    // First step: Attribute base size of the directory
    if (!lstat(workTable, &status))
    {
        size += sizeAttribution(&status);
    }

    d = opendir(workTable);

    if (d == NULL)
    {
        perror("Error: ");
        return -1;
    }
    else
    {
        if (VERBOSE)
            printf("    [INFO] Directory '%s' was successfully loaded ....... OK!\n", workTable);

        while ((dira = readdir(d)) != NULL)
        {
            strcpy(workTable, currentdir);
            strcat(workTable, "/");
            if (strcmp(dira->d_name, IGNORE_1) != 0 && strcmp(dira->d_name, IGNORE_2) != 0)
            {
                if (!lstat(strcat(workTable, dira->d_name), &status))
                {
                    // [DEBUG] printf("Cycle loaded by process id: %d -- (%s)!\n", getpid(), workTable);
                    // If it is a directory:
                    if (S_ISDIR(status.st_mode))
                    {
                        if (VERBOSE)
                            printf("    [INFO] Directory '%s' is a directory ....... OK! (size: %ld)\n", workTable, status.st_size);
                        long int returned = 0;
                        returned = createProcess(workTable, depth);
                        if (!tags.separatedirs_C)
                            size += returned;
                    }
                    // If it is a link:
                    else if (S_ISLNK(status.st_mode))
                    {
                        if (VERBOSE)
                            printf("    [INFO] Directory '%s' is a symbolic link ....... OK!\n", workTable);
                        if (tags.dereference_C == 0)
                        {
                            // Only considers the file's own size
                            if (tags.allFiles_C)
                            {
                                if (depth > 0)
                                {
                                    //printf("[%d]\t", depth);
                                    print(sizeAttribution(&status), workTable);
                                }
                            }
                            size += sizeAttribution(&status);
                        }
                        else
                        {
                            // Considers the referenced file!
                            size += dereferenceLink(workTable, depth);
                        }
                    }
                    // If it is a regular file:
                    else if (S_ISREG(status.st_mode))
                    {
                        if (VERBOSE)
                            printf("    [INFO] Directory '%s' is a regular file ....... OK!\n", workTable);
                        size += sizeAttribution(&status);
                        if (tags.allFiles_C)
                        {
                            if (depth > 0)
                            {
                                //printf("[%d]\t", depth);
                                print(sizeAttribution(&status), workTable);
                            }
                        }
                    }
                }
                else
                {
                    if (VERBOSE)
                        printf("    [FAILURE REPORT] Directory '%s' returned some error ....... ERROR!\n", workTable);
                }
            }
        }
    }
    closedir(d);
    strcpy(workTable, currentdir);
    if (depth >= 0)
    {
        if (tags.bytesDisplay_C)
        {
            float value = size;
            value /= tags.blockSize_C;
            int out = value;
            if (value > out)
                value++;
            long int temp_out = value;
            printf("%ld\t%s\n", temp_out, workTable);
        }
        else
        {
            printf("%ld\t%s\n", size, workTable);
        }
    }
    sleep(1);

    return size;
}

int main(int argc, char *argv[])
{
    startTime = malloc(sizeof(struct timeval));
    gettimeofday(startTime, 0);

    char directoryLine[MAX_SIZE] = DIRECTORY;
    tags.maxDepth_C = CUSTOM_INF;
    tags.countLink_C = 1;
    tags.blockSize_C = BLOCK_SIZE_STAT;

    threads = (pid_t *)malloc(sizeof(pid_t) * 1000);
    num_threads = 0;

    pgid = 0;

    initSigaction();

    // initSigactionSIGIGN();

    // Set up flags
    for (int i = 1; i < argc; i++)
    {
        // allFiles_C (-a or --all)
        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0)
        {
            tags.allFiles_C = 1;
        }
        else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bytes") == 0)
        {
            tags.blockSize_C = 1;
            tags.bytesDisplay_C = 1;
        }
        else if (strcmp(argv[i], "-B") == 0 || strncmp(argv[i], "--block-size=", 13) == 0 || (strstr(argv[i], "-B")) != NULL || (strstr(argv[i], "--block-size=")) != NULL)
        {
            if (strcmp(argv[i], "-B") == 0)
            {
                i++;
                tags.blockSize_C = atoi(argv[i]);
            }
            if (strncmp(argv[i], "--block-size=", 13) == 0)
            {
                tags.blockSize_C = atoi(argv[i] + 13 * sizeof(char));
            }
            char *temp;
            if ((temp = strstr(argv[i], "-B")) != NULL)
            {
                temp += 2;
                tags.blockSize_C = atoi(temp);
            }
            if ((temp = strstr(argv[i], "--block-size=")) != NULL)
            {
                temp += 13;
                tags.blockSize_C = atoi(temp);
            }
        }
        else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--count-links") == 0)
        {
            // Do thing
        }
        else if (strcmp(argv[i], "-L") == 0 || strncmp(argv[i], "--dereference", 13) == 0)
        {
            tags.dereference_C = 1;
        }
        else if (strcmp(argv[i], "-S") == 0 || strncmp(argv[i], "--separate-dirs", 13) == 0)
        {
            tags.separatedirs_C = 1;
        }
        else if (strncmp(argv[i], "--max-depth=", 12) == 0)
        {
            tags.maxDepth_C = atoi(argv[i] + 12 * sizeof(char));
        }
        else
        {
            strcpy(directoryLine, argv[i]);
        }
    }

    //printTags();
    seekdirec(directoryLine, tags.maxDepth_C);

    return 0;
}
