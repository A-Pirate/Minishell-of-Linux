#include "minishell.h"
#include <errno.h>
#include <signal.h>

char *external_cmd[200]; // 200 indicates the number of rows
extern char input_string[50];
extern char prompt[25];
extern char *builtins[];
extern pid_t pid;                                // pid_t is defined in <sys/types.h>
extern int status;                               // Volatile to prevent CPU optimization issues in signal handling
extern volatile sig_atomic_t minishell_sigchild; // used to track whether a SIGCHLD signal (child process termination) has been received
extern volatile sig_atomic_t got_sigint;         // flag to indicate SIGINT received
extern volatile sig_atomic_t got_sigtstp;        // flag to indicate SIGTST

Slist *head = NULL; // head pointer for stopped process list

// void own_signal_handler(int signum)
// {
//     if(signum == SIGINT)
//     {
//         if(pid == 0)
//             printf("\no_of_tokens %s", prompt);
//     }
//     if(signum == SIGTSTP)
//     {
//         if(pid == 0)
//             printf("\n%s", prompt);
//         else
//             insert_first(&head, pid, input_string);
//     }
// }
int extract_external_commands(char **external_commands) // fn for external commands extraction from txt file
{
    int fd = open("ext_commands.txt", O_RDONLY);
    if (fd == -1)
    {
        if (errno == EACCES)
        {
            perror("open");
            return FAILURE;
        }
    }

    char ch, buff[200];
    int i = 0, j = 0;

    for (int i = 0; i < 200; i++)
        external_cmd[i] = NULL;

    while (read(fd, &ch, 1) > 0)
    {
        if (i < (int)sizeof(buff) - 1)
            buff[i++] = ch;

        if (ch == '\n' && i > 0)
        {
            buff[i - 1] = '\0';
            if (i > 1 && buff[i - 2] == '\r') buff[i - 2] = '\0';//The issue was that ext_commands.txt uses Windows-style CRLF line endings (\r\n), causing each command string to include a trailing \r character.
            external_cmd[j] = malloc((strlen(buff) + 1) * sizeof(char)); // allocate memory
            strcpy(external_cmd[j], buff);                               // copy the commands to 2d array

            j++;
            i = 0;
        }
    }

    if (i > 0) // To include the command in the last line if it doesn't end with a newline
    {
        buff[i] = '\0';
        if (i > 0 && buff[i - 1] == '\r') buff[i - 1] = '\0';
        external_cmd[j] = malloc((strlen(buff) + 1) * sizeof(char)); // allocate memory for last line
        strcpy(external_cmd[j], buff);
    }
    external_cmd[j] = NULL;

    // for (int i = 0; i < 152; i++)
    //     printf("External Command %d: %s\n",i+1, external_cmd[i]);

    close(fd);
    return SUCCESS;
}

void scan_input(char *prompt, char *input_string)
{
    // signal(SIGINT, signal_handler);
    // signal(SIGTSTP, signal_handler);
    // signal(SIGCHLD, signal_handler);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);

    if (extract_external_commands(external_cmd) == FAILURE)
    {
        printf("Error in extracting external commands\n");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        // Printing the prompt
        printf("\n%s", prompt);
        if (scanf("%[^\n]", input_string) != 1)
        {
            input_string[0] = '\0';
        }
        char ch; // to clear the buffer
        while ((ch = getchar()) != '\n')
            ; // to clear the buffer

        // If PS1 is used
        if (!strncmp(input_string, "PS1=", 4) && input_string[4] != ' ')
        {
            strcpy(prompt, input_string + 4); // Changing the prompt by doing anything after PS1=
        }
        else
        {
            char *cmd = get_command(input_string);
            int type = check_command_type(cmd);
            if (type == BUILTIN)
                execute_internal_commands(input_string);
            else if (type == EXTERNAL)
            {
                pid = fork();

                if (pid > 0)
                {
                    waitpid(pid, &status, WUNTRACED);

                    if (WIFSTOPPED(status))
                    {
                        sigset_t set, old;
                        sigemptyset(&set);
                        sigaddset(&set, SIGCHLD);

                        sigprocmask(SIG_BLOCK, &set, &old);
                        insert_at_first(pid, input_string);
                        sigprocmask(SIG_SETMASK, &old, NULL);
                    }
                }

                else if (pid == 0) // This is child process
                {
                    signal(SIGINT, SIG_DFL); // make it default again
                    signal(SIGTSTP, SIG_DFL);
                    execute_external_commands(input_string); // call ext_execute fn
                }

                if (minishell_sigchild)
                {
                    pid_t dead;
                    while ((dead = waitpid(-1, &status, WNOHANG)) > 0)
                    {
                        delete_pid_node(dead);
                    }
                    minishell_sigchild = 0;
                }

                if (got_sigint)
                {
                    write(STDOUT_FILENO, "\n", 1);
                    write(STDOUT_FILENO, prompt, strlen(prompt));
                    got_sigint = 0;
                }

                if (got_sigtstp)
                {
                    write(STDOUT_FILENO, "\n", 1);
                    write(STDOUT_FILENO, prompt, strlen(prompt));
                    got_sigtstp = 0;
                }
            }
            else
            {
                printf(ANSI_COLOR_CYAN "cmd --> %s   type --> NO_COMMAND\n" ANSI_COLOR_RESET, cmd); // test
            }
            free(cmd);
        }
    }
}

int check_command_type(char *command) // fn to check command type
{
    for (int i = 0; builtins[i] != NULL; i++)
    {
        if (!strcmp(builtins[i], command))
            return BUILTIN;
    }

    for (int i = 0; external_cmd[i] != NULL; i++)
    {
        // printf("COMMAND : %s \n", external_cmd[i]);
        if (strcmp(external_cmd[i], command) == 0)
        {
            return EXTERNAL;
        }
    }
    return NO_COMMAND;
}

char *get_command(char *input)
{
    static char empty[] = "";

    if (!input || input[0] == '\0')
        return empty;

    char *cmd = malloc(20);
    if (!cmd)
        return empty;

    if (sscanf(input, "%19s", cmd) != 1)
    {
        free(cmd);
        return empty;
    }

    char *tmp = realloc(cmd, strlen(cmd) + 1);
    return tmp ? tmp : cmd;
}

// int insert_first(Slist **head, pid_t pid, char *string)
// {
//     Slist *new = (Slist *)malloc(sizeof(Slist))
//     if(*head == NULL)
//     {

//     }
// }

void execute_internal_commands(char *input_string) // fn to execute internal commands
{
    if (!strncmp(input_string, "exit", 4))
    {
        for (int i = 0; i < 200; i++)
        {
            free(external_cmd[i]);
        }
        exit(0);
    }
    else if (!strncmp(input_string, "pwd", 3))
    {
        char buff[100];
        getcwd(buff, 100); // get current working directory by getcwd()
        printf("%s\n", buff);
    }
    else if (!strncmp(input_string, "cd", 2))
    {
        chdir(input_string + 3); // change directory by chdir() by input_String + 3 = Path to change
        char buff[200];
        getcwd(buff, 200); // get current working directory by getcwd()
        printf("%s\n", buff);
    }
    else if (!strncmp(input_string, "echo $$", 7)) // echo $$ to print current process id
    {
        printf("%d\n", getpid()); // print the current process id
    }
    else if (!strncmp(input_string, "echo $?", 7)) // echo $? to print previous execution status
    {
        printf("%d\n", WEXITSTATUS(status)); // print prev execution is success or not
    }
    else if (!strncmp(input_string, "echo $SHELL", 11)) // echo $SHELL to print the shell path
    {
        char *buff = getenv("SHELL"); // get the SHELL environment variable value
        if (buff != NULL)
            printf("%s\n", buff);
    }
    else if (!strcmp(input_string, "jobs"))
    {
        print_stop_process(); // print the stopped process list
    }
    else if (!strcmp(input_string, "fg"))
    {
        if (head == NULL)
        {
            printf("-bash: fg: current: no such job\n");
        }
        else
        {
            pid_t fg_pid = head->pid;
            char *fg_cmd = strdup(head->string);
            if (!fg_cmd)
            {
                printf("Memory allocation failed\n");
                return;
            }
            kill(fg_pid, SIGCONT);
            waitpid(fg_pid, &status, WUNTRACED);
            delete_first(&head);
            if (WIFSTOPPED(status))
            {
                insert_at_first(fg_pid, fg_cmd);
            }
            free(fg_cmd);
        }
    }
    else if (!strcmp(input_string, "bg"))
    {
        if (head == NULL)
        {
            printf("-bash: bg: current: no such job\n");
        }
        else
        {
            kill(head->pid, SIGCONT);
        }
    }

    return;
}

// Jobs List Functions
void print_stop_process() // printing nodes
{
    if (head == NULL)
    {
        return;
    }
    int i = 0; // When stopped process exists
    Slist *temp = head;
    while (temp != NULL)
    {
        printf("[%d][pid:%d]    Stopped    %s\n", ++i, temp->pid, temp->string);
        temp = temp->link;
    }
    return;
}

void insert_at_first(pid_t cpid, char *cmd)
{
    Slist *node = malloc(sizeof(Slist));
    if (!node)
        return;

    node->pid = cpid;
    node->string = malloc(strlen(cmd) + 1);
    if (!node->string)
    {
        free(node);
        return;
    }
    strcpy(node->string, cmd);

    node->link = head;
    head = node;
}

int delete_pid_node(pid_t del_pid)
{
    Slist *cur = head;
    Slist *prev = NULL;

    while (cur)
    {
        if (cur->pid == del_pid)
        {
            if (prev)
                prev->link = cur->link;
            else
                head = cur->link;

            free(cur->string);
            free(cur);
            return SUCCESS;
        }
        prev = cur;
        cur = cur->link;
    }
    return FAILURE;
}

int delete_first(Slist **head)
{
    if (head == NULL)
        return -1;

    Slist *temp = *head;
    *head = (*head)->link;
    free(temp->string);
    free(temp);
    return SUCCESS;
}

// Functions to execute external & internal commands
void execute_external_commands(char *input)
{
    char *tokens[64];
    int nt = 0;

    char *tmp = strdup(input);
    if (!tmp)
        exit(1);

    char *p = strtok(tmp, " ");
    while (p && nt < 63)
    {
        tokens[nt++] = strdup(p);
        p = strtok(NULL, " ");
    }
    tokens[nt] = NULL;
    free(tmp);

    struct command cmds[10] = {0};
    int ci = 0, ai = 0;

    for (int i = 0; i < nt; i++)
    {
        if (!strcmp(tokens[i], "|"))
        {
            if (ai == 0)
            {
                fprintf(stderr, "syntax error near '|'\n");
                goto cleanup;
            }
            cmds[ci++].argv[ai] = NULL;
            ai = 0;
        }
        else
        {
            if (ai >= 19) // argv[20] limit
            {
                fprintf(stderr, "minishell: too many arguments\n");
                goto cleanup;
            }
            cmds[ci].argv[ai++] = tokens[i];
        }
    }
    cmds[ci].argv[ai] = NULL;
    int ncmds = ci + 1;

    if (ncmds == 1)
    {
        execvp(cmds[0].argv[0], cmds[0].argv);
        perror("execvp");
        exit(1);
    }

    int pipefd[9][2];
    for (int i = 0; i < ncmds - 1; i++)
        pipe(pipefd[i]);

    for (int i = 0; i < ncmds; i++)
    {
        pid_t child = fork();
        if (child == 0)
        {
            if (i > 0)
                dup2(pipefd[i - 1][0], STDIN_FILENO);
            if (i < ncmds - 1)
                dup2(pipefd[i][1], STDOUT_FILENO);

            for (int j = 0; j < ncmds - 1; j++)
            {
                close(pipefd[j][0]);
                close(pipefd[j][1]);
            }

            execvp(cmds[i].argv[0], cmds[i].argv);
            perror("execvp");
            exit(1);
        }
    }

    for (int i = 0; i < ncmds - 1; i++)
    {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }

    for (int i = 0; i < ncmds; i++)
        wait(NULL);

cleanup:
    for (int i = 0; i < nt; i++)
        free(tokens[i]);
}