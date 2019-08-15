/**
 * CS 240 Shell Spells
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "command.h"
#include "joblist.h"
#include "terminal.h"

/** Shell name: replace with something more creative. */
static char const * const NAME = "shell";

/** Command prompt: feel free to change. */
static char const * const PROMPT = "-> ";

/** Path to file to use for command history. */
static char const * const HIST_FILE =  ".shell_history";

/**
 * Exit (terminate) the shell.
 *
 * Arguments:
 * status - exit status code (0 for OK, nonzero for error)
 */
void shell_builtin_exit(int status) {
  // Save command history.
  write_history(HIST_FILE);
  
  // Exit this process with the given status.
  exit(status);
}

/**
 * Reaps completed job by updating its status and then deleting it
 *
 * Arguments:
 * Jobs - job list
 * job - individual job
 *
 */
void reap(JobList* jobs, Job* job) {  
  pid_t job_pid = job->pid; 
  assert(job_pid > 0);

  //if job has completed update status and delete
  if (job_pid == waitpid(job_pid, NULL, WNOHANG)) {   
    job_set_status(jobs, job, JOB_STATUS_DONE); 
    job_delete(jobs,job); 
  }
}



/**
 * Reaps completed job by updating its status, printing its status
 * and then deleting it
 *
 * Arguments:
 * Jobs - job list
 * job - individual job
 *
 */
void reapprint(JobList* jobs, Job* job) {
  pid_t job_pid = job->pid; 
  assert(job_pid > 0);
  //if job has completed update status, print, and delete
  if (job_pid == waitpid(job_pid, NULL, WNOHANG)) {   
    job_set_status(jobs, job, JOB_STATUS_DONE);    
    job_print(jobs, job); 
    job_delete(jobs,job);
  }
}


/**
 * defers exit while unifinisehd jobs still exist
 *
 * Arguments:
 * Jobs - job list
 * ctrlD - tracks status of ctrl_D
 *
 * Returns:
 * 1 if failed to exit
 * 0 if successfully exited
 */
int politeExit(JobList* jobs, int* ctrlD) {
  job_iter(jobs, reap);
  //if unifhised jobs still exist
  if (!joblist_empty(jobs)) { 
    printf("\nThere are unfinished jobs.\n");
    job_iter(jobs, job_print); 
    //updates ctrlD if typed by user
    if (ctrlD){
      *ctrlD = 1;
    }   
    return 1; 
  }
  //otherwise, free joblist and exit
  joblist_free(jobs); 
  exit(0);
}

/**
 * If command is a builtin feature of the shell, do it directly.
 *
 * Arguments:
 * jobs    - job list (used only in Part 2+)
 * command - command to run
 *
 * Return:
 * 1 if the command is built in
 * 0 if the command is not built in
 */

                                       
int shell_run_builtin(JobList* jobs, char** command) {
  
  //exit command
  if (!strncmp(command[0], "exit", 5) && command[1] == NULL) {
    politeExit(jobs, NULL);
    return 1;
  } 

  else {

    //help command 
    if (!strncmp(command[0],"help", 5) && command[1] == NULL ){
      printf("-exit exits the shell process.\n");
      printf("-help displays a message listing the usage of all built-in commands that your shell supports.\n");
      printf("-cd changes the working directory of the shell\n  -If a path is given as an argument, then cd uses the directory described by that path: cd cs240\n");
      printf("  -If no argument is given, then cd uses the directory path given by the HOME environment variable.\n");
      return 1;
    }
    
    //cd command
    if (!strncmp(command[0], "cd",3)){ 
      //if directory not given
      if (command[1] == NULL ) { 
        //change to directory given by "HOME"
        chdir(getenv("HOME"));
      }
      //if directory given
      else {
       chdir(command[1]);
      }
      return 1;
    }

    //job command
    if (!strncmp(command[0], "job", 4)) {
      //print jobs
      job_iter(jobs, job_print);
      return 1;
    }

    //fg or bg command
    else if (!strncmp(command[0], "fg", 3) || !strncmp(command[0], "bg", 3)) {
      Job* job;
      int foreground = strncmp(command[0], "fg", 3)== 0;
      
      //if job not given
      if (command[1] == NULL) {  
        //get current job from joblist
        job = job_get_current(jobs);    
      }

      //if job given
      else{ 
        //use given job
        job = job_get(jobs, atoi(command[1]));
      } 

      //check for existence of job
      if (job) { 
        //foreground
        if (foreground) { 
          //resume foreground job if stopped
          if (job->status == JOB_STATUS_STOPPED) {
            kill(-1*(job->pid), SIGCONT);
          }
          //update status
          job_set_status(jobs, job, JOB_STATUS_FOREGROUND);
        }
        //background
        else{
          //resume background job if stopped
          if (job->status == JOB_STATUS_STOPPED) {
            kill(-1*(job->pid), SIGCONT);
          }
          //update status and print
          job_set_status(jobs, job, JOB_STATUS_BACKGROUND);
          job_print(jobs, job);
        }
      }
      //if no job exists
      else {
        printf("invalid job id/no background job exists.\n");
      }
      return 1;
    }
    //if not builtin command
    else {
    
      return 0;     
    }
  } 
}





/**
 * Fork and exec the requested executable command in the foreground (Part 1)
 * or background (Part 2).
 *
 * Exit the shell in error if an error occurs while forking (Part 1).
 *
 * Arguments:
 * jobs       - job list for saving or deleting jobs (Part 2+)
 * command    - command array of executable and arguments
 * foreground - indicates foregounrd (1) vs. background (0)
 */
void shell_run_executable(JobList* jobs, char** command, int foreground) {//

  pid_t pid = fork();

  //error checking for forking
  if (pid < 0) {
    perror("fork");
    shell_builtin_exit(0);
    
  }

  //foreground
  if (foreground) { 
    
    //if child
    if (pid == 0) {
      //set up child process’s signal handlers and terminal interaction
      term_child_init(jobs, foreground);

      //execute command
      int result= execvp(command[0], &command[0]); 

      //check for exec error
      if (result == -1){
        perror("execv");
        job_delete(jobs, job_get_current(jobs));
        return;
      }
    }

    //if parent
    if (pid >  0 )  {

      int childstatus;

      //save job
      Job* job = job_save(jobs, pid, command, JOB_STATUS_FOREGROUND); 

      //passes foreground status in the terminal from the shell to the new child process
      term_give(jobs, job);

      int wait = waitpid(pid, &childstatus, WUNTRACED);

      //reclaims foreground status for the shell from the previous foreground process.
      term_take(jobs, job);

      //check for wait error
      if(wait == -1) { 
        perror("wait");
        exit(1);
      }


      else if(wait == pid) { 
        //abort job, ctrl-C
        if (WIFSIGNALED(childstatus)) { 
          job_delete(jobs, job);
          return;
        }

        //job paused, ctrl-Z
        else if (WIFSTOPPED(childstatus)) {
          job_set_status(jobs, job, JOB_STATUS_STOPPED); 
          job_print(jobs, job); 
          return;
        }

        //job completed
        else {
          job_set_status(jobs, job, JOB_STATUS_DONE);
          job_print(jobs, job);
          job_delete(jobs, job);
        }
      }

      //check for wait error
      else {
        perror("wait");
        shell_builtin_exit(0);
        job_delete(jobs, job);
        shell_builtin_exit(0);
        return;  
      }
      return ;    
    }
  }

  //if background
  else {

    Job* job;

    //if child
    if (pid == 0) {

      //execute command
      int result = execvp(command[0], &command[0]);

      //check for execvp error
      if (result == -1) {
        perror("exec");
        exit(1);        
      }
    }

    //if parent
    else {
      //save and print job. return without waiting for child
      job = job_save(jobs, pid , command, JOB_STATUS_BACKGROUND);
      job_print(jobs,job);
      return;
    }  
  }
}

/**
 * Run the given builtin or executable command.
 *
 * Arguments:
 * jobs       - job list for savining or deleting jobs (Part 2+)
 * command    - command array of executable and arguments
 * foreground - indicates foreground (1/true) vs. background (0/false).
 */
void shell_run_command(JobList* jobs, char** command, int foreground) {

  //run either built in or executable depending on type of command
  int builtin = shell_run_builtin(jobs,  command);

  typedef unsigned long word_t;
  printf("%ld",sizeof(word_t));
  if (!builtin) {
    shell_run_executable(jobs, command, foreground);
    return;
  }

  //free command
  command_free(command);
  return;
}


/**
 * Main shell loop: read, parse, execute.
 *
 * Arguments:
 * argc - number of command-line arguments for the shell.
 * argv - array of command-line arguments for the shell.
 *
 * Return:
 * exit status - 0 for normal, non-zero for error.
 */
int main(int argc, char** argv) {
  // Initialize a job list (used only in Part 2+).
  
  
  // Load history if available.
  using_history();
  read_history(HIST_FILE);
  JobList* jobs = joblist_create();
  term_shell_init(jobs);
  int ctrl_D = 0;
  char* line = NULL;

  // Shell loop
  while ((line = readline(PROMPT)) || (politeExit(jobs, &ctrl_D) == 1)) {
    // Read a command line. Supports command history, emacs key
    // bindings, etc. Calls malloc internally.
    // char* line = readline(PROMPT);
    if (ctrl_D == 1) {
      ctrl_D = 0; 
      continue;
    }


    // Add line to command history.
    add_history(line);

    // Parse command line: this is Pointer Potions.
    // It allocates a command array.
    int fg = -1;
    int redirect = -1;
    char** command = command_parse(line, &fg);

    
    if (command == NULL) {
      // Command syntax is invalid. Complain.
      printf("Invalid command syntax: %s\n", line);
    } else {
      // Command syntax is valid. Try to run the command.

      shell_run_command(jobs, command, fg);
      job_iter(jobs, reapprint);
    }

    
    // Free command line.
    free(line);
  }

  printf("Invalid command syntax");

}
