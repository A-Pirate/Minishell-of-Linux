#include "minishell.h"

pid_t pid = 0;                         // current foreground child
int status = 0;

volatile sig_atomic_t minishell_sigchild = 0;
volatile sig_atomic_t got_sigint = 0;
volatile sig_atomic_t got_sigtstp = 0;

char input_string[50];
char prompt[25] = "minishell$: ";

int main(void)
{
    system("clear");
    scan_input(prompt, input_string);
}