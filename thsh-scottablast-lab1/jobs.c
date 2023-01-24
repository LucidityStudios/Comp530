/* COMP 530: Tar Heel SHell
 *
 * This file implements functions related to launching
 * jobs and job control.
 */

// PID: 730245658
// PID: 730625465
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "thsh.h"

static char ** path_table;

/* Initialize the table of PATH prefixes.
 *
 * Split the result on the parenteses, and
 * remove any trailing '/' characters.
 * The last entry should be a NULL character.
 *
 * For instance, if one's PATH environment variable is:
 *  /bin:/sbin///
 *
 * Then path_table should be:
 *  path_table[0] = "/bin"
 *  path_table[1] = "/sbin"
 *  path_table[2] = '\0'
 *
 * Hint: take a look at getenv().  If you use getenv, do NOT
 *       modify the resulting string directly, but use
 *       malloc() or another function to allocate space and copy.
 *
 * Returns 0 on success, -errno on failure.
 */
int init_path(void) {
  /* Lab 0: Your code here */
  char *x = getenv("PATH");
  char *z=malloc(sizeof(char)*(strlen(x)+5)); //changed
  int ct = 0;
  strcpy(z, x);
  for (int i=0;i<strlen(z);i++)
  {
	  
	  if (z[i] == ':')
	  {
		  ct++;
	  }
  }
  /* 
  if (ct == 0)
  {
	  return 0;
  }*/
  //ct = ct + 5; 
 
  path_table = malloc(sizeof(char*)*(ct+5));

  char *strptr = z;
  char *copptr = strptr;
  //char *trmptr = copptr;
  //#1 make a pointer for where to begin copying
  //#2 make a copy pointer for incrementation. Loop through until you find a :
  //#3 now that you have the :'s address, allocate space equal to the difference of 
  // the pointer addresses +1 
  //#4 Copy this over with memcpy. be sure to add \0.
  //the copy pointer + 1 becomes the new start pointer. make a copy of this pointer and
  //increment it.
  //#5. continue this until there is no :'s left. at that point, add in the final address
  //#6 allocate one final address of size 1. make it '\0'  
  //use ptr for full string
  
  //once copptr points to a ':', we do a reverse while loop to detect how many '/'
  //exist DIRECTLY before the colon. We then subtract that many from malloc and memcpy
  //essentially cutting off the trailing '/'s
    if (strlen(z) == 0)
  {
	  return 0;
  }
  int p = 0;
  path_table[p] = copptr;
  p++;



  while(*copptr != '\0') {
	if(*copptr == ':') {
		// trimming '/'s
		
		// trimming '/'s
		path_table[p] = copptr+1; 
		*copptr = '\0';
		p++;

		copptr++;
		strptr = copptr;

	}
	else if(*copptr=='/' && *(copptr+1)=='/')
	{
		*copptr = '\0';
	}
	else {
	copptr++;
	}
	
  }
  /* 
  for(trmptr = copptr-1; *trmptr == 47; trmptr--) {
	trails++;	
  }
  path_table[p] = malloc(sizeof(copptr - strptr + 1 - trails));
  memcpy(path_table[p], strptr, copptr - strptr - trails);
  
  path_table[p+1] = malloc(sizeof('\0'));
  path_table[p + 1] = NULL;
*/
  return 0;
}

/* Debug helper function that just prints
 * the path table out.
 */
 //b jobs.c:47
 //valgrind memcheck
void print_path_table() {
  if (path_table == NULL) {
    printf("XXXXXXX Path Table Not Initialized XXXXX\n");
    return;
  }

  printf("===== Begin Path Table =====\n");
  for (int i = 0; path_table[i]; i++) {
    printf("Prefix %2d: [%s]\n", i, path_table[i]);
  }
  printf("===== End Path Table =====\n");
}

static int job_counter = 0;

struct kiddo {
  int pid;
  struct kiddo *next; // Linked list of sibling processes
};

// A job consists of a unique numeric ID and
// one or more processes
struct job {
  int id;
  struct kiddo *kidlets; // Linked list of child processes
  struct job *next; // Linked list of active jobs
};

// A singly linked list of active jobs.
static struct job *jobbies = NULL;

/* Initialize a job structure
 *
 * Returns an integer ID that represents the job.
 */
int create_job(void) {
  struct job *tmp;
  struct job *j = malloc(sizeof(struct job));
  j->id = ++job_counter;
  j->kidlets = NULL;
  j->next = NULL;
  if (jobbies) {
    for (tmp = jobbies; tmp && tmp->next; tmp = tmp->next) ;
    assert(tmp!=j);
    tmp->next = j;
  } else {
    jobbies = j;
  }
  return j->id;
}

/* Helper function to walk the job list and find                                                                    
 * a given job.                                                                                                     
 *                                                                                                                  
 * remove: If true, remove this job from the job list.                                                              
 *                                                                                                                  
 * Returns NULL on failure, a job pointer on success.                                                               
 */
static struct job *find_job(int job_id, bool remove) {
  struct job *tmp, *last = NULL;
  for (tmp = jobbies; tmp; tmp = tmp->next) {
    if (tmp->id == job_id) {
      if (remove) {
        if (last) {
          last->next = tmp->next;
        } else {
          assert (tmp == jobbies);
          jobbies = NULL;
        }
      }
      return tmp;
    }
    last = tmp;
  }
  return NULL;
}

/* Given the command listed in args,
 * try to execute it.
 *
 * If the first argument starts with a '.'
 * or a '/', it is an absolute path and can
 * execute as-is.
 *
 * Otherwise, search each prefix in the path_table
 * in order to find the path to the binary.
 *
 * Then fork a child and pass the path and the additional arguments
 * to execve() in the child.  Wait for exeuction to complete
 * before returning.
 *
 * stdin is a file handle to be used for standard in.
 * stdout is a file handle to be used for standard out.
 *
 * If stdin and stdout are not 0 and 1, respectively, they will be
 * closed in the parent process before this function returns.
 *
 * job_id is the job_id allocated in create_job
 *
 * Returns 0 on success, -errno on failure
 */
int run_command(char *args[MAX_ARGS], int stdin, int stdout, int job_id) {
  /* Lab 1: Your code here */
  //int rv = 0;
  if(*args[0] == '/' || *args[0] == '.') { //first char is / or . 
	char* path = malloc(sizeof(args[0]));
	path = args[0];  
	struct stat buffer;
	if(stat(path, &buffer) == 0) { //if cmd exists at the given path
	  int childPID = fork();
	  if(childPID == 0) {
		args++;
		execve(path, args, NULL);
	  }
	  else {
			wait(NULL);
			return 0;
	  }
	}
  }
  int i = 0;
  while (path_table[i] != NULL) {
	  char* path = malloc(sizeof(path_table[i]) + sizeof(args[0]) + 1);
	  strcat(path, path_table[i]);
	  char* tmp = "/";
	  strcat(path, tmp);
	  strcat(path, args[0]);
	  
	  struct stat buffer;
	  if(stat(path, &buffer) == 0) { //if cmd exists at the given path
	    int childPID = fork();
		if(childPID == 0) {
			args++;
			execve(path, args, NULL);
		}
		else {
			wait(NULL);
			return 0;
		}
	  }
	  
	//concatenate path_table[i] + '/' + args[0]
	//check if it exists through stat. If so, run it.
	i++;
  }
  
  // Suppress the compiler warning that find_job is not used in the starer code.
  // You may remove this line if/when you use find_job in your code.
  (void)&find_job;
  return -errno;
}

/* Wait for the job to complete and free internal bookkeeping
 *
 * job_id is the job_id allocated in create_job
 *
 * exit_code is the exit code from the last child process, if it executed.
 *           This parameter may be NULL, and is only set if the return
 *           value is zero.  This is the same as the wstatus parameter
 *           to waitpid variants, and can be used with functions such
 *           as WIFEXITED.  If this job includes multiple
 *           processes, the exit code will be the last process.
 *
 * Returns zero on success, -errno on error.
 */
int wait_on_job(int job_id, int *exit_code) {
  int ret = 0;
  return ret;
}
