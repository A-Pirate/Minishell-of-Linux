#include "minishell.h"

extern volatile sig_atomic_t minishell_sigchild;
extern volatile sig_atomic_t got_sigint;
extern volatile sig_atomic_t got_sigtstp;

void signal_handler(int sig)
{
    if (sig == SIGINT)
        got_sigint = 1;
    else if (sig == SIGTSTP)
        got_sigtstp = 1;
    else if (sig == SIGCHLD)
        minishell_sigchild = 1;
}

// #include "minishell.h"

// extern pid_t pid;
// extern char prompt[25];
// extern char input_string[25];
// extern int status;
// extern volatile sig_atomic_t minishell_sigchild;
// extern volatile sig_atomic_t got_sigint;
// extern volatile sig_atomic_t got_sigtstp;
// void signal_handler(int sig_num)
// {
//     if (sig_num == SIGINT)
//     {
//         if (pid == 0)
//         {
//             //printf(ANSI_COLOR_RED"\n%s"ANSI_COLOR_RESET, prompt);
//             got_sigint = 1;
//             fflush(stdout);
//         }
//     }

//     else if (sig_num == SIGTSTP)
//     {
//         if (pid == 0)
//         {
//             //printf(ANSI_COLOR_GREEN"\n%s"ANSI_COLOR_RESET, prompt);
//             got_sigtstp = 1;
//             fflush(stdout);
//         }
//         else
//         {
//             /* DO NOT call insert_at_first (malloc/strcpy) from a signal handler.
//                Those functions are not async-signal-safe and corrupt the heap.
//                The main loop will detect stopped children (via waitpid with
//                WUNTRACED) and insert them into the job list in a safe context. */
//         }

//     }

//     else if (sig_num == SIGCHLD)
//     {
//          /* Do minimal, async-signal-safe work only: set a flag. Main loop will
//            reap children with waitpid(-1, WNOHANG) and update job list safely. */
//         minishell_sigchild = 1;
//     }
// }