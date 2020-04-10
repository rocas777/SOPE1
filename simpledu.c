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
#include <errno.h>

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

#define MAX_SIZE_ARR 1000        // size of fd_arr and pgid

pid_t *pgid; //process id array for the parent of child's process group.
int num_pgid; //number of process groups

int **fd_arr; //pipe array
int num_fd; //number of pipes

struct timeval *startTime; //Time of beginning of program
struct timeval secc;

FILE *log_filename;
int8_t lf_exists=0; //File

/* Global variables: Used the dictate the flags */

typedef struct {
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

long long int seekdirec(char *currentdir, int depth);

void createProcess(char *currentdir, int depth, int fd[2]);

void read_fd_arr(pid_t *pid_p, long long int *size);

void read_info_pipe(pid_t *pid_p, long long int *returned);

void send_info_pipe(pid_t *pid_p, int fd_1, long long int *size);

int wait_child(pid_t *pid_p);

void close_fd_0(int fd);

void close_fd_1(int fd);

void delete_fd(pid_t *pid_p, int *fd);

void printTags();

void print(long long int size, char *workTable);

long long int sizeAttribution(struct stat *temp);

long long int dereferenceLink(char *workTable, int depth);

void sigintHandler(int sig);

void initSigaction();

void initSigactionSIG_IGN();

void init_fd_arr();

void init_log_file(char* envp[]);

int init_flags(int argc, char *argv[], char directoryLine[MAX_SIZE]);

int init(int argc, char *argv[], char directoryLine[MAX_SIZE],char* envp[]);

void init_child(pid_t *pid_p);


double timeSinceStartTime();

void printInstantPid(pid_t *pid, char *temp);

void printActionInfoCREATE(pid_t *pid, int argc, char *argv[]);

void printActionInfoEXIT(pid_t *pid, int exit);

void printActionInfoRECV_SIGNAL(pid_t *pid, char *signal);

void printActionInfoSEND_SIGNAL(pid_t *pid, char *signal, pid_t destination);

void printActionInfoRECV_PIP(pid_t *pid, char *message);

void printActionInfoSEND_PIPE(pid_t *pid, char *message);

void printActionInfoENTRY(pid_t *pid, long long int bytes, char path[]);


int argcG;
char **argvG;

// Function bodies:

/* Log prints*/
double timeSinceStartTime() //Returns time in milliseconds since begging of program
{
    struct timeval instant;
    gettimeofday(&instant, 0);

    *startTime = secc;

    return (double) (instant.tv_sec - startTime->tv_sec) * 1000.0f + (instant.tv_usec - startTime->tv_usec) / 1000.0f;
}

void printInstantPid(pid_t *pid, char *temp) {
    sprintf(temp, "%0.2f - %d - ", timeSinceStartTime(), *pid);
}

void printActionInfoCREATE(pid_t *pid, int argc, char *argv[]) {
    char temp[10000] = "";
    printInstantPid(pid, temp);
    strcat(temp, "CREATE - ");
    int i = 1;
    if (argc > 1) {
        for (; i < argc - 1; i++) {
            strcat(temp, argv[i]);
        }
        strcat(temp, argv[i]);
    }
    fprintf(log_filename, "%s\n", temp);
    fflush(log_filename);
}

void printActionInfoEXIT(pid_t *pid, int exit) {
    char temp[10000] = "";
    printInstantPid(pid, temp);
    fprintf(log_filename, "%sEXIT - %i\n", temp, exit);
    fflush(log_filename);
}

void printActionInfoRECV_SIGNAL(pid_t *pid, char *signal) {
    char temp[10000] = "";
    printInstantPid(pid, temp);
    fprintf(log_filename, "%sRECV_SIGNAL - %s\n", temp, signal);
    fflush(log_filename);
}

void printActionInfoSEND_SIGNAL(pid_t *pid, char *signal, pid_t destination) {
    char temp[10000] = "";
    printInstantPid(pid, temp);
    fprintf(log_filename, "%sSEND_SIGNAL - %s \"%d\"\n", temp, signal, destination);
    fflush(log_filename);
}

void printActionInfoRECV_PIP(pid_t *pid, char *message) {
    char temp[10000] = "";
    printInstantPid(pid, temp);
    fprintf(log_filename, "%sRECV_PIP - %s\n", temp, message);
    fflush(log_filename);
}

void printActionInfoSEND_PIPE(pid_t *pid, char *message) {
    char temp[10000] = "";
    printInstantPid(pid, temp);
    fprintf(log_filename, "%sSEND_PIPE - %s\n", temp, message);
    fflush(log_filename);
}

void printActionInfoENTRY(pid_t *pid, long long int bytes, char path[]) {
    char temp[10000] = "";
    printInstantPid(pid, temp);
    fprintf(log_filename, "%sENTRY - %lld %s\n", temp, bytes, path);
    fflush(log_filename);
}

/* Accesses the link */

long long int dereferenceLink(char *workTable, int depth) {
    struct stat status;
    long long int sizeRep;

    char megaPath[MAX_SIZE];
    readlink(workTable, megaPath, MAX_SIZE - 1);
    if (!lstat(megaPath, &status)) {
        sizeRep = sizeAttribution(&status);
        return sizeRep;
    } else {
        perror("(dereferenceLink) Failure to access path : ");
        return 0;
    }
}

/* [Used mostly for debugging] - Prints tags */
void printTags() {
    printf("Printing variables:\n");
    printf("tags.allFiles_C = %d\n", tags.allFiles_C);
    printf("tags.bytesDisplay_C = %d\n", tags.bytesDisplay_C);
    printf("tags.blockSize_C = %d\n", tags.blockSize_C);
    printf("tags.countLink_C = %d\n", tags.countLink_C);
    printf("tags.dereference_C = %d\n", tags.dereference_C);
    printf("tags.separatedirs_C = %d\n", tags.separatedirs_C);
    printf("tags.maxDepth_C = %d\n", tags.maxDepth_C);
}

/* Signals */
void sigintHandler(int sig) {
    pid_t pid = getpid();

    if(lf_exists)
    	printActionInfoRECV_SIGNAL(&pid, "SIGINT");

    if (num_pgid != 0) {
        for (int n = 0; n < num_pgid; n++) {
	    if(lf_exists)
            	printActionInfoSEND_SIGNAL(&pid, "SIGSTOP", -pgid[n]);
            kill(-pgid[n], SIGSTOP);
        }
    }

    char i[2];
    while (i[0] != '0' && i[0] != '1') {
        printf("0 - Continue \n1 - Terminate Program\n\n");
	fflush(stdout);
        read(0, i, 2);
    }

    if (i[0] == '0') {
        if (num_pgid != 0) {
            for (int n = 0; n < num_pgid; n++) {
		if(lf_exists)
                	printActionInfoSEND_SIGNAL(&pid, "SIGCONT", -pgid[n]);
                kill(-pgid[n], SIGCONT);
            }
        }
    } else {
        if (num_pgid != 0) {
            for (int n = 0; n < num_pgid; n++) {
		if(lf_exists)
                	printActionInfoSEND_SIGNAL(&pid, "SIGTERM", -pgid[n]);
                kill(-pgid[n], SIGTERM);
            }
        }
	if(lf_exists)
        	printActionInfoSEND_SIGNAL(&pid, "SIGTERM", pid);
        kill(pid, SIGTERM);
    }
}

void initSigactionSIG_IGN() {
    // prepare the 'sigaction struct'
    struct sigaction action;
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    // install the handler
    sigaction(SIGINT, &action, NULL);
}

void initSigaction() {
    // prepare the 'sigaction struct'
    struct sigaction action;
    action.sa_handler = sigintHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    // install the handler
    sigaction(SIGINT, &action, NULL);
}

void close_fd_0(int fd) {
    if (close(fd) < 0) { /* fecha lado receptor do pipe */
        perror("Failed to close read end of pipe");
    }
}

void close_fd_1(int fd) {
    if (close(fd) < 0) { /* fecha lado emissor do pipe */
        perror("Failed to close write end of pipe");
    }
}

void delete_fd(pid_t *pid_p, int *fd) {
    fflush(stdout);
    if (pgid > 0) {
        int i;
        for (i = 0; i < num_pgid; i++) {
            if (pgid[i] == *pid_p) {
                break;
            }
        }

        if (i < num_pgid) {
            num_pgid--;
//            num_fd--;
            for (int j = i; j < num_pgid; j++) {
                pgid[j] = pgid[j + 1];
            }
        }
    }
    if (num_fd > 0) {
        int i;
        for (i = 0; i < num_fd; i++) {
            if (fd_arr[i] == fd) {
                break;
            }
        }

        if (i < num_fd) {
//            num_fd--;
            for (int j = i; j < num_pgid; j++) {
                fd_arr[j] = fd_arr[j + 1];
            }
        }
    }
}

int wait_child(pid_t *pid_p) {
    int status = -1;
    int pid = 0;
    int soma=0;
    while (status != 0 && soma < 100) {
        pid = wait(&status);
	if(pid!=-1){
		if(lf_exists)
        		printActionInfoEXIT(&pid, status);
	}
	else
		soma++;
    }
    return pid;
}

void read_info_pipe(pid_t *pid_p, long long int *returned) {
    char digitsre[DIGITS_MAX];
    memset(digitsre, '\0', DIGITS_MAX);

    read(fd_arr[num_fd][0], digitsre, DIGITS_MAX);
    if(lf_exists)
    	printActionInfoRECV_PIP(pid_p, digitsre);
    *returned = atoll(digitsre);
}

void read_fd_arr(pid_t *pid_p, long long int *size) {
    while (num_fd > 0) {
        num_fd--;

        int pid = wait_child(pid_p);

        long long int returned = 0;

        read_info_pipe(pid_p, &returned);

        close_fd_0(fd_arr[num_fd][0]);
        delete_fd(&pid, fd_arr[num_fd]);

        if (!tags.separatedirs_C) {
            *size += returned;
        }
    }
}

void send_info_pipe(pid_t *pid_p, int fd_1, long long int *size) {
    char digitsre[DIGITS_MAX];
    memset(digitsre, '\0', DIGITS_MAX);

    sprintf(digitsre, "%lld", *size);
    write(fd_1, digitsre, strlen(digitsre));
    if(lf_exists)
    	printActionInfoSEND_PIPE(pid_p, digitsre);
}

void init_child(pid_t *pid_p) {
    init_fd_arr();

    num_fd = 0;
    initSigactionSIG_IGN();
    if (num_pgid >= 0) {
        setpgrp(); // cria novo grupo de processos para o filho e filhos deste
    }
    num_pgid = -1;

    if(lf_exists)
    	printActionInfoCREATE(pid_p, argcG, argvG);
}

/* Creates a process. */
void createProcess(char *currentdir, int depth, int fd[2]) {
    fflush(stdout);
    pid_t pid;

    if ((pid = fork()) < 0) {
        fprintf(stderr, "fork error\n");
        exit(2);
    } else if (pid > 0) { /* pai */
        close_fd_1(fd[1]);
        if (num_pgid != -1) { // se não existir pai (thread principal)
            pgid[num_pgid] = pid;
            num_pgid++;
        }
    } else { /* filho */
        pid_t pid_p = getpid();

        close_fd_0(fd[0]);

        init_child(&pid_p);

        long long int size;
        depth--;
        size = seekdirec(currentdir, depth);
        send_info_pipe(&pid_p, fd[1], &size);

        close_fd_1(fd[1]);

        exit(0);
    }
}

void print(long long int size, char *workTable) {
    long long int out = size;
    size /= tags.blockSize_C;
    if (out % tags.blockSize_C)
        size++;

    printf("%lld\t%s\n", size, workTable);
    fflush(stdout);
}

/* Attributes sizes based on the activated tags. */
long long int sizeAttribution(struct stat *temp) {

    long long int value;

    if (tags.bytesDisplay_C)
        value = temp->st_size;
    else {
        value = temp->st_blocks * BLOCK_SIZE_STAT;
    }
    return value;
}

char getLastItem(char *string) {
    char last;
    for (unsigned i = 0;; i++) {
        if ((*string) == 0)
            break;
        last = (*string);
        string++;
    }
    return last;
}

/* Reads all files in a given directory and displays identically the way 'du' does */
long long int seekdirec(char *currentdir, int depth) {
    pid_t pid_p = getpid();

    if (VERBOSE)
        printf("    [INFO] Received address '%s' ....... OK!\n", currentdir);

    char workTable[MAX_SIZE];
    memset(workTable, '\0', MAX_SIZE);
    strcpy(workTable, currentdir);

    DIR *d;
    struct dirent *dira;
    struct stat status;
    long long int size = 0;

    // First step: Attribute base size of the directory
    if (!stat(workTable, &status)) {
        size += sizeAttribution(&status);
    }
    d = opendir(workTable);

    if (d == NULL) {
        perror("Error: ");
        return -1;
    } else {
        if (VERBOSE)
            printf("    [INFO] Directory '%s' was successfully loaded ....... OK!\n", workTable);

        while ((dira = readdir(d)) != NULL) {

            strcpy(workTable, currentdir);
            if (getLastItem(currentdir) != '/')
                strcat(workTable, "/");
            if (strcmp(dira->d_name, IGNORE_1) != 0 && strcmp(dira->d_name, IGNORE_2) != 0) {
                if (!lstat(strcat(workTable, dira->d_name), &status)) {
                    //printf("modo: %i\n",status.st_mode);
                    //printf("modo: %li\n",status.st_size);
                    // [DEBUG] printf("Cycle loaded by process id: %d -- (%s)!\n", getpid(), workTable);
                    // If it is a directory:
                    if (S_ISDIR(status.st_mode)) {
                        if (VERBOSE)
                            printf("    [INFO] Directory '%s' is a directory ....... OK! (size: %ld)\n", workTable,
                                   status.st_size);

                        if (pipe(fd_arr[num_fd]) < 0) {
                            fprintf(stderr, "pipe error\n");
                            exit(1);
                        }
                        int *fd;
                        fd = fd_arr[num_fd];

                        createProcess(workTable, depth, fd);
                        num_fd++;
                        if (num_fd > 10) {
                            read_fd_arr(&pid_p, &size);
                        }
                    }
                        // If it is a link:
                    else if (S_ISLNK(status.st_mode)) {
                        if (VERBOSE)
                            printf("    [INFO] Directory '%s' is a symbolic link ....... OK!\n", workTable);
                        if (tags.dereference_C == 0) {
                            // Only considers the file's own size
                            if (depth > 0) {
                                long long int temporary = sizeAttribution(&status);
                                if (tags.allFiles_C) {
                                    //printf("[%d]\t", depth);
                                    print(temporary, workTable);
                                }
				if(lf_exists)
                                printActionInfoENTRY(&pid_p, temporary, workTable);
                            }
                            size += sizeAttribution(&status);
                        } else {
                            // Considers the referenced file!
                            //size += dereferenceLink(workTable, depth);
			    stat(workTable, &status);
			    if(S_ISDIR(status.st_mode)){
        			//size += sizeAttribution(&status);
			    	if (pipe(fd_arr[num_fd]) < 0) {
                          	  	fprintf(stderr, "pipe error\n");
                          	  	exit(1);
                          	  }
			 	   int *fd;
			  	  fd = fd_arr[num_fd];

			  	  createProcess(workTable, depth, fd);
				  //fprintf(stderr,"%s\n",workTable);
			  	  num_fd++;
			  	  if (num_fd > 10) {
					read_fd_arr(&pid_p, &size);
			   	 }	
			    }else if (S_ISREG(status.st_mode)) {
                       		if (VERBOSE)
                            		printf("    [INFO] Directory '%s' is a regular file ....... OK!\n", workTable);
                        	size += sizeAttribution(&status);
                        	if (depth > 0) {
                            		long long int temporary = sizeAttribution(&status);
                            		if (tags.allFiles_C) {
                                		print(temporary, workTable);
                            		}
					if(lf_exists)
                            		printActionInfoENTRY(&pid_p, temporary, workTable);
                        	}
                    	    }
			    
			    		
                        }
                    }
                        // If it is a regular file:
                    else if (S_ISREG(status.st_mode)) {
                        if (VERBOSE)
                            printf("    [INFO] Directory '%s' is a regular file ....... OK!\n", workTable);
                        size += sizeAttribution(&status);
                        if (depth > 0) {
                            long long int temporary = sizeAttribution(&status);
                            if (tags.allFiles_C) {
                                //printf("[%d]\t", depth);
                                //printf("acom: %lli\n",size);
                                //printf("file: %li\n",status.st_size);
                                print(temporary, workTable);
                            }
			    if(lf_exists)
                            printActionInfoENTRY(&pid_p, temporary, workTable);
                        }
                    } else {
                        if (VERBOSE)
                            printf("    [INFO] Directory '%s' is a regular file ....... OK!\n", workTable);
                        size += sizeAttribution(&status);
                        if (depth > 0) {
                            long long int temporary = sizeAttribution(&status);
                            if (tags.allFiles_C) {
                                print(temporary, workTable);
                            }
			    if(lf_exists)
                            printActionInfoENTRY(&pid_p, temporary, workTable);
                        }
                    }
                } else {
                    if (VERBOSE)
                        printf("    [FAILURE REPORT] Directory '%s' returned some error ....... ERROR!\n", workTable);
                }
            }
        }
    }

    read_fd_arr(&pid_p, &size);

    closedir(d);
    strcpy(workTable, currentdir);

    if (depth >= 0) {
        print(size, workTable);
	if(lf_exists)
        printActionInfoENTRY(&pid_p, size, workTable);
    }

    return size;
}

void init_fd_arr() {
    fd_arr = (int **) malloc(sizeof(int *) * MAX_SIZE_ARR + sizeof(int) * 2 * MAX_SIZE_ARR);
    int *ptr = (int *) (fd_arr + MAX_SIZE_ARR);
    for (int i = 0; i < MAX_SIZE_ARR; i++) {
        fd_arr[i] = ptr + i * 2;
    }
    num_fd = 0;
}

void init_log_file(char* envp[]) {
    char *beg;
    char *logfilename=malloc(sizeof(char)*1000);

    for (int i = 0; envp[i] != NULL; i++){
    	if((beg=strstr(envp[i], "LOG_FILENAME")) != NULL) {
		//sprintf(logfilename,"%s",envp[i]);
		logfilename = getenv("LOG_FILENAME");	
		lf_exists=1;
		break;		
	}
    }
    if(lf_exists==1){
    	if ((log_filename = fopen(logfilename, "w")) == NULL) {
    	    int errsv = errno;
    	    printf("%d\n", errsv);
    	    fflush(stdout);
    	    perror(strerror(errsv));
    	    exit(1);
    	}
    }
}

int init_flags(int argc, char *argv[], char directoryLine[MAX_SIZE]) {
    tags.maxDepth_C = CUSTOM_INF;
    tags.countLink_C = 1;
    tags.blockSize_C = 1024;

    argvG = argv;
    argcG = argc;

    // Set up flags
    for (int i = 1; i < argc; i++) {
        // allFiles_C (-a or --all)
        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) {
            tags.allFiles_C = 1;
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bytes") == 0) {
            tags.blockSize_C = 1;
            tags.bytesDisplay_C = 1;
        } else if (strcmp(argv[i], "-B") == 0 || strncmp(argv[i], "--block-size=", 13) == 0 ||
                   (strstr(argv[i], "-B")) != NULL || (strstr(argv[i], "--block-size=")) != NULL) {
            if (strcmp(argv[i], "-B") == 0) {
                i++;
                tags.blockSize_C = atoi(argv[i]);
            }
            if (strncmp(argv[i], "--block-size=", 13) == 0) {
                tags.blockSize_C = atoi(argv[i] + 13 * sizeof(char));
            }

            char *temp;
            if ((temp = strstr(argv[i], "-B")) != NULL) {
                temp += 2;
                tags.blockSize_C = atoi(temp);
            }
            if ((temp = strstr(argv[i], "--block-size=")) != NULL) {
                temp += 13;
                tags.blockSize_C = atoi(temp);
            }
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--count-links") == 0) {
        } else if (strcmp(argv[i], "-L") == 0 || strncmp(argv[i], "--dereference", 13) == 0) {
            tags.dereference_C = 1;
        } else if (strcmp(argv[i], "-S") == 0 || strncmp(argv[i], "--separate-dirs", 13) == 0) {
            tags.separatedirs_C = 1;
        } else if (strncmp(argv[i], "--max-depth=", 12) == 0) {
            tags.maxDepth_C = atoi(argv[i] + 12 * sizeof(char));
        } else {
            strcpy(directoryLine, argv[i]);
        }
    }
    
	struct stat sb;
	if (!(stat(directoryLine, &sb) == 0 && S_ISDIR(sb.st_mode))){
		printf("%s\n",directoryLine);
		fflush(stdout);
		return 1;
	}
	else
		return 0;
}

int init(int argc, char *argv[], char directoryLine[MAX_SIZE],char* envp[]) {
    startTime = malloc(sizeof(struct timeval));
    gettimeofday(startTime, 0);

    secc = *startTime;
    pgid = (int *) malloc(sizeof(int) * MAX_SIZE_ARR);
    num_pgid = 0;

    init_fd_arr();

    initSigaction();

    init_log_file(envp);

    if(init_flags(argc, argv, directoryLine))
	return 1;
    return 0;
}

int main(int argc, char *argv[],char * envp[]) {

    char directoryLine[MAX_SIZE] = DIRECTORY;

    if(init(argc, argv, directoryLine,envp)){
	printf("Flags ou Diretorio de pesquisa não existentes\n");
	printf("Uso: simpledu -l [path] [-a] [-b] [-B size] [-L] [-S] [--max-depth=N]\n");
	exit(1);
    }

    seekdirec(directoryLine, tags.maxDepth_C);

    if(lf_exists)
    	fclose(log_filename);

    return 0;
}

