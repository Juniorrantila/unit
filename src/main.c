#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/wait.h>
#ifdef __linux__
   #include <linux/limits.h>
#elif __APPLE__
   #include <sys/syslimits.h>
#endif

#define NC        "\x1b[0;0m"
#define BLACK     "\x1b[0;30m"
#define DGRAY     "\x1b[1;30m"
#define RED       "\x1b[0;31m"
#define LRED      "\x1b[1;31m"
#define GREEN     "\x1b[0;32m"
#define LGREEN    "\x1b[1;32m"
#define BROWN     "\x1b[0;33m"
#define YELLOW    "\x1b[1;33m"
#define BLUE      "\x1b[0;34m"
#define LBLUE     "\x1b[1;34m"
#define PURPLE    "\x1b[0;35m"
#define LPURPLE   "\x1b[1;35m"
#define CYAN      "\x1b[0;36m"
#define LCYAN     "\x1b[1;36m"
#define LGRAY     "\x1b[0;37m"
#define WHITE     "\x1b[1;37m"

#ifdef __linux__
    #define NUMBERS_C YELLOW
    #define TESTNAM_C LCYAN
    #define SUCCESS_C LGREEN
    #define FAILURE_C LRED
    #define MESSAGE_C LPURPLE
#elif __APPLE__
    #define NUMBERS_C YELLOW
    #define TESTNAM_C CYAN
    #define SUCCESS_C GREEN
    #define FAILURE_C RED
    #define MESSAGE_C PURPLE
#endif

#define errndie(msg) do {perror(msg); exit(1);} while(0)

#define spoon() \
   pid = fork(); \
   if (pid < 0) errndie("Fork"); \
   else if (pid == 0)

#define cmp(a, b) strncmp(a, b, strlen(b))

void config();

int main(int argc, char *argv[]){

   char* dir = getenv("UNIT_DIR");
   if (dir == NULL)
      dir = "tests";
   
   char* ext = getenv("UNIT_EXT");
   if (ext == NULL)
      ext = ".test.out";

   switch(argc){
      case 3:
         ext = argv[2];
      case 2:
         if (cmp(argv[1], "-help") == 0)
            goto usage;
         else dir = argv[1];
      case 1:
         break;
      usage:
         printf("USAGE: %s [directory | options] [extension]\n"
                "\n"
                "SETTINGS:\n"
                "\tdirectory (UNIT_DIR) = %s\n"
                "\textension (UNIT_EXT) = %s\n"
                "\n"
                "OPTIONS:\n"
                "\t-help\tShow help message\n"
                "\n"
                ,
                argv[0], dir, ext);
         exit(0);
   }
   const int ext_size = strlen(ext);
   
   struct dirent *de;
   DIR* dr = opendir(dir);
   if (dr == NULL){
      perror("DIR");
      exit(1);
   }
   int elems = 0;
   while ((de = readdir(dr)) != NULL){
      if (strncmp(de->d_name+strlen(de->d_name) - ext_size, ext, ext_size) == 0){
         elems++;
      }
   }
   closedir(dr);

   if (elems == 0){
      fprintf(stderr, "There are no %s files in %s.\n", ext, dir);
      exit(1);
   }
   char** program = calloc(elems, sizeof(char*));
   int* exit_code = mmap(NULL, elems*sizeof(int), PROT_READ | PROT_WRITE, 
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
   pid_t* pids =  mmap(NULL, elems*sizeof(pid_t), PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS, -1, 0);

   dr = opendir(dir);
   if (dr == NULL){
      perror("DIR");
      exit(1);
   }
   for (int i = 0; (de = readdir(dr));){
      if (strncmp(de->d_name+strlen(de->d_name) - ext_size, ext, ext_size) == 0){
         program[i] = malloc(PATH_MAX*sizeof(char));
         snprintf(program[i], PATH_MAX, "%s/%s", dir, de->d_name);
         i++;
      }
   }
   closedir(dr);

   printf("\nRunning " YELLOW "%d" NC " tests\n\n", elems);
   pid_t pid = 0;
   for (int i = 0; i<elems; i++){
      printf(NUMBERS_C "%d/%d\t" TESTNAM_C "%s\t" MESSAGE_C "Started\n", i+1, elems, program[i]);
      spoon(){
         spoon(){
            char* log = malloc(PATH_MAX*sizeof(char));
            snprintf(log, PATH_MAX, "%s.log", program[i]);
            if (log){
               fflush(stdout);
               freopen(log, "w", stdout);
            }
            free(log);
            argv[0] = program[i];
            if (execvp(program[i], argv))
               errndie(program[i]);
         }
         int status;
         wait(&status);
         if (WIFEXITED(status)){
            exit_code[i] = WEXITSTATUS(status);
         }
         exit(0);
      }
      pids[i] = pid;
   }
   
   int success = 0;
   for (int i = 0; i<elems; i++){ 
      waitpid(pids[i], NULL, 0);
      printf(NUMBERS_C "%d/%d\t" TESTNAM_C "%s\t", i+1, elems, program[i]);
      if (exit_code[i])
         printf(FAILURE_C "Failure " NC "(%d)\n", exit_code[i]);
      else {
         printf(SUCCESS_C "Success\n");
         success++;
      }
   }
   printf("\n");
  
   if (success == elems)
      printf(SUCCESS_C "All tests passed!\n\n" NC);
   else
      printf(NUMBERS_C "%d " NC "out of " NUMBERS_C "%d " NC
            "succeeded\n\n", success, elems);

   munmap(exit_code, elems*sizeof(int));
   for (int i = 0; i<elems; i++){
      free(program[i]);
   }
   free(program);
   return 0;
}
