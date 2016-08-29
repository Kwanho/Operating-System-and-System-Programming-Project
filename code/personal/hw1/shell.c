#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_quit(tok_t arg[]);
int cmd_help(tok_t arg[]);
int cmd_pwd(tok_t arg[]);
int cmd_cd(tok_t arg[]);
int cmd_wait(tok_t arg[]);
int execute(tok_t **tokens_p, char ***PATH_tokens_p, bool background);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(tok_t args[]);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_quit, "quit", "quit the command shell"},
  {cmd_pwd, "pwd", "prints the current working directory"},
  {cmd_cd, "cd", "changes the current working directory"},
  {cmd_wait, "wait", "wait until all background jobs have terminated"},
};

/**
 * Prints a helpful description for the given command
 */
int cmd_help(tok_t arg[]) {
  for (int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++) {
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  }
  return 1;
}

/**
 * Quits this shell
 */
int cmd_quit(tok_t arg[]) {
  exit(0);
  return 1;
}

/**
 * Prints the current working directory
 */
int cmd_pwd(tok_t arg[]) {
  size_t PATH_MAX=1024;
  char buffer[PATH_MAX+1];
  fprintf(stdout, "%s\n", getcwd(buffer, PATH_MAX+1));
  return 1;
}

/**
 * Changes the current working directory
 */
int cmd_cd(tok_t arg[]) {
  chdir(*arg);
  return 1;
}

/**
 * wait until all background jobs have terminated
 */
int cmd_wait(tok_t arg[]) {
  int status;
  while (waitpid(-1, &status, 0)) {
    if (errno==ECHILD) {
      break;
    }
  }
  return 1;
}

/**
 * Looks up the built-in command, if it exists.
 */
int lookup(char cmd[]) {
  for (int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++) {
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
  }
  return -1;
}

/**
 * 
 */
int redirection_from_process_to_file(int redirection_index, tok_t **tokens_p, char ***PATH_tokens_p, bool background) {
  tok_t *tokens = *tokens_p;
  char **PATH_tokens = *PATH_tokens_p;
  int fd;

  tokens[redirection_index]=NULL;
  fd = open(tokens[redirection_index+1], O_CREAT|O_TRUNC|O_WRONLY, 0644);
  dup2(fd, 1);
  execute(&tokens, &PATH_tokens, background);
  close(fd);
  return 1;
}

/**
 * 
 */
int redirection_from_file_to_process(int redirection_index, tok_t **tokens_p, char ***PATH_tokens_p, bool background) {
  tok_t *tokens = *tokens_p;
  char **PATH_tokens = *PATH_tokens_p;
  int fd;

  tokens[redirection_index]=NULL;
  fd = open(tokens[redirection_index+1], O_RDONLY, 0644);
  dup2(fd, 0);
  execute(&tokens, &PATH_tokens, background);
  close(fd);
  return 1;
}


/**
 * 
 */
int execute(tok_t **tokens_p, tok_t **PATH_tokens_p, bool background) {
  tok_t *tokens = *tokens_p;
  tok_t *PATH_tokens = *PATH_tokens_p;

  //int status;
  char *full_path;
  pid_t pid=fork();

  if (pid==0) { //in child process
    struct termios termios_p;
    tcgetattr(0, &termios_p);
    if (!background){
      put_process_in_foreground(getpid(), 1, &termios_p);
    } else {
      put_process_in_background(getpid(), 1);
    }
    

    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGKILL, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGCONT, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);

    if (tokens[0][0]=='/') { //if full path
      execv(tokens[0], tokens);
    } else {  //if not full path, we need to use PATH
      for (int i=0; PATH_tokens[i]!=NULL; i++) {
        full_path=(char *)malloc(strlen(PATH_tokens[i])+strlen(tokens[0])+1);
        strcpy(full_path, PATH_tokens[i]);
        strcat(full_path, "/");
        strcat(full_path, tokens[0]);
        if (access(full_path, F_OK)==0) {
          execv(full_path, tokens);
        }
        free(full_path);
      }
    }
    exit(1);
  } else {  //in parent process
  }
  return 1;
}

/**
 * Intialization procedures for this shell
 */
void init_shell() {
  /* Check if we are running interactively */
  shell_terminal = STDIN_FILENO;
  shell_is_interactive = isatty(shell_terminal);

  if(shell_is_interactive){
    /* Force the shell into foreground */
    while(tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

int shell(int argc, char *argv[]) {
  char *input_bytes;
  tok_t *tokens;
  int line_num = 0;
  int fundex = -1;

  char *PATH = getenv("PATH");
  tok_t *PATH_tokens = get_toks(PATH);

  int redirection = 0;
  int index;
  int redirection_index = -1;
  int ampersand_index = -1;
  bool background=0;

  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGKILL, SIG_IGN);
  signal(SIGTERM, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGCONT, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);

  init_shell();

  if (shell_is_interactive)
    /* Please only print shell prompts when standard input is not a tty */
    fprintf(stdout, "%d: ", line_num);

  while ((input_bytes = freadln(stdin))) {
    tokens = get_toks(input_bytes);
    fundex = lookup(tokens[0]);

    if (fundex >= 0) {  //built-in cmd
      cmd_table[fundex].fun(&tokens[1]);
    } else {  //not built-in cmd
      /* REPLACE this to run commands as programs. */

      for (index=0; tokens[index]!=NULL; index++) {
        if (strcmp(tokens[index], ">")==0) {
          redirection=1;
          redirection_index=index;
        }
        if (strcmp(tokens[index], "<")==0) {
          redirection=-1;
          redirection_index=index;
        }
        if (strcmp(tokens[index], "&")==0) {
          ampersand_index=index;
          background=1;
          tokens[ampersand_index]=NULL;
        }
      }

      if (redirection==1) {
        redirection_from_process_to_file(redirection_index, &tokens, &PATH_tokens, background); 
      } else if (redirection==-1) {
        redirection_from_file_to_process(redirection_index, &tokens, &PATH_tokens, background);
      } else {
        execute(&tokens, &PATH_tokens, background);
      }
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);
  }

  return 0;
}