#include "minishell.h"

char input_string[50];
char prompt[25] = "minishell$:";
int main(void)
{
    system("clear");
    scan_input(prompt, input_string);
}