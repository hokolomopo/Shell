#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#define __USE_MISC 1
#include <net/if.h>
#include <arpa/inet.h>

#define MAX_ARGUMENTS 255
#define MAX_ARGUMENTS_CARACTERS 255
#define MAX_COMMAND_LINE_LENGHT 65026 // (255*255 + 1 for \0)

#define NO_SHELL_COMMAND 2
#define SHELL_COMMAND_ERROR 3
#define COMMAND_NOT_FOUND_ERROR 4

typedef int bool;
#define true 1
#define false 0

//Structure and global variables to manage shell variables ($)
typedef struct{
    char* varName;
    char* varValue;
}shellVariable;

static int numberOfVars = 10;
static shellVariable* vars;
static int varCount = 0;

/*
 * Manage all the sys built in in the shell
 *
 *ARGUMENTS :
 * - params : an array of parameters, must be terminated with a NULL pointer
 *
 *RETURN :
 * - the return value of the built-in if a built-in corresponding to the parameters was found, 1 otherwise
 */
int sysBuiltIn(char** params);

/*
 * Manage all the sys cpu built in in the shell
 *
 *ARGUMENTS :
 * - params : an array of parameters, must be terminated with a NULL pointer
 *
 *RETURN :
 * - the return value of the built-in if a built-in corresponding to the parameters was found, 1 otherwise
 */
int cpuBuiltIn(char** params);

/*
 * Manage all the sys ip built in in the shell
 *
 * ARGUMENTS:
 *  - params : array of parameters, must be terminated with a NULL pointer
 *
 * RETURN :
 *  - the return value of the built-in if a built-in corresponding to the parameters was found, 1 otherwise
 */
int ipBuiltIn(char** params);

/*
 * Print the hostname
 *
 *RETURN :
 * - 0 if it was successful, 1 otherwise
 */
int printHostName();

/*
 * Print the cpu model
 *
 *RETURN :
 * - 0 if it was successful, 1 otherwise
 */
int printCpuModel();

/*
 * Print the N cpu frequency
 *
 *ARGUMENTS :
 * - n : the number of the cpu we want to print the frequency of
 *
 *RETURN :
 * - 0 if it was successful, SHELL_COMMAND_ERROR otherwise
 */
int printCpuNFreq(int n);

/*
 * Set the frequency of the cpu desingned by char* cpu
 *
 * ARGUMENTS :
 * - cpu : char parameter from command designing the considered cpu
 * - freq : char parameter from command indicating the wished frequency
 *
 * RETURN :
 * - 0 if it was successful, 1 otherwise
 */
int setCpuFreq(char *cpu, char *freq);

/*
 * Print or set the ip address and subnet mask of device DEV according to the parameter rw
 *
 * ARGUMENTS :
 * - params : parsed command
 * - params[3] : name DEV of the device concerned
 * - params[4] : wished ip address if writting context
 * - params[5] : wished subnet mask if writting context
 * - rw : indictaing reading context (rw == 0) or writting context (rw == 1)
 *
 * RETURN :
 * - 0 if it was successful, 1 otherwise
 */
int devIpAddressMask(char** params, int rw);

/*
 * Put the minimal and maximal frequencies of the concerned cpu in char* maxima
 *
 * ARGUMENTS :
 * - maxima : array which will be used in setCpuFreq containing the min and max frequency
 * - path : path in the sys folder of the concerned cpu
 *
 * RETURN :
 * - 0 if it was successful, 1 otherwise
 */
int cpuFrequencyMaxima(int maxima[2], char* path);

/*
 * Compare the beginning of the string "big" with he string "small"
 *
 *ARGUMENTS :
 * - big, small : 2 strings
 *
 *RETURN :
 * - 0 if a char* is NULL, or if the small string is smaller than the bug string
 * - 1 if the beginning of the big string is equal to the small string, 0 otherwise
 */
int strcmpbeginning(const char* big, const char* small);

/*
 * Search in a file a line beginning with the string "begin" and put the line in the buffer.
 *  Won't work if a line is longer than 254 characters.
 *
 *ARGUMENTS :
 * - f : a pointer to a FILE
 * - begin : the string we will search in the beginning of the lines of the files
 * - buffer : a buffer in which the function store the read line. Must be at least 255 characters.
 *
 *RETURN :
 * - 0 if a char* is NULL, or if the small string is smaller than the bug string
 * - 1 if the beginning of the big string is equal to the small string, 0 otherwise
 */
int searchBeginning(FILE* f, char* begin, char* buffer);

/*
 *  Create or update a shell variable if param[0] has the format "varName=varValue"
 *
 *ARGUMENTS :
 * - params : an array of string, must be terminated with a NULL pointer
 *
 *RETURN :
 * - 0 if a assignement was detected and done, 1 if no assignement was found,
 *    -1 if an error occured
 */
int manageShellVariables(char **params);

/*
 *  Replace in the cmd string shell $variables with their value
 *
 *ARGUMENTS :
 * - cmd : the string in which to replace the $ variables
 *
 *RETURN :
 * - 0 if successful, 1 if a problem arose
 */
int manageVarsReplacement(char* cmd);
/*
 * Set the last returned value by a process in the $? variable, and print the value
 *
 *ARGUMENTS :
 * - ret : the last returned value by a process
 *
 */
void setReturn(int ret);

/*
 * Set the pid of the last background process in the $! variable
 *
 *ARGUMENTS :
 * - pid : pid of the last background process
 *
 */
void setBackgroundPid(int pid);

/*
 * Parse cmd array into array of parameters
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
 * Execute the command in params[0], if the command in params[0] is NULL, just print a newline character
 *
 *ARGUMENTS :
 * - params : a char* array ,the list of parameters must be terminated with a NULL pointer
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

    //Initialize ! and ? variables
    if(!(vars = calloc(numberOfVars , sizeof(shellVariable)))){
        printf("Unable to allocate space for shell variables\n");
        return 0;
    }

    varCount = 2;
    vars[0].varName = "?";
    vars[0].varValue = NULL;
    vars[1].varName = "!";
    vars[1].varValue = NULL;

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

        if(manageVarsReplacement(cmd))
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

int manageVarsReplacement(char* cmd){

    //Search for $ symbols in cmd
    for(int k = 0;cmd[k] != '\0';k++){
        if(cmd[k] == '$' && cmd[k+1] != '\0'){
            //If the special character \ is used, ignore the $ symbol
            if(k >= 1 && cmd[k-1] == '\\')
                continue;

            //Find the lenght of the $ variable
            int lenght = 0;
            for(int i = k+1;cmd[i] != '\0' && cmd[i] != ' ' && cmd[i] != '/';i++)
                lenght++;

            //Copy the name of the variable in varNAme
            char varName[lenght+1];
            strncpy(varName, cmd + k+1, lenght);
            varName[lenght] = '\0';

            //Cut cmd to delete the variable name
            char tmp[MAX_COMMAND_LINE_LENGHT];
            strcpy(tmp, cmd);
            cmd[k] = '\0';
            strcat(cmd, tmp + k + 1 + lenght);

            //Search for a existing variable by the name varName
            for(int i = 0;i < varCount;i++)
                if(!strcmp(varName, vars[i].varName)){
                    if(vars[i].varValue){
                        //Error if we want to insert something too big
                        if(strlen(cmd) + strlen(vars[i].varValue) >= MAX_COMMAND_LINE_LENGHT){
                            printf("ERROR: %s\n", strerror(E2BIG));
                            return 1;
                        }

                        //Insert the variable value in cmd
                        strcpy(tmp, cmd);
                        cmd[k] = '\0';
                        strcat(cmd, vars[i].varValue);
                        strcat(cmd, tmp +k);
                        break;
                    }
                }
        }
    }

    return 0;
}

void setReturn(int ret){
    printf("\n%d", ret);
    for(int i =0;i < varCount;i++)
        if(!strcmp("?", vars[i].varName)){
            free(vars[i].varValue);
            //String of 12 chars should be enough for a int
            vars[i].varValue = malloc(12 * sizeof(char));
            sprintf(vars[i].varValue, "%d", ret);
        }
}

void setBackgroundPid(int pid){
    for(int i =0;i < varCount;i++)
        if(!strcmp("!", vars[i].varName)){
            free(vars[i].varValue);
            //String of 12 chars should be enough for a int
            vars[i].varValue = malloc(12 * sizeof(char));
            sprintf(vars[i].varValue, "%d", pid);
        }
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
                    //Delete the quote
                    for(unsigned int k = index + offset;k < strlen(cmd);k++)
                        cmd[k] = cmd[k+1];
                    quote = true;
                    continue;
                }
                //Close quote :
                else{
                    memcpy(temp, &cmd[index], offset);
                    offset++;
                    break;
                }
            }

            //Backslash to indicate special character
            else if(cmd[index+offset] == '\\' && !quote){
                //Delete the '\\' and shift everything, then skip the next character
                for(unsigned int k = index + offset;k < strlen(cmd);k++)
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

    //Check if ther's a command
    if(!params[0]){
        printf("\n");
        return;
    }

    //Check if it's a variable assignement
    int x;
    if((x = manageShellVariables(params)) != 1){
      setReturn(x);
      return;
    }


    //Check if the command is a shell command
    if(checkForShellCommands(params) != NO_SHELL_COMMAND){
        return;
    }

    //Check if we want the process as background process
    int background = 0;
    for(int i = 0;;i++)
        if(!params[i+1]){
            if(params[i][0] == '&'){
                background = 1;
                params[i] = NULL;
            }
            break;
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
        if(background == 0){
            waitpid(pid, &child, 0);

            if(WIFEXITED(child)){
                int status = WEXITSTATUS(child);
                setReturn(status);
            }
        }
        else{
          setBackgroundPid(pid);
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
        setReturn(ret);
        return 0;
    }

    return NO_SHELL_COMMAND;
}

int changeDirectory(char* dir){

    char* to;

    if(!dir){
        char* tmp = getenv("HOME");
        to = malloc(strlen(tmp) * sizeof(char));
        strcpy(to, tmp);
    }

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
        if(!params[4])
            return printCpuNFreq(atoi(params[3]));
        return setCpuFreq(params[3],params[4]);
    }

    printf("%s: no such command for sys cpu\n", params[2]);
    return 1;
}

int ipBuiltIn(char** params){

    if( !strcmp(params[1], "ip") && !strcmp(params[2], "addr") && params[3] != NULL){
        if(params[4] == NULL)
            return devIpAddressMask(params,1);
        if(params[6] == NULL)
            return devIpAddressMask(params,0);
        }

    printf("sys : Invalid arguments\n");
    return 1;
}

int printHostName(){

  char buff[255];

  FILE* f;
  f = fopen("/proc/sys/kernel/hostname", "r");

  if(f == NULL){
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

    //Take only the cpu model
    strtok(buff, ":");
    char* model = strtok(NULL, ":");

    //Delete spaces at the start of the string
    int i = 0;
    while(model[i] == ' ')
      i++;
    model += i;

    printf("%s", model);

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
    if(!begin || !buffer || !f)
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

            //Take only the cpu freq
            strtok(buff, ":");
            char* freq = strtok(NULL, ":");

            //Delete eventual spaces at the start of the string
            int i = 0;
            while(freq[i] == ' ')
              i++;
            freq += i;

            printf("%s ", freq);
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

int setCpuFreq(char *cpu, char *freq){

    FILE *f;
    const char *path1 = "/sys/devices/system/cpu/cpu";
    const char *path2 = "/cpufreq/scaling_setspeed";
    int maxima[2];
    char path[256];
    char nbCpu[3] = {cpu[0],cpu[1], '\0'};
    char buff[99];


    strcpy(path, path1);
    strcat(path, nbCpu);

    if(cpuFrequencyMaxima(maxima,path) == 1)
        return 1;

    if(maxima[0] > atoi(freq)){
        printf("Error: The input frequency is below cpu limits\n");
        return 1;
    }
    else if(atoi(freq) > maxima[1]){
      printf("Error: The input frequency is above cpu limits\n");
      return 1;
    }

    strcat(path, path2);

    if((f = fopen(path, "r")) == NULL){
        perror("Set frequency");
        return 1;
    }

    fscanf(f, "%s", buff);

    if(  !(strcmp( buff,"<unsupported>")) ){
        printf("This architecture does not allow manual frequency settings\n");
        fclose(f);
        return 1;
    }

    fprintf(f, "%s", freq);

    fclose(f);

    return 0;
}

int devIpAddressMask(char** params, int rw){

    char ip_address[15];
    char mask[15];
    int fd;
    struct ifreq ifr;

    //AF_INET - to define network interface IPv4
    //Creating soket for it.
    fd = socket(AF_INET, SOCK_DGRAM, 0);

    if(fd == -1){
        perror("Socket");
        return -1;
    }


    //AF_INET - to define IPv4 Address type.
    ifr.ifr_addr.sa_family = AF_INET;
    ifr.ifr_netmask.sa_family = AF_INET;

    //define the ifr_name params[3] == DEV
    //port name where network attached.
    memcpy(ifr.ifr_name, params[3], IFNAMSIZ);

    //reading part
    if(rw){

        if( (ioctl(fd, SIOCGIFADDR, &ifr) == -1) ){
            perror("Socket (ip address)");
            close(fd);
            return 1;
        }

        // print ip address
        printf("IP address: %s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

        if( (ioctl(fd, SIOCGIFNETMASK, &ifr) == -1) ){
            perror("Socket (mask)");
            close(fd);
            return 1;
        }

        // print mask
        printf("Mask: %s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

        close(fd);
        return 0;


    }

    //writing part

    strncpy(ip_address, params[4], sizeof(ip_address));
    strncpy(mask, params[5], sizeof(mask));

    //defining the sockaddr_in
    struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
    struct sockaddr_in *netmask = (struct sockaddr_in *)&ifr.ifr_netmask;

    //convert ip address in correct format to write
    inet_pton(AF_INET, ip_address, &addr->sin_addr);

    //Setting the Ip Address using ioctl
    if( ioctl(fd, SIOCSIFADDR, &ifr) == -1){
        perror("Ip setting");
        printf("A sudo permission is required\n");
        close(fd);
        return 1;
    }

    //convert mask in correct format to write
    inet_pton(AF_INET, mask, &netmask->sin_addr);

    //Setting the mask using ioctl
    if( ioctl(fd, SIOCSIFNETMASK, &ifr) == -1){
        perror("Mask setting:");
        printf("A sudo permission is required\n");
        close(fd);
        return 1;
    }

    //closing fd
    close(fd);

    return 0;

}

int manageShellVariables(char** params){

    if(!params[0])
        return 1;

    //Look for variable initialization
    char* varName = strtok(params[0], "=");
    char* varValue = strtok(NULL, "=");
    if(varValue){
        int found = 0;
        for(int i =0;i < varCount;i++)
            if(!strcmp(varName, vars[i].varName)){
                found = 1;
                free(vars[i].varValue);
                vars[i].varValue = malloc(sizeof(char) * (strlen(varValue)+1));
                strcpy(vars[i].varValue, varValue);
                params[0] = NULL;
            }
        if(found == 0){
            if(varCount == numberOfVars){
                numberOfVars *= 2;
                if(!(vars = realloc(vars,numberOfVars * sizeof(shellVariable) ))){
                    printf("Unable to allocate more memory for the new shell variable\n");
                    return -1;
                }
                for(int i = numberOfVars/2;i < numberOfVars;i++){
                    vars[varCount].varName = NULL;
                    vars[varCount].varValue = NULL;
                }
            }

            vars[varCount].varName = malloc(sizeof(char) * (strlen(varName)+1) );
            strcpy(vars[varCount].varName, varName);

            vars[varCount].varValue = malloc(sizeof(char) * (strlen(varValue)+1));
            strcpy(vars[varCount].varValue, varValue);

            varCount++;
            params[0] = NULL;
        }
    }
    else return 1;
    return 0;
}

int cpuFrequencyMaxima(int maxima[2], char* path){

    FILE* f;
    char str[99];
    char path_min[99];
    char path_max[99];

    strcpy(path_min,path);
    strcat(path_min,"/cpufreq/cpuinfo_min_freq");
    strcpy(path_max,path);
    strcat(path_max,"/cpufreq/cpuinfo_max_freq");

    if((f = fopen(path_min, "r")) == NULL){
        perror("Cpu min freq");
        return 1;
    }

    fscanf(f, "%s", str);
    maxima[0] = atoi(str);
    fclose(f);

    if((f = fopen(path_max, "r")) == NULL){
        perror("Cpu max freq");
        return 1;
    }

    fscanf(f, "%s", str);
    maxima[1] = atoi(str);
    fclose(f);

    return 0;
}
