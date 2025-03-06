#include <sys/wait.h> // waitpid() and associated macros
#include <unistd.h> //chdir()   fork()  exec()  pid_t
#include <stdlib.h> //malloc()  realloc()   free()  exit()  execvp() EXIT_SUCCESS, EXIT_FAILURE
#include <stdio.h> //fprintf()   stderr  getchar() perror()
#include <string.h>

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
We won’t allow quoting or backslash escaping in our command line arguments. 
Instead, we will simply use whitespace to separate arguments from each other. 
So the command echo "this message" would not call echo with a single argument this message, but rather it would call echo with two arguments: "this and message".

With those simplifications, all we need to do is “tokenize” the string using whitespace as delimiters. 
That means we can break out the classic library function strtok to do some of the dirty work for us.
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

/*How shells start processes
There are only two ways of starting processes on Unix. 
The first one (which almost doesn’t count) is by being Init. 
You see, when a Unix computer boots, its kernel is loaded. 
Once it is loaded and initialized, the kernel starts only one process, which is called Init. 
This process runs for the entire length of time that the computer is on, and it manages loading up the rest of the processes that you need for your computer to be useful.

Since most programs aren’t Init, that leaves only one practical way for processes to get started: the fork() system call. 
When this function is called, the operating system makes a duplicate of the process and starts them both running. 
The original process is called the “parent”, and the new one is called the “child”. 
fork() returns 0 to the child process, and it returns to the parent the process ID number (PID) of its child. 
In essence, this means that the only way for new processes is to start is by an existing one duplicating itself.

Typically, when you want to run a new process, you don’t just want another copy of the same program – you want to run a different program. 
That’s what the exec() system call is all about. 
It replaces the current running program with an entirely new one. 
This means that when you call exec, the operating system stops your process, loads up the new program, and starts that one in its place. 
A process never returns from an exec() call (unless there’s an error).

With these two system calls, we have the building blocks for how most programs are run on Unix. 
First, an existing process forks itself into two separate ones. 
Then, the child uses exec() to replace itself with a new program. 
The parent process can continue doing other things, and it can even keep tabs on its children, using the system call wait().

*/
int lsh_launch(char **args){
    pid_t pid, wpid;
    int status;
    pid = fork();

    if(pid == 0){
        if(execvp(args[0], args) == -1){
            perror("lsh");
        }
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

You may have noticed that the lsh_loop() function calls lsh_execute(), but above, we titled our function lsh_launch(). 
This was intentional! You see, most commands a shell executes are programs, but not all of them. 
Some of them are built right into the shell.

The reason is actually pretty simple. 
If you want to change directory, you need to use the function chdir(). The thing is, the current directory is a property of a process. 
So, if you wrote a program called cd that changed directory, it would just change its own current directory, and then terminate. 
Its parent process’s current directory would be unchanged. 
Instead, the shell process itself needs to execute chdir(), so that its own current directory is updated. 
Then, when it launches child processes, they will inherit that directory too.

Similarly, if there was a program named exit, it wouldn’t be able to exit the shell that called it. 
That command also needs to be built into the shell. 
Also, most shells are configured by running configuration scripts, like ~/.bashrc. 
Those scripts use commands that change the operation of the shell. 
These commands could only change the shell’s operation if they were implemented within the shell process itself.

So, it makes sense that we need to add some commands to the shell itself. The ones I added to my shell are cd, exit, and help.
 */

int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);
int lsh_type(char **args);
int lsh_echo(char **args);
int lsh_pwd(char **args);

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
    int i = 1;
    char check;
    
    if(args[1][0] == '\'') check = '\'';
    else if(args[1][0] == '"') check = '"';
    
    while(args[i] != NULL){
        /*if(args[i][0] == '\'' && args[i][1] == '\''){
            printf("");
        }*/
        if(args[i][1] == check && args[i][ strlen(args[i]) - 1] == check){
            printf("%.*s", strlen(args[i]) - 2, args[i] + 1);
        }
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

int lsh_execute(char **args){
	if(args[0] == NULL){
		//empty command entered
		return 1;
	}

	for(int i = 0;i < lsh_num_builtins(); i++){
		if(strcmp(args[0], builtin_str[i]) == 0){
			return (*builtin_func[i])(args);
		}
	}
	return lsh_launch(args);
}
