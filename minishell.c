/*******************************************************************************
 * Name        : minishell.c
 * Author      : Esat Adiloglu
 * Pledge      : I pledge my honor that I have abided by the Stevens Honor System
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#define BLUE "\x1b[34;1m"
#define DEFAULT "\x1b[0m"


volatile sig_atomic_t interrupted;

//Sets interrupted to SIGINT
void holdup(int signum){
    if(signum == SIGINT){
        interrupted = signum; 
    }
}
//Free the words from the heap
//The words can either be from mallocing or reallocing issues,ending func_lp, or ending an iteration
void free_words(char** words, int size){
    for(size_t i = 0; i < size; i++){
        free(words[i]);
    }
    free (words);
}

//Compares strings for qsort
int cmp(const void* a, const void* b){
    char** a_string = (char**)a;
    char** b_string = (char**)b;
    if(a_string > b_string){
        return 1;
    }
    else if (a_string < b_string){
        return -1;
    }
    else{
        return 0;
    }
}

void func_lp(){
    char** pids = NULL;
    int size = 0;
    struct stat file_info;
    struct dirent* current_file;
    FILE* filecmd;
    DIR* current_directory;
    current_directory = opendir("/proc");
        if(current_directory == NULL){
            fprintf(stderr,"Error: Cannot open current working directory. %s\n",strerror(errno));
        }
        else{
            //Iterates through /proc and puts the processes into pids
            while((current_file = readdir(current_directory)) != NULL){

                    char* temp = NULL;
                    if((temp = (char*)malloc(sizeof(char)*(strlen("/proc/")+(strlen(current_file->d_name))))) == NULL){
                        fprintf(stderr,"Error: malloc() failed. %s.\n",strerror(errno));
                        free_words(pids,size);
                        return;
                    }
                    else{
                        sprintf(temp,"/proc/%s",current_file->d_name);
                        if(stat(temp,&file_info) == 0){
                            if(S_ISDIR(file_info.st_mode) && atoi(current_file->d_name)){ //Its a pid
                                if((pids = realloc(pids,sizeof(char*) * (size+1))) == NULL){ //Adding more space for pids
                                    fprintf(stderr,"Error: malloc() failed. %s.\n",strerror(errno));
                                    free_words(pids,size);
                                    return;
                                }
                                int temp_length = strlen(temp)+1;
                                if((pids[size] = (char*)malloc(sizeof(char)*(temp_length))) == NULL){
                                    fprintf(stderr,"Error: malloc() failed. %s.\n",strerror(errno));
                                    free_words(pids,size);
                                    return;
                                }
                                sprintf(pids[size],"%s",temp);
                                size++; //Since i added another pid, i increment size by 1
                            }
                    }
                     else{
                        fprintf(stderr,"Error: Cannot access the file %s. %s.\n",current_file->d_name,strerror(errno));
                     }
                     free(temp);
                }
            }
        }
    closedir(current_directory);
    qsort(pids,size,sizeof(char*),cmp);
    struct passwd* info;
    //Iterate through pids and print out the pid, user, and cmdline
    for(size_t i = 0; i < size; i++){
        if(stat(pids[i],&file_info) == 0){
            if((info=getpwuid(file_info.st_uid)) == NULL){
                fprintf(stderr,"Error: Cannot get passwd entry. %s.\n",strerror(errno));
            }
            else{
                char* temp = pids[i];
                for(size_t i = 0; i < 6; i++){ //gets the pid from "/proc/<pid>"
                    temp++;
                }
                char* temp2 = NULL;
                if((temp2 = (char*)malloc(sizeof(char)*(strlen("/proc/")+strlen(temp)+strlen("cmdline") + 1)))== NULL){
                    fprintf(stderr,"Error: malloc() failed. %s.\n",strerror(errno));
                }
                else{
                    sprintf(temp2,"/proc/%s/cmdline",temp); 
                    filecmd = fopen(temp2,"r"); //want to read the cmdline
                    if(filecmd == NULL){
                        printf("%s %s\n",temp,info->pw_name);
                    } 
                    else{
                        char cmd[PATH_MAX];
                        memset(cmd,0,sizeof(char)*PATH_MAX);
                        fread(cmd,PATH_MAX,1,filecmd);
                        printf("%s %s %s\n",temp,info->pw_name,cmd);
                        memset(cmd,0,sizeof(char)*PATH_MAX); //reset buffer, or cmd
                    }
                    free(temp2);
                    fclose(filecmd); //close the current cmdline
                }
            }
        }
        else{
            fprintf(stderr,"Error: Cannot access the file %s. %s.\n",pids[i],strerror(errno));
        }
    }
    free_words(pids,size); //Don't need pids anymore, so I just free it from the heap
}
//Takes the arguments and puts it into an char**
char** parse_argv(char* arguments, int arg_size, int size){
    char** words = NULL;
    if((words = (char**)malloc(sizeof(char*) * (size+1))) == NULL){
        fprintf(stderr,"Error: malloc() failed. %s.\n",strerror(errno));
        return NULL;
    }
    char word[PATH_MAX];
    memset(word,0,sizeof(char)*PATH_MAX);
    size_t j = 0; //for word
    size_t k = 0; //for words
    for(size_t i = 0; i < arg_size; i++){
        if(arguments[i] != ' '){//puts a char into a string
            word[j] = arguments[i];
            j++;
        }
        if(i == arg_size-1 && arguments[i] != ' '){//End of arguments
            word[j+1] = '\0';
            int word_length = strlen(word) + 1;
            if((words[k] = (char*)malloc(sizeof(char) * word_length)) == NULL){
                fprintf(stderr,"Error: malloc() failed. %s.\n",strerror(errno));
                free_words(words,k);
                return NULL;
            }
            // strcpy(words[k],word);
            sprintf(words[k],"%s",word);
            words[k+1] = NULL;
            memset(word, 0,sizeof(char)*PATH_MAX); //resets the buffer, or word, for next iteration
        }
        if(arguments[i] != ' ' && arguments[i+1] == ' '){//Tells us that we are at the end of a word
            word[j+1] = '\0';
            int word_length = strlen(word)+1;
            if((words[k] = (char*)malloc(sizeof(char) * word_length)) == NULL){
                fprintf(stderr,"Error: malloc() failed. %s.\n",strerror(errno));
                free_words(words,k);
                return NULL;
            }
            // strcpy(words[k],word);
            sprintf(words[k],"%s",word);
            memset(word, 0,PATH_MAX * sizeof(char)); //resets the buffer, or word, for next use
            j = 0;
            k++;
        }
    }
    return words;
}
//Finds how many arguments there are
int parse_argc(char* arguments, int size){
    int count = 0;
    for(size_t i = 0; i < size; i++){
        if(i == size-1 && arguments[i] != ' '){//End of Arguments
            count++;
        }
        if(arguments[i] != ' ' && arguments[i+1] == ' '){//Tells us that we are at the end of a word
            count++;
        }
    }
    return (count);

}
int main(){
    struct sigaction sig_handle = {0};
    sig_handle.sa_handler = &holdup;
    while(1){ 
        if(sigaction(SIGINT,&sig_handle,NULL) == -1){
            fprintf(stderr,"Error: Cannot register signal handler. %s.\n",strerror(errno));
        }   
        char path_array[256];
        char* path;
        if((path = getcwd(path_array,256)) == NULL){
            fprintf(stderr,"Error: Cannot get current working directory. %s.\n",strerror(errno));
            break;
        }
        char line[PATH_MAX];
        printf("%s[%s]%s>",BLUE,path,DEFAULT);
        if((!(fgets(line,PATH_MAX,stdin)) && ferror(stdin)) || feof(stdin)){
            if(interrupted){ //Checks to see if the input was ctrl+C
                interrupted = 0;
                printf("\n");
                continue;
            }
            fprintf(stderr,"Error: Failed to read from stdin. %s.\n",strerror(errno));
            exit(EXIT_FAILURE);
        }
        line[strcspn(line, "\n")] = 0; //I want to remove the newline character that I get from fgets().
        int length = strlen(line);
        int argc = parse_argc(line,length);
        if(argc == 0){
            continue; //No arguments
        }
        char** argv = parse_argv(line,length,argc);
        if(argv == NULL){
            continue; //malloc failed
        }
        //cd
        if(strcmp(argv[0],"cd") == 0){
            uid_t id;
            struct passwd* info;
            id = getuid();
            if((info=getpwuid(id)) == NULL){
                fprintf(stderr,"Error: Cannot get passwd entry. %s.\n",strerror(errno));
            }
            else if((argc > 2)){
                fprintf(stderr,"Error: Too many arguments to cd.\n");
            }
            else if(argc == 1 || strcmp(argv[1],"~") == 0){//Go back to home directory
                if(chdir(info->pw_dir) == -1){
                    fprintf(stderr,"Error: Cannot change directory to %s. %s.\n",info->pw_dir,strerror(errno));
                }
            }
            else if(strcmp(argv[1],".") == 0){}//Don't do anything
            else{
                struct stat file_info;
                if(argv[1][0] == '/'){//Don't concat it to the cwd
                     if(stat(argv[1],&file_info) == 0){
                        if(chdir(argv[1]) == -1){
                            fprintf(stderr,"Error: Cannot change directory to %s. %s.\n",argv[1],strerror(errno));
                        }
                     }
                     else{
                        fprintf(stderr,"Error: Cannot change directory to %s. %s.\n",argv[1],strerror(errno));
                     }
                }
                else if(argv[1][0] == '~'){//convert '~' to home directory
                    char* temp = argv[1];
                    *temp++;
                    char* temp2 = (char*)malloc(sizeof(char)*(strlen(info->pw_dir)+(strlen(temp))+1));
                    sprintf(temp2,"%s%s",info->pw_dir,temp);
                     if(chdir(temp2) == -1){
                            fprintf(stderr,"Error: Cannot change directory to %s. %s.\n",temp2,strerror(errno));
                    }
                    *temp--;
                    free(temp2);
                }
                else{ //Concat argv[1] to cwd
                    char* temp = path;
                    strcat(temp,"/");
                    strcat(temp,argv[1]);
                     if(stat(temp,&file_info) == 0){
                        if(chdir(temp) == -1){
                            fprintf(stderr,"Error: Cannot change directory to %s. %s.\n",temp,strerror(errno));
                        }
                     }
                     else{
                        fprintf(stderr,"Error: Cannot change directory to %s. %s.\n",argv[1],strerror(errno));
                     }
                }
            } 
        }
        //exit
        else if(strcmp(argv[0],"exit") == 0){
        free_words(argv,argc+1);
        break;
        }
        //pwd
        else if(strcmp(argv[0],"pwd") == 0){
            char* temp;
            if((temp = getcwd(path_array,PATH_MAX)) == NULL){
                fprintf(stderr,"Error: Cannot get current working directory. %s.\n",strerror(errno));
            }
            else{
                printf("%s\n",temp);
            }
        }
        //lf
       else if(strcmp(argv[0],"lf") == 0){
            DIR* current_directory = opendir(".");
            if(current_directory == NULL){
                fprintf(stderr,"Error: Cannot open current working directory. %s\n",strerror(errno));
            }
            else{
                struct dirent* current_file;
                while((current_file = readdir(current_directory)) != NULL){
                    if(strcmp(current_file->d_name,".") != 0 && strcmp(current_file->d_name,"..") != 0){
                        printf("%s\n",current_file->d_name);
                    }
                }
            }
            closedir(current_directory);
        }
        //lp
        else if(strcmp(argv[0],"lp") == 0){
            func_lp();
        }
        //other
        //Don't forget to do signal handling
        else{
            int stat;
            pid_t pid = fork();
            if ( pid < 0){
                fprintf(stderr,"Error: fork() failed. %s.\n",strerror(errno));
            }
            else if(pid == 0){
                if(execvp(argv[0],argv) == -1){
                    fprintf(stderr,"Error: exec() failed. %s.\n",strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }
            else{
                if(wait(&stat) == -1){
                    if(interrupted){
                        interrupted = 0;
                        printf("\n");
                        if(wait(&stat) == -1){
                            fprintf(stderr,"Error: wait() failed. %s.\n",strerror(errno));
                        }
                    }
                    else{
                    fprintf(stderr,"Error: wait() failed. %s.\n",strerror(errno));
                    }
                }
            }
        }
    free_words(argv,argc+1);
    }
    
    exit(EXIT_SUCCESS);
}

