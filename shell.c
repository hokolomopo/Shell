#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_ARGUMENTS 255
#define	MAX_ARGUMENTS_CARACTERS 255
#define MAX_COMMAND_LINE_LENGHT 65025

#define NO_SHELL_COMMAND 2
#define SHELL_COMMAND_ERROR 3

void initShell();
void parseCmd(char* cmd, char** params);

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
	char* params[MAX_ARGUMENTS];

	initShell();

	while(1){

        printf("> ");

		// Read command and exit on Ctrl+D (EOF)
        if(fgets(cmd, sizeof(cmd), stdin) == NULL){
                printf("\n");
        	break;
        }


        // Remove trailing newline character, if any
        if(cmd[strlen(cmd)-1] == '\n')
            cmd[strlen(cmd)-1] = '\0';

        //Check if there is a command
        if(strlen(cmd) == 0)
            continue;

        // Parse cmd array into array of parameters
        parseCmd(cmd, params);

        // Execute command
        executeCmd(params);
	}

	return 0;
}


void initShell(){

    printf("\033[H\033[J"); // Clearing shell
   // Print 1st command prompt
}


// Parse cmd array into array of parameters
void parseCmd(char* cmd, char** params){

	char* temp;
	size_t j = MAX_ARGUMENTS_CARACTERS;

    for(int i = 0; i < MAX_ARGUMENTS; i++) {

    	temp = strsep(&cmd, " ");

        if(temp != NULL){
            if( strlen(temp) > j){
                printf("An argument include too many characters. It should be %lu char maximum.\n", j);
                params[0] = "exit";
            }
        }

        params[i] = temp;

        if(params[i] == NULL)
        	break;
    }
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

                /*if(access(buffer, X_OK) == 0){
                    x = execv(buffer, params);
                    // Error occurred
                    char* error = strerror(errno);
                    printf("%s: %s\n", buffer, error);
                    return x;
                }*/
                params[0] = buffer;

                x = execv(params[0], params);

                i++;
            }


            // Command wasn't found in PATH
            printf("%s: command not found\n", cmd);
            exit(x);

        }

	// Parent process
	else{
		// Waiting for the child process to proceed
		int child;
		waitpid(pid, &child, 0);

        if(WIFEXITED(child)){
            int status = WEXITSTATUS(child);
            printf("%d", status);
        }
        else
            printf("127");

		return;
	}

}

int checkForShellCommands(char** params){

    int ret = NO_SHELL_COMMAND;

    if(!strcmp(params[0], "exit"))
        exit(0);
    else if(!strcmp(params[0], "cd"))
        ret = changeDirectory(params[1]);

    if(ret != NO_SHELL_COMMAND){
        printf("%d", ret);
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
