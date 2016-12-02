#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <errno.h>

#define MAXLINE	259
#define PROMPT getPrompt()
#define MAX_ARG_LIST	200

extern char **environ;
extern int  errno;

#define MAX_CMD_SIZE	50
#define SEARCH_FOR_CMD	-1
typedef void (*buildInFunc) (char **);
typedef struct {
	char cmd[MAX_CMD_SIZE];
	buildInFunc func;
} builtInCmd;

// built-in commands
void execExit(char *cmd[]);
void execPwd(char *cmd[]);
void execCd(char *cmd[]);

int redirect_out(int new_fd);

builtInCmd builtInCmds[] = {
	{"exit", execExit  },
	{"pwd",  execPwd   },
	{"cd",   execCd    }
};
int builtInCnt = sizeof(builtInCmds)/sizeof(builtInCmd);
int isBuiltIn(char *cmd);
void execBuiltIn(int i, char *cmd[]);
const char* getPrompt();

// capture SIG_INT and recover
sigjmp_buf ctrlc_buf;
void ctrl_hndlr(int signo) {
	siglongjmp(ctrlc_buf, 1);
}

int main(int argc, char *argv[]) {

	char line[MAXLINE];
	pid_t childPID;
	int argn; 
	char *args[MAX_ARG_LIST];
	int cmdn;
	
	int in_redirect_n  = 0;
	int out_redirect_n = 0;
	
	// Boolean for append out
	int append_out	   = 0;

	// setup SIG_INT handler
	if (signal(SIGINT, ctrl_hndlr) == SIG_ERR)
		fputs("ERROR: failed to register interrupts in kernel.\n", stderr);

	// setup longjmp buffer
	while (sigsetjmp(ctrlc_buf, 1) != 0) ;

	for(;;) {
	// prompt and get commandline
		fputs(PROMPT, stdout);
		fgets(line, MAXLINE, stdin);

		if (feof(stdin)) break; // exit on end of input

	// process commandline
		if (line[strlen(line)-1] == '\n')
			line[strlen(line)-1] = '\0';
		// build argument list
		args[argn=0]=strtok(line, " \t");
		while(args[argn]!=NULL && argn<MAX_ARG_LIST) {
			
			args[++argn]=strtok(NULL," \t");
			
			
			// handle redirection
			if(args[argn] != NULL && strcmp(args[argn], ">") == 0 )
				out_redirect_n = argn+1;
				
			if(args[argn] != NULL && strcmp(args[argn], "<") == 0)
				in_redirect_n = argn+1;
				
			if(args[argn] != NULL && strcmp(args[argn], ">>") == 0) {
				
				out_redirect_n = argn+1;
				append_out = 1;
			}
				
			
		}

		
	// execute commandline
	if (args[0] != NULL && (cmdn = isBuiltIn(args[0]))>-1) {
		
		int stdout_copy = dup(1);
		int stdin_copy  = dup(0);
		
		// Redirct stdin or exit upon fail to open file
		if(in_redirect_n > 0 && args[in_redirect_n] != NULL) {
		
			char* f_in_string = malloc(1024);
			size_t buffer = 80;
			strcpy(f_in_string, args[in_redirect_n]);
			
			FILE* f_in = fopen(f_in_string, "r");
			
			if(f_in != NULL) {
				
				getline(&f_in_string, &buffer, f_in);
				args[in_redirect_n] = strtok(f_in_string, "\n");

				args[in_redirect_n-1] = args[in_redirect_n];
				args[in_redirect_n] = NULL;
				
			} else {
				
				fprintf(stderr, "ERROR: %s: %s\n", args[in_redirect_n], 
					strerror(errno));
					
				_exit(1);
			}
			
			fclose(f_in);
			free(f_in_string);
			
		}
		
		
		// Redirect stdout to
		if(out_redirect_n > 0 ) {
		
			// Append or not	
			if(append_out == 0) 
				redirect_out(open(args[out_redirect_n], O_WRONLY|O_CREAT, 0666));
			else
				redirect_out(open(args[out_redirect_n], O_WRONLY|O_CREAT|O_APPEND, 0666));
		}
		
		// Execute the command
		execBuiltIn(cmdn, args);
		
		// Reset stdout and stdin
		redirect_out(stdout_copy);
		//redirect_in(stdin_copy);

	} else {
		
		childPID = fork();
		if (childPID == 0) {
			execve(args[0], args, environ);
			fputs("ERROR: can't execute command.\n", stderr);
			_exit(1);
		} else {
			waitpid(childPID, NULL, 0);
		}
	}

	// cleanup
		fflush(stderr);
		fflush(stdout);
	}

	return 0;
}

int isBuiltIn(char *cmd) {
	int i = 0;
	while (i<builtInCnt) {
		if (strcmp(cmd,builtInCmds[i].cmd)==0)
			break;
		++i;
	}
	return i<builtInCnt?i:-1;
}

void execBuiltIn(int i, char *cmd[]) {
	if (i == SEARCH_FOR_CMD)
		i = isBuiltIn(cmd[0]);
	if (i >= 0 && i < builtInCnt)
		builtInCmds[i].func(cmd);
	else
		fprintf(stderr, "ERROR: unknown built-in command\n");
}

void execExit(char *cmd[]) {
	exit(0);
}

void execPwd(char *cmd[]) {
	char cwd[1024];
	
	getcwd(cwd, sizeof(cwd));
	
	if(sizeof(cwd) != 0)
		fprintf(stdout, "%s\n", cwd);
	else
		fprintf(stderr, "ERROR: Can not print current working directory\n");
	
}

void execCd(char *cmd[]) {
	
	/*
	// Accept home '~' shortcut
	if(cmd[1] != NULL && strcmp(cmd[1],'~') == 0 || strcmp(cmd[1],'~/') == 0) {
		strcpy(cmd[1], "/home/")
		strcat(cmd[1], getlogin());
	}
	*/
	/*
	if(strcmp(cmd[1], "<") == 0 && cmd[2] != NULL)
		cmd[1] = cmd[2];
	*/
	
	if(chdir(cmd[1]) < 0) {
	
		fprintf(stderr, "ERROR: %s: %s\n", cmd[1], strerror(errno));
		
	}
}

const char* getPrompt() {
	char* prompt = malloc(1024);
	
	if(getcwd(prompt, sizeof(prompt)) == NULL)
		strcpy(prompt, "~ $ ");
	else
		strcat(prompt, " $ ");
	
	free(prompt);
	
	return prompt;
}

int redirect_out(int new_fd) {

	if(new_fd >= 0) {
		
		dup2(new_fd, STDOUT_FILENO);
		close(new_fd);
	}
	
	return new_fd;
}

int redirect_in(int new_fd) {

	if(new_fd < 0) {
		dup2(new_fd, STDIN_FILENO);
		close(new_fd);
	}
	
	return new_fd;
}