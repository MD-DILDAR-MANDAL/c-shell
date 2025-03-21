#include <sys/wait.h> // waitpid() and associated macros
#include <unistd.h> //chdir()   fork()  exec()  pid_t
#include <stdlib.h> //malloc()  realloc()   free()  exit()  execvp() EXIT_SUCCESS, EXIT_FAILURE
#include <stdio.h> //fprintf()   stderr  getchar() perror()
#include <string.h>
#include <fcntl.h>

void lsh_loop(void);
char * lsh_read_line(void);
char **lsh_split_line(char * line);
int lsh_execute(char **args);

//argc: argument count, argv: argument vector
int main(int argc, char **argv){
    lsh_loop();
    return EXIT_SUCCESS;
}

/*Read, Parse, Execute */
void lsh_loop(void){
    char *line;
    char **args;
    int status;
    
    do{
        printf("$ > ");
        line = lsh_read_line();
        args = lsh_split_line(line);
        status = lsh_execute(args);
        free(line);
        free(args);
    }while(status);
}

#define LSH_RL_BUFSIZE 1024
char * lsh_read_line(void){
    int bufsize = LSH_RL_BUFSIZE;
    int position = 0;
    char * buffer = malloc(sizeof(char) * bufsize);
    int c;

    if(!buffer){
        fprintf(stderr,"lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    while(1){
        c = getchar();
        
        if(c == EOF || c == '\n'){
            buffer[position] = '\0';
            return buffer;
        }else{
            buffer[position] = c;
        }
        position++;

        if(position >= bufsize){
            bufsize += LSH_RL_BUFSIZE;
            buffer = realloc(buffer,bufsize);
            if(!buffer){
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

/*
We won’t allow quoting or backslash escaping is not allowed yet . 
Instead, we will simply use whitespace to separate arguments from each other. 
So the command echo "this message" would not call echo with a single argument this message, but rather it would call echo with two arguments: "this and message".
*/
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
char **lsh_split_line(char * line){
    int bufsize = LSH_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if(!tokens){
        fprintf(stderr,"lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token =  strtok(line, LSH_TOK_DELIM);
    while(token != NULL){
        tokens[position] = token;
        position++;
        if(position >= bufsize){
            bufsize += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens,bufsize * sizeof(char*));
            if(!tokens){
                fprintf(stderr,"lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

int lsh_launch(char **args){
    pid_t pid, wpid;
    int status;
    pid = fork();

    if(pid == 0){
        if(execvp(args[0], args) == -1){
            perror("lsh");
        }
        /*this exit is reached only if execvp fails 
         *if it succeds the child process stops  
         *and the new program starts running. The process is replaced
         *so rest of the code of child will never reach
         */
        exit(EXIT_FAILURE);
    }
    else if(pid < 0){
        perror("lsh");
    }
    else{
        do{ 
            wpid = waitpid(pid, &status, WUNTRACED);
        }while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}

/*Shell Builtins
If you want to change directory, you need to use the function chdir(). 
The current directory is a property of a process. 
So, if you wrote a program called cd that changed directory, 
it would just change its own current directory, and then terminate. 
Its parent process’s current directory would be unchanged. 
Instead, the shell process itself needs to execute chdir(), so that its own current directory is updated. 
Then, when it launches child processes, they will inherit that directory too.
 */

int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_type(char **args);
int lsh_echo(char **args);
int lsh_pwd(char **args);
int lsh_redirect(char **args, int *saved_stdout);

char *builtin_str [] = {
	"cd",
	"help",
	"exit",
	"type",
    "echo",
    "pwd"
};

int (*builtin_func[]) (char **) = {
	&lsh_cd,
	&lsh_help,
	&lsh_exit,
	&lsh_type,
    &lsh_echo,
    &lsh_pwd
}; 

int lsh_num_builtins(){
	return sizeof(builtin_str)/sizeof(char *);
}

int lsh_cd(char **args){
	if(args[1] == NULL){
		fprintf(stderr,"lsh: expected argument to \"cd\"\n");
	}
	else{
		if(chdir(args[1]) != 0){
			perror("lsh");
		}
	}
	return 1;
}

int lsh_help(char **args){
	printf("LSH\n");
	printf("Type program names and arguments, hit enter.\n");
	printf("The following are built in :\n");

	for(int i = 0;i < lsh_num_builtins(); i++){
		printf("%s\n", builtin_str[i]);
	}
    printf("> and >>\n");
	printf("Use the man command for information on other programs.\n");
	return 1;
}

int lsh_exit(char **args){
	return 0;
}

int lsh_type(char **args){
	if(args[1] == NULL){
		fprintf(stderr,"lsh: expected argument to \"type\" \n");	
	}
	else{
		for(int i = 0;i < lsh_num_builtins();i++){
            if(strcmp(args[1], builtin_str[i]) == 0){
                printf("%s is a shell builtin \n",args[1]);
                return 1;
            }
        }
        printf("lsh: %s not found \n",args[1]);
	}
	return 1;
}

int lsh_echo(char **args){
    if(args[1] == NULL){
        printf("\n");
        return 1;
    }

    int i = 1;
    char check;
    
    if(args[1][0] == '\'') check = '\'';
    else if(args[1][0] == '"') check = '"';
    
    while(args[i] != NULL){
        if(args[i][0] == check && args[i][ strlen(args[i]) - 1] == check){
            printf("%.*s", strlen(args[i]) - 2, args[i] + 1);
        }
        //for multiple words the below two condition required
        else if( i == 1 && args[1][0] == check ){
            printf("%s ", args[i] + 1);

        } 
        else if( args[i+1] == NULL && args[i][ strlen(args[i]) -1 ] == check){
            printf("%.*s",strlen(args[i]) - 1, args[i]);
        }
        else printf("%s ",args[i]);
        i++;
    }
    printf("\n");
    return 1;
}

int lsh_pwd(char **args){
    char *directory = malloc(1024 * sizeof(char));
    if(directory == NULL){
        perror("malloc");
        return 1;
    }
    if(getcwd(directory, 1024) == NULL){
        perror("lsh");
    }
    else printf("%s\n", directory);
    free(directory);
    return 1;
}

/* In redirect function we open the file for redirection open().Then save the original stdout 
 * using dup() in saved_stdout. Redirect output from STDOUT_FILENO to 
 * the file using dup2(), then close the file. In Execute function the 
 * built-in or external command will be detected. Now we restore original 
 * stdout using dup2(saved_stdout, STDOUT_FILENO) this redirect the 
 * output to the saved_stdout. The saved_stdout points to the file.
 * Now close(saved_stdout) to free the file descriptor
*/
int lsh_redirect(char **args, int *saved_stdout){
    int fd = -1;
    int i = 0;
    int append = 0;
    char *filename;

    while(args[i] != NULL){
        if( strcmp(args[i], ">") == 0 ){
            append = 0;
            break;
        }
        else if( strcmp(args[i], ">>") == 0){
            append = 1;
            break;
        }
        i++;
    }

    if(args[i] == NULL) return 0;

    args[i] = 0;
    
    filename = args[i + 1];
    // mode: octal notation so 4 values
    // owner : 6 - rw_ , group : 4 - r__ , others: 4 - r__
    fd = open(filename,O_WRONLY | O_CREAT | (append?O_APPEND:O_TRUNC),0644);
    if(fd < 0){
        perror("lsh");
        return -1;
    }

    *saved_stdout = dup(STDOUT_FILENO);
    //fd is the STDOUT_FILENO 
    //saved the original STDOUT_FILENO in saved_stdout earlier
    dup2(fd, STDOUT_FILENO);
    close(fd);
    return 1;
}

int lsh_execute(char **args){
	if(args[0] == NULL) return 1;
   
    int saved_stdout = -1;
    int result;
    int redirection_applied = lsh_redirect(args, &saved_stdout);

	for(int i = 0;i < lsh_num_builtins(); i++){
		if(strcmp(args[0], builtin_str[i]) == 0){
			result = (*builtin_func[i])(args);
            //output is stored in the file as  we changed the file descriptor
            //in the lsh_redirect function
            //now we will set the STDOUT_FILENO to original file descriptor
            //ie saved_stdout
            if(redirection_applied){
                dup2(saved_stdout, STDOUT_FILENO);
                close(saved_stdout);
            }
            return result;
		}
	}
    
    result = lsh_launch(args);
    if(redirection_applied){
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
	return result;
}
