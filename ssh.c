#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <setjmp.h>
#include <signal.h>
#include <errno.h>

#define MAXLINE	259
#define PROMPT "> "
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

builtInCmd builtInCmds[] = {
	{"exit", execExit  },
	{"pwd",  execPwd   },
	{"cd",   execCd    }
};
int builtInCnt = sizeof(builtInCmds)/sizeof(builtInCmd);
int isBuiltIn(char *cmd);
void execBuiltIn(int i, char *cmd[]);

// capture SIG_INT and recover
sigjmp_buf ctrlc_buf;
void ctrl_hndlr(int signo) {
	siglongjmp(ctrlc_buf, 1);
}

int main(int argc, char *argv[]) {

	char line[MAXLINE];
	pid_t childPID;
	int argn; char *args[MAX_ARG_LIST];
	int cmdn;

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
		while(args[argn]!=NULL && argn<MAX_ARG_LIST)
			args[++argn]=strtok(NULL," \t");

	// execute commandline
	if ((cmdn = isBuiltIn(args[0]))>-1) {
		execBuiltIn(cmdn, args);
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
	char cwd[2048];
	
	getcwd(cwd, sizeof(cwd));
	
	if(sizeof(cwd) != 0)
		fprintf(stdout, "%s\n", cwd);
	else
		fprintf(stderr, "ERROR: Can not print current working directory\n");
	
}

void execCd(char *cmd[]) {

	if(chdir(cmd[1]) < 0) {
	
		fprintf(stderr, "ERROR: %s: %s\n", cmd[1], strerror(errno));
		
	}
}
