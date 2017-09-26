#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/wait.h> 
#include <signal.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

int currentPID = 0;
struct node *head_job = NULL;
struct node *current_job = NULL;



struct node {
    int number; // the job number
    int pid; // the process id of the a specific process
    struct node *next; // when another process is called you add to the end of the linked list
};

/* Add a job to the list of jobs
 */
void addToJobList(pid_t process_pid) {

    struct node *job = malloc(sizeof(struct node));

    //If the job list is empty, create a new head
    if (head_job == NULL) {
        job->number = 1;
        job->pid = process_pid;

        //the new head is also the current node
        job->next = NULL;
        head_job = job;
        current_job = head_job;
    }

    //Otherwise create a new job node and link the current node to it
    else {

        job->number = current_job->number + 1;
        job->pid = process_pid;

        current_job->next = job;
        current_job = job;
        job->next = NULL;
    }
}

void sighandler(int sig) {
    if (currentPID == 0) {
        printf("No processes currently running.");
    }
    else {
        kill(currentPID, SIGKILL); 
    }
}

char *getcmd(char *prompt, char *args[], int *background, int *redirect)
{
	int i = 0;
    int length = 0;
    char *token;
    char *line = malloc(1500);
    char * returnString = "";
    size_t linecap = 0;
    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);
    if (line[0] == '\n'){
        return NULL;
    }
    while ((token = strsep(&line, " \t\n")) != NULL) {
        if (strlen(token) > 0) {
            if (strcmp(token, "&") == 0) {
                *background = 1;
            }
            else if (strcmp(token, ">") == 0) {
                *redirect = 1;
            }
            else {
                token[strlen(token)] = '\0';
                args[i] = token;
                i++;      
            }
        }
    }
    if (*redirect == 1) {
        returnString = args[i-1];
        args[i-1] = NULL;
    }
    args[i] = NULL;
    return returnString;
}

void printJobs() {
   struct node *ptr = head_job;
    
   //start from the beginning
   while(ptr != NULL) {
      printf("(Job: %d, PID: %d) ",ptr->number,ptr->pid);
      ptr = ptr->next;
   }
}

void updateJobs() {
    struct node *ptr = head_job;
    struct node *previous = NULL;
    while(ptr != NULL) {
        int status;
        pid_t pid = waitpid(ptr -> pid, &status, WNOHANG);
        int didStop = WIFEXITED(status);
        if (didStop == 1) {
            if (previous == NULL) {
                head_job = NULL;
            }
            else {
                previous -> next = ptr -> next;
                ptr = ptr -> next;
            }
        }
        previous = ptr;
        ptr = ptr -> next;
    }
}

int main(void) {
    char *args[20];
    int bg;
    int redirect;
    struct node *head_job = NULL;
    struct node *current_job = NULL;
    while(1) {
        bg = 0;
        redirect = 0;
        currentPID = 0;
        pid_t pid;
        int status;
        int fd[2];

        updateJobs();

        if (pipe(fd) == -1) {
            perror("pipe");
            exit(1);
        }
        if (signal(SIGINT, sighandler) == SIG_ERR) {
            printf("*** ERROR performing signal");
        }
        else if(signal(SIGTSTP, sighandler) == SIG_ERR){
            printf("*** ERROR performing signal");
        }
        char *returnString = getcmd("\n >>", args, &bg, &redirect);

        if(returnString == NULL){
            continue;
        }
        if ((pid = fork()) < 0){
            printf("*** ERROR: forking child process failed");
            exit(1);
        } 
        else if (pid == 0) {
            if (redirect == 1) {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
            }
            else if(strcmp(args[0], "jobs") == 0){
                exit(0);
            }
            sleep(2);
            if(strcmp(args[0], "cd") == 0) {
                exit(0);
            }
            if(strcmp(args[0], "pwd") == 0) {
                char cwd[800];
                if (getcwd(cwd, sizeof(cwd)) != NULL) {
                    printf("%s", cwd);
                    exit(0);
                }
                else {
                    printf("*** ERROR: Cannot retrieve pwd");
                    exit(1);
                }
            }
            else if(strcmp(args[0], "exit") == 0) {
                 exit(0);
             }
            else if (execvp(*args, args) < 0) {
                printf("*** ERROR: exec of command failed");
                exit(1);
            }
            close(fd[1]);
        }
        else {
            if (strcmp(args[0], "cd") == 0) {
                 chdir(args[1]);
            }
            else if (strcmp(args[0], "jobs") == 0) {
                printJobs();
            }
            if (redirect == 1) {
                char linenum[1000];
                waitpid(pid, &status, 0);
                read(fd[0], linenum, 999);
                FILE *f = fopen(returnString, "w");
                if (f == NULL) {
                    printf("Error opening file! \n");
                    exit(1);
                }
                fprintf(f, "%s", linenum);
                fclose(f);
            }
            else if(bg == 0){
                currentPID = pid;
                if(strcmp(args[0], "exit") == 0) {
                    exit(0);
                }
                waitpid(pid, &status, 0);
            }
            else if(bg == 1) {
                addToJobList(pid);
            }
            updateJobs();
        }
    }
}
