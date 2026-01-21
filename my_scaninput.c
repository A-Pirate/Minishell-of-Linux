#include "minishell.h"
#include <errno.h>
#include <signal.h>

char *external_cmd[152]; // 152 indicates the number of rows
extern char input_string[50];
extern char prompt[25];
extern char *builtins[];
pid_t pid;                                    // pid_t is defined in <sys/types.h>
int status;                                   // Volatile to prevent CPU optimization issues in signal handling
volatile sig_atomic_t minishell_sigchild = 0; // used to track whether a SIGCHLD signal (child process termination) has been received
Slist *head = NULL;                           // head pointer for stopped process list

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
        if (errno == EACCES){
            perror("open");
            return FAILURE;
        }
    }

    char ch, buff[30];
    int i = 0, j = 0;

    for (int i = 0; i < 152; i++)
        external_cmd[i] = NULL;

    while (read(fd, &ch, 1) > 0)
    {
        if (ch != 13) // The ASCII value (13) is a Carriage Return (CR) control character(vs-code error fix)
        {
            buff[i++] = ch;
        }

        if (ch == '\n')
        {
            buff[i - 1] = '\0';
            external_cmd[j] = malloc((strlen(buff) + 1) * sizeof(char)); // allocate memory
            strcpy(external_cmd[j], buff);                               // copy the commands to 2d array

            j++;
            i = 0;
        }
    }

    if (i > 0) // To include the command in the last line if it doesn't end with a newline
    {
        buff[i] = '\0';
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
    if(extract_external_commands(external_cmd)== FAILURE)
    {
        printf("Error in extracting external commands\n");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        // Printing the prompt
        printf("%s", prompt);
        scanf("%[^\n]", input_string);
        char ch;                          // to clear the buffer
        while ((ch = getchar()) != '\n'); // to clear the buffer

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
                    waitpid(pid, &status, WUNTRACED); // parent is waiting for child process to complete

                    // In case the child process is stopped we add its pid to the linked list by inserting that pid
                    /* add it to the job list here (safe to call malloc/strcpy). */
                    if (WIFSTOPPED(status))
                    {
                        insert_at_first(pid, input_string);
                    }
                }
                else if (pid == 0) // This is child process
                {
                    execute_external_commands(input_string);
                }

                if (minishell_sigchild == 1) // If sigchild termination signal is received
                {                            // Updated inside the SIGCHLD signal handler
                    pid_t del_pid;
                    while (del_pid = (waitpid(-1, &status, WNOHANG)) > 0)
                    {
                        if (WIFEXITED(status) || WIFSIGNALED(status))
                        { // WIFEXITED = Checks if the child terminated normally
                          // WIFSIGNALED = Checks if the child process was terminated by a signal
                            delete_pid_node(del_pid);
                        }
                    }
                    minishell_sigchild = 0;
                }
            }
            else
            {
                printf("%s command not found\n", input_string);
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
        if (strcmp(external_cmd[i], command) == 0){
            return EXTERNAL;
        }
    }
    return NO_COMMAND;
}

char *get_command(char *input_string) // fn to get command from input string
{
    char *cmd = (char *)malloc(20); // max command size is 20
    sscanf(input_string, "%s", cmd);
    cmd = realloc(cmd, strlen(cmd) + 1);
    return cmd;
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
        for (int i = 0; i < 152; i++)
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
            kill(head->pid, SIGCONT);
            waitpid(head->pid, &status, WUNTRACED);
            delete_first(&head);
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

void insert_at_first(pid_t pid, char *input_string)
{
    Slist *newnode = (Slist *)malloc(sizeof(Slist));
    if (head == NULL)
    {
        newnode->pid = pid;
        strcpy(newnode->string, input_string);
        newnode->link = NULL;
        head = newnode;
    }
    else
    {
        Slist *temp = (Slist *)malloc(sizeof(Slist));
        temp = head;
        while (temp->link != NULL)
        {
            temp = temp->link;
        }
        temp->link = newnode;
        newnode->pid = pid;
        strcpy(newnode->string, input_string);
        newnode->link = NULL;
    }
}

int delete_pid_node(pid_t del_pid)
{
    if (head == NULL)
        return FAILURE;

    Slist *temp = (Slist *)malloc(sizeof(Slist));
    Slist *prev = (Slist *)malloc(sizeof(Slist));
    temp = head;
    while (temp != NULL)
    {
        prev = temp; // prev stores current position always
        if (temp->pid == del_pid)
        {
            if (temp == head)
                head = temp->link;
            else
                prev->link = temp->link;

            free(temp);
            return SUCCESS;
        }
        temp = temp->link;
    }
    return SUCCESS;
}

int delete_first(Slist **head)
{
    if (head == NULL)
        return -1;

    Slist *temp = *head;
    *head = (*head)->link;
    free(temp);
    return SUCCESS;
}

// Functions to execute external & internal commands
void execute_external_commands(char *input_string)
{
    // Tokenizer
    char *tokens[100];
    int no_of_tokens  = 0;          // number of String tokens

    char *tmp = strdup(input_string); // strdup is dynamic memory allocator & copies the string with null char
    char *p = strtok(tmp, " ");

    while (p != NULL)
    {
        tokens[no_of_tokens ++] = strdup(p);
        p = strtok(NULL, " ");
    }
    tokens[no_of_tokens] = NULL;
    free(tmp);

    int command_count = 1;
    for (int i = 0; i < no_of_tokens ; i++)
        if (strcmp(tokens[i], "|") == 0)
            command_count++;

    struct command cmds[10]; // assuming max 10 commands with pipes
    for (int i = 0; i < 10; i++)
        for (int j = 0; j < 20; j++)
            cmds[i].argv[j] = NULL;

    int no_of_pipes = 0, arg_i = 0;
    for (int i = 0; i < no_of_tokens ; i++)         // parsing tokens into commands structure
    {
        if (strcmp(tokens[i], "|") == 0)
        {
            cmds[no_of_pipes].argv[arg_i] = NULL;
            no_of_pipes++;
            arg_i = 0;
        }
        else
        {
            cmds[no_of_pipes].argv[arg_i++] = tokens[i];
        }
    }
    cmds[no_of_pipes].argv[arg_i] = NULL;

    if (no_of_pipes == 0) // No Pipe Implementation
    {
        if (execvp(cmds[0].argv[0], cmds[0].argv) == -1)
        {
            perror("exec");
            exit(1);
        }
        return;
    }

    // Pipe Implementation
    int pipefd[10][2];
    for (int i = 0; i < no_of_pipes; i++)
        pipe(pipefd[i]);

    for (int i = 0; i <= no_of_pipes; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            if (i > 0)
            {
                dup2(pipefd[i - 1][0], STDIN_FILENO);
            }
            if (i < no_of_pipes)
            {
                dup2(pipefd[i][1], STDOUT_FILENO);
            }
            for (int j = 0; j < no_of_pipes; j++)
            {
                close(pipefd[j][0]);
                close(pipefd[j][1]);
            }

            execvp(cmds[i].argv[0], cmds[i].argv);
            perror("exec");
            exit(1);
        }
    }
    for (int j = 0; j < no_of_pipes; j++)
    {
        close(pipefd[j][0]);
        close(pipefd[j][1]);
    }
    for (int i = 0; i <= no_of_pipes; i++)
        wait(NULL);
}