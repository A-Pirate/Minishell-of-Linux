#ifndef MAIN_H
#define MAIN_H

#include <stdio.h> //headers files
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>


typedef struct sl       //structure for stopped process linked list
{
    pid_t pid;
    char *string;
    struct sl *link;
}Slist;

struct command          //Structure that holds array of commands inside array of structures
{        
    char *argv[20];
};

#define BUILTIN		1
#define EXTERNAL	2
#define NO_COMMAND  3

#define SUCCESS     0
#define FAILURE     -1

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"


void scan_input(char *prompt, char *input_string);
char *get_command(char *input_string);
void copy_change(char *prompt, char *input_string);
int extract_external_commands(char **external_commands);

int check_command_type(char *command);
void echo(char *input_string, int status);
void execute_internal_commands(char *input_string);
void execute_external_commands(char *input_string);
void signal_handler(int sig_num);

void insert_at_first(pid_t pid, char *input_string);
int delete_first(Slist **head);
int delete_pid_node(pid_t del_pid);
void print_stop_process(void);
#endif