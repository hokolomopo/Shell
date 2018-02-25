#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_ARGUMENTS 255
#define MAX_ARGUMENTS_CARACTERS 255
#define MAX_COMMAND_LINE_LENGHT 65026 // (255*255 + 1 for \0)

#define NO_SHELL_COMMAND 2
#define SHELL_COMMAND_ERROR 3
#define COMMAND_NOT_FOUND_ERROR 4

typedef int bool;
#define true 1
#define false 0

int printHostName();
int sysBuiltIn(char** params);
int printCpuModel();
int cpuBuiltIn(char** params);
int strcmpbeginning(const char* big, const char* small);
int printCpuNFreq(int n);
int searchBeginning(FILE* f, char* begin, char* buffer);


/* Parse cmd array into array of parameters
 *
 *ARGUMENTS :
 * -cmd : a sting of parameter divided by spaces
 * -params : a array of string, must be non-NULL
 *
 *RETURN
 * - 0 if successful, 1 otherwise
 */
int parseCmd(char* cmd, char** params);

/*
 * Execute the command in params[0]
 *
 *ARGUMENTS :
 * - params : a char* array, params[0] is expected to be a valid pointers to null-terminated string,
 *              the list of parameters must be terminated with a NULL pointer
 *
 */
void executeCmd(char** params);

/*
 * Change the current working directory
 *
 *ARGUMENTS :
 * - dir : a string to the directory we want to move to,
 *          can be either a absolute path beginning with '/') or a relative path
 *
 *RETURN :
 * - 0 if it was successful, SHELL_COMMAND_ERROR otherwise
 */
int changeDirectory(char* dir);


/*
 * Check if the command in params[0] is a shell command (e.g. exit, cd, ...) and execute it
 *
 *ARGUMENTS :
 * - params : a char* array, params[0] is expected to be a valid pointers to null-terminated string,
 *              the list of parameters must be terminated with a NULL pointer
 *
 *RETURN :
 * - 0 if a shell command was found, NO_SHELL_COMMAND otherwise
 */
int checkForShellCommands(char** params);

int main(){

    char cmd[MAX_COMMAND_LINE_LENGHT];
    char* params[MAX_ARGUMENTS + 1]; //(plus 1 for the \0 char)

    while(1){
        
        // Clear output stream
        fflush(stdout);
        printf("> ");
        fflush(stdout);

        // Read command and exit on Ctrl+D (EOF)
        if(fgets(cmd, sizeof(cmd), stdin) == NULL)
            break;

        // Remove trailing newline character, if any
        if(cmd[strlen(cmd)-1] == '\n')
            cmd[strlen(cmd)-1] = '\0';

        //Check if there is a input
        if(strlen(cmd) == 0)
            continue;

        // Parse cmd array into array of parameters
        if(parseCmd(cmd, params) == 1)
            continue;

        // Execute command
        executeCmd(params);

        fflush(stdout);

        for(int i = 0; params[i] ;i++)
            free(params[i]);

    }

    return 0;
}


int parseCmd(char* cmd, char** params){

    char* temp;
    int index = 0;
    int i;

    for(i = 0; i < MAX_ARGUMENTS-1; i++) {
        temp = calloc(MAX_ARGUMENTS_CARACTERS + 1, sizeof(char) );
        int offset = 0;
        bool quote = false;
        bool space = true;

        while(offset < MAX_ARGUMENTS_CARACTERS){

            //Check if we're at the end of the command
            if(cmd[index+offset] == '\0'){
                if(space){
                    free(temp);
                    temp = NULL;
                    break;
                }
                if(!quote){
                    memcpy(temp, &cmd[index], offset);
                    break;
                }
                //If there's still an open quote,there's an error somewhere
                else{
                    printf("Error in quotes\n\n");
                    return 1;
                }

                break;
            }

            //Check if space is there's a space, and if the space is the end of the parameter
            if( !isspace(cmd[index+offset]) )
               space = false;
            else{
                if(!quote && !space){
                memcpy(temp, &cmd[index], offset);
                break;
                }
                else if(space){
                index++;
                continue;
                }
            }

            if(cmd[index+offset] == '"' || cmd[index+offset] == '\''){
                //Open quote
                if(!quote){
                    quote = true;
                }
                //Close quote :
                else{
                    memcpy(temp, &cmd[index+1], offset-1);
                    offset++;
                    break;
                }
            }

            //Backslash to indicate special character
            else if(cmd[index+offset] == '\\' && !quote){
                //Delete the '\\' and shift everything, then skip the next character
                for(unsigned    int k = index + offset;k < strlen(cmd);k++)
                    cmd[k] = cmd[k+1];
            }

            offset++;


        }


        params[i] = temp;

        if(offset >= MAX_ARGUMENTS_CARACTERS && strlen(temp) == 0){
            printf("An argument includes too many characters. It should be %d char maximum.\n\n", MAX_ARGUMENTS_CARACTERS);
            return 1;
        }

        if(cmd[index+offset] == '\0')
            break;

        index += offset;
    }

    params[i+1] = NULL;
    return 0;

}

void executeCmd(char** params){

    //Check if the command is a shell command
    if(checkForShellCommands(params) != NO_SHELL_COMMAND){
        return;
    }

    // Fork process
    pid_t pid = fork();

    // Error
    if(pid == -1){
        char* error = strerror(errno);
        printf("fork: %s\n", error);
        return;
    }

    // Child process
    else if(pid == 0){

            int x = 0;

            //Check if we want to launch a program in the current directory (./)
            if(strlen(params[0]) > 2 && params[0][0] == '.' && params[0][1] == '/'){

                //Get the current directory
                char currDir[MAX_COMMAND_LINE_LENGHT];
                getcwd(currDir, MAX_COMMAND_LINE_LENGHT);

                strcat(currDir, "/");
                strcat(currDir, params[0]+2);
                params[0] = currDir;

                x = execv(params[0], params);

                exit(x);
            }

            //Check if it's a absolute path to a program
            if(strlen(params[0]) > 1 && params[0][0] ==  '/'){

            x = execv(params[0], params);

            exit(x);
            }


            //Search for the command in the PATH environment

            //Create a copy of the environment PATH to be sure to not modify it
            char* env = getenv("PATH");
            char PATH[strlen(env)];
            strcpy(PATH, env);

            //Cut PATH
            size_t buffer_size = strlen(env);
            char* path[buffer_size];
            int i = 0;

            path[i] = strtok(PATH, ":");
            while(path[i++] != NULL)
                path[i] = strtok(NULL,":");

            //Memorize the command we want to execute
            char cmd[strlen(params[0])];
            strcpy(cmd, params[0]);

            //Try to execute the command in every path folder
            i = 0;
            while(path[i]){

                char buffer[MAX_COMMAND_LINE_LENGHT];

                strcpy(buffer, path[i]);
                strcat(strcat(buffer, "/") , cmd);
                params[0] = buffer;

                x = execv(params[0], params);

                i++;
            }


            // Command wasn't found in PATH
            printf("%s: command not found\n", cmd);

            exit(COMMAND_NOT_FOUND_ERROR);

        }

    // Parent process
    else{
        // Waiting for the child process to proceed
        int child;
        waitpid(pid, &child, 0);

        if(WIFEXITED(child)){
            int status = WEXITSTATUS(child);
            printf("\n%d", status);
        }
        return;
    }

}

int checkForShellCommands(char** params){

    int ret = NO_SHELL_COMMAND;

    if(!strcmp(params[0], "exit")){
        for(int i = 0; params[i] ;i++)
            free(params[i]);
        exit(0);
    }
    else if(!strcmp(params[0], "cd"))
        ret = changeDirectory(params[1]);
    else if(!strcmp(params[0], "sys"))
        ret = sysBuiltIn(params);

    if(ret != NO_SHELL_COMMAND){
        printf("\n%d", ret);
        return 0;
    }

    return NO_SHELL_COMMAND;
}

int changeDirectory(char* dir){

    char* to;

    if(!dir)
        return SHELL_COMMAND_ERROR;

    //Absolute path
    else if(dir[0] == '/'){
        to = (char*)malloc(strlen(dir) * sizeof(1));
        to = strcpy(to, dir);
    }
    //Relative path
    else{
        int size = 50;

        //Loop to be sure to have a buffer big enough to store the current directory name
        while(1){
            to = (char*)malloc(size * sizeof(char));

            getcwd(to, size);

            //If the buffer size was too small, double it
            if(errno == ERANGE){
                errno = 0;
                size*= 2;
                free(to);
            }
            else
                break;
        }

        strcat(to, "/");
        strcat(to, dir);
    }

    //Change current directory
    int changed = chdir(to);

    free(to);

    //Test for errors
    if(changed){
        printf("%s: %s\n", dir, strerror(errno));
        return SHELL_COMMAND_ERROR;
    }

    return 0;
}

int sysBuiltIn(char** params){

    if(!params[1]){
        printf("sys : not enough aruments\n");
        return 1;
    }

    if(!strcmp(params[1], "hostname"))
        return printHostName();
    else if(!strcmp(params[1], "cpu"))
        return cpuBuiltIn(params);
    else if(!strcmp(params[1], "ip"))
        return ipBuiltIn(params);

    printf("%s: no such command for sys\n", params[1]);
    return 1;
}

int cpuBuiltIn(char** params){

    if(!params[2]){
        printf("sys cpu: not enough aruments\n");
        return 1;
    }

    if(!strcmp(params[2], "model"))
        return printCpuModel();
    if(!strcmp(params[2], "freq")){
        if(!params[3]){
            printf("sys cpu freq: need the processor number as argument\n");
            return 1;
        }
        return printCpuNFreq(atoi(params[3]));
    }

    printf("%s: no such command for sys cpu\n", params[2]);
    return 1;
}

int printHostName(){

  char* hostnamePath = "/proc/sys/kernel/hostname";
  char buff[255];

  FILE* f;
  f = fopen(hostnamePath, "r");

  if(!f){
      printf("Unable to read hostname\n");
      return 1;
  }

  fscanf(f, "%s", buff);
  printf("%s \n", buff);

  fclose(f);

  return 0;

}


int printCpuModel(){

    char* cpuInfoPath = "/proc/cpuinfo";
    char buff[255];

    FILE* f;
    f = fopen(cpuInfoPath, "r");

    if(!f){
        printf("Unable to read cpu informations\n");
        return 1;
    }

    if(searchBeginning(f, "model name", buff)){
        printf("Unable to read cpu informations\n");
        return 1;
    }


    printf("%s", buff);

    fclose(f);

    return 0;

}

int strcmpbeginning(const char* big, const char* small){

    if(!big || !small || (strlen(big) < strlen(small)))
        return 0;

    int i = 0;
    while(small[i] != '\0'){
        if(big[i] != small[i])
            return 0;
        i++;
    }

    return 1;
}

int searchBeginning(FILE* f, char* begin, char* buffer){
    if(!begin || !buffer)
        return 1;

    do{
        if(!fgets(buffer, 255, f))
            return 1;
        if(strcmpbeginning(buffer, begin))
            return 0;
    }while(1);

    return 0;
}

int printCpuNFreq(int n){

    char* cpuInfoPath = "/proc/cpuinfo";
    char buff[255];

    FILE* f;
    f = fopen(cpuInfoPath, "r");

    if(!f){
        printf("Unable to read cpu informations\n");
        return 1;
    }

    while(1){

        if(searchBeginning(f, "processor", buff)){
            printf("Unable to read cpu informations\n");
            return 1;
        }

        strtok(buff, ":");
        char* numberStr = strtok(NULL, ":");
        int number = atoi(numberStr);

        if(number == n){
            if(searchBeginning(f, "cpu MHz", buff)){
                printf("Unable to read cpu informations\n");
                return 1;
            }

            printf("%s ", buff);
            break;
        }
        else if(number > n){
            printf("No process number %d\n", n);
            return 1;
        }
    }

    fclose(f);

    return 0;
}

int ipBuiltIn(char** params){
     
    if (strlen(params) < 2) 
        return 1;
    
    FILE *fp = fopen("/proc/net/arp", "r");
    
    char ip[99], hw[99], flags[99], mac[99], mask[99], dev[99], dummy[99];
    
    fgets(dummy, 99, fp); //header line
    
    while (fscanf(fp, "%s %s %s %s %s %s\n", ip, hw, flags, mac, mask, dev) != EOF)
            printf("%s\n",ip);
    
    return 0;
}