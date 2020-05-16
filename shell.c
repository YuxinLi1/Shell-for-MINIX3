#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>


#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */

char prompt[] = ">> ";
char cmdline[MAXLINE];
char *args[MAXARGS];
char *subcmd[MAXARGS];
int cmd_num = 0;
int subcmd_num = 0;
int is_redirect = 0; /* 1:> | 2:>> | -1:< */

typedef struct historyList {
    char *commondline;
    struct historyList *next;
    struct historyList *pre;
} HisLi;

HisLi *hisList = NULL;
HisLi *tail = NULL;

void parse_cmd();

void parse_subcmd();

void cd_handler();

void record_cmd();

void history_handler();

void mytop_handler();

void type_check(char *cmd);

void execute(int n);

void execute_cmd(char *cmd);

void execute_simple(char *cmd);

void execute_redirect(char *cmd);

int print_memory(void);

void sigchld_handler(int signo);

int main(void) {

    char *tmp;
    pid_t pid;
    int *status;

    // signal(SIGCHLD,sigchld_handler);

    while (1) {
        int p = 0;
        int background_program = 0;

        printf("%s", prompt);
        fflush(stdout);
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
            fprintf(stdout, "%s\n", "fgets error");
            exit(1);
        }
        if (feof(stdin)) { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        parse_cmd();
        parse_subcmd();
        record_cmd();
        //---------------------------------------------------------------------------------------------------------------------//
        // printf("subcmd num : %d\n",subcmd_num);
        // int i;
        // for(i = 0; i < MAXARGS; i++){
        //     if(subcmd[i])
        //         printf("%s\n",subcmd[i]);
        //     else
        //         break;
        // }
        //---------------------------------------------------------------------------------------------------------------------//

        if (!args[0]) {
            continue;
        }
        if (strncmp(args[0], "cd", 2) == 0) {
//            printf("This is cd\n");
            cd_handler();
            continue;
        }
        if (strcmp(args[0], "history") == 0) {
//            printf("This is history\n");
            history_handler();
            continue;
        }
        if (strcmp(args[0], "mytop") == 0) {
//            printf("This is mytop.\n");
            mytop_handler();
            continue;
        }
        if (strcmp(args[0], "exit") == 0) {
            break;
        }
        // 判断是否为后台程序
        while (args[cmd_num - 1][p] != '\0') {
            p++;
        }
        if (args[cmd_num - 1][--p] == '&') {
            args[cmd_num - 1][p] = '\0';
            int len = strlen(subcmd[subcmd_num - 1]);
            subcmd[subcmd_num - 1][len - 1] = '\0';

            background_program = 1;
        }

        if ((pid = fork()) < 0)
            printf("fork error!\n");
        if (pid == 0) {
            /* child */
            //---------------------------------------------------------------------------------------------------------------------//
            // printf("execute(subcmd_num) - %d\n",subcmd_num);
            //---------------------------------------------------------------------------------------------------------------------//
            execute(subcmd_num);
            exit(1);
        } else {
            /* parent */
            if (background_program) {
                printf("BACK!\n");
            } else {
                if ((pid = waitpid(pid, status, 0)) < 0)
                    printf("waitpid error!\n");
            }
        }
    }
    exit(0);
}

/*
 * parse_cmd - parsing command line arguments (Separate commands by space)
 */
void parse_cmd() {
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf) - 1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
        buf++;

    /* Build the args list */
    argc = 0;
    if (*buf == '\'') {
        buf++;
        delim = strchr(buf, '\'');
    } else {
        delim = strchr(buf, ' ');
    }

    while (delim) {
        args[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
            buf++;

        if (*buf == '\'') {
            buf++;
            delim = strchr(buf, '\'');
        } else {
            delim = strchr(buf, ' ');
        }
    }
    args[argc] = NULL;
    cmd_num = argc;
}

/*
 * parse_subcmd - parsing command line subcommands (Separate commands by '|')
 */
void parse_subcmd() {
    /* Build the subcmd list */
    subcmd_num = 0;
    char *tmp = (char *) malloc(MAXLINE * sizeof(char));
    strcpy(tmp, cmdline);
    tmp[strlen(tmp) - 1] = ' ';  /* replace trailing '\n' with space */
    char *mPtr = NULL;
    mPtr = strtok(tmp, "|");
    while (mPtr != NULL) {
        int len = strlen(mPtr);
        char *str = (char *) malloc(len * sizeof(char));
        strcpy(str, mPtr);
        while (*str && (*str == ' ')) /* ignore leading spaces */
            str++;
        while (str[len - 1] == ' ') {
            str[len - 1] = '\0';
            len--;
        }
        subcmd[subcmd_num] = str;
        mPtr = strtok(NULL, "|");
        subcmd_num++;
    }
    subcmd[subcmd_num] = NULL;
}

/*
 * cd_handler - excuting "cd" cmd
 */
void cd_handler() {
    if (args[1] != NULL) {
        if (chdir(args[1]) < 0)
            switch (errno) {
                case ENOENT:
                    fprintf(stderr, "DIRECTORY NOT FOUND\n");
                    break;
                case ENOTDIR:
                    fprintf(stderr, "NOT A DIRECTORY NAME\n");
                    break;
                case EACCES:
                    fprintf(stderr, "YOU DO NOT HAVE RIGHT TO ACCESS\n");
                    break;
                default:
                    fprintf(stderr, "SOME ERROR HAPPENED IN CHDIR\n");
            }
    }
}

/*
 * record_cmd - record history commands
 */
void record_cmd() {
    char *tmp = (char *) malloc(MAXLINE * sizeof(char));
    strcpy(tmp, cmdline);

    HisLi *node;
    node = (HisLi *) malloc(sizeof(HisLi));
    node->commondline = tmp;
    node->pre = NULL;
    node->next = NULL;

    if (!hisList) {
        hisList = node;
        tail = node;
    } else {
        node->pre = tail;
        tail->next = node;
        tail = node;
    }
}

/*
 * history_handler - excuting "history n" cmd
 */
void history_handler() {
    int n = atoi(args[1]);
    struct historyList *cur = tail;
    for (int i = 1; i < n; i++) {
        if (cur->pre)
            cur = cur->pre;
        else
            break;
    }
    int i = 1;
    while (cur) {
        printf("history %d: %s", i, cur->commondline);
        cur = cur->next;
        i++;
    }
}

/*
 * mytop_handler - excuting "mytop" cmd
 */
void mytop_handler() {
    int num = print_memory();
}

//
//          parent                           child                        
//       _____________                   _____________                    
//      |             |                 |             |                   
//      |             |                 |             |                   
//      |             |                 |             |                   
//      | f[0]  f[1]  |                 | f[0]  f[1]  |                   
//      |_____________|                 |_____________|                   
//         |      |                        |      |                       
//         |      |                        |      |                       
//         |      |____________×___________|______|____________           
//         |                               |      |            |          
//         |      _____________×___________|      |            |          
//         |      |                               |            |          
//         |      |                               |            |          
//         |      |       _________________       |            |          
//         |      |      |    _________    |      |            |          
//         |      |______|___|         |___|______|            |          
//         |             |   |         |   |                   |          
//         |             |   |  pipe   |   |                   |          
//         |_____________|___|         |___|___________________|          
//                       |   |_________|   |                              
//                       |_________________|                              
//                             kernel                                     
//
//                               |
//                               |
//                               |
//                              \|/
//
//          parent                           child                        
//       _____________                   _____________                    
//      |             |                 |             |                   
//      |             |                 |             |                   
//      |             |                 |             |                   
//      | f[0]  f[1]  |                 | f[0]  f[1]  |                   
//      |_____________|                 |_____________|                   
//         |                                      |                       
//         |                                      |                       
//         |                                      |           
//         |                                      |                     
//         |                                      |                     
//         |                                      |                     
//         |                                      |                     
//         |              _________________       |                     
//         |             |    _________    |      |                     
//         |             |   |         |___|______|                     
//         |             |   |         |   |                            
//         |             |   |  pipe   |   |                             
//         |_____________|___|         |   |          
//                       |   |_________|   |                              
//                       |_________________|                              
//                             kernel                                     
//
// 
// example : cmd0 | cmd1 | cmd2
//           n = subcmd_num = 3
//                                                       ________________________._________________
//                                                      |         (n=1)      execvp(cmd0)
//                                                      |
//                             _________________________|________________________._________________
//                            |         (n=2)          fork                  execvp(cmd1)
//                            |
//  __________________________|__________________________________________________._________________
//               main(n=3)       fork                                        execvp(cmd2)
//
//

/*
 * execute - executing commands
 * n: the number of subcommand 
 */
void execute(int n) {
    pid_t pid;
    int *status;
    int fd[2];
    //---------------------------------------------------------------------------------------------------------------------//
    // printf("execute - subcmd[0] : %s\n",subcmd[0]);
    // printf("execute - n : %d\n",n);
    //---------------------------------------------------------------------------------------------------------------------//
    if (n == 1) {
        execute_cmd(subcmd[0]);
    } else {
        if ((pipe(fd)) < 0) {
            printf("pipe error!\n");
            exit(0);
        }
        if ((pid = fork()) < 0) {
            printf("fork error!\n");
            exit(0);
        }
        if (pid == 0) { /* child */
            close(fd[0]);
            dup2(fd[1], STDOUT_FILENO);
            close(fd[1]);
            execute(n - 1);
            exit(0);
        } else { /* parent */
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            //---------------------------------------------------------------------------------------------------------------------//
            // printf("execute - subcmd[n-1] : %s\n",subcmd[n-1]);
            //---------------------------------------------------------------------------------------------------------------------//
            execute_cmd(subcmd[n - 1]);
            if ((pid = waitpid(pid, status, 0)) < 0) {
                printf("waitpid error!\n");
                exit(0);
            }
        }
    }
}

/*
 * type_check - check whether redirection
 * cmd: command
 */
void type_check(char *cmd) {
    //---------------------------------------------------------------------------------------------------------------------//
    // printf("type_check begin with cmd \"%s\"\n",cmd);
    //---------------------------------------------------------------------------------------------------------------------//
    int i = 0;
    while (cmd[i] != '\0') {
        if (cmd[i] == '<') {
            is_redirect = -1; // input <
            break;
        }
        if (cmd[i] == '>') {
            if (cmd[i + 1] == '>') {
                is_redirect = 2; // output >>
                break;
            }

            is_redirect = 1; // output >
            break;
        }

        i++;
    }
    //---------------------------------------------------------------------------------------------------------------------//
    // printf("type_check end\n");
    //---------------------------------------------------------------------------------------------------------------------//
}

/*
 * execute_cmd - executing command
 * cmd: command
 */
void execute_cmd(char *cmd) {
    //---------------------------------------------------------------------------------------------------------------------//
    // printf("execute_cmd - cmd : %s\n",cmd);
    //---------------------------------------------------------------------------------------------------------------------//
    type_check(cmd);
    //---------------------------------------------------------------------------------------------------------------------//
    // printf("is_redirect : %d\n",is_redirect);
    //---------------------------------------------------------------------------------------------------------------------//
    if (is_redirect == 0)
        execute_simple(cmd);
    else
        execute_redirect(cmd);
}

/*
 * execute_simple - executing commands other than non-redirect
 * cmd: command
 */
void execute_simple(char *cmd) { /*execute nonpipe command*/
    //---------------------------------------------------------------------------------------------------------------------//
    // printf("execute_simple - cmd : %s\n",cmd);
    //---------------------------------------------------------------------------------------------------------------------//
    char *tmp;
    char **arg;
    int i = 1;
    tmp = strtok(cmd, " ");
    arg = (char **) malloc(MAXARGS * sizeof(char *));
    if ((arg[0] = (char *) malloc(strlen(tmp) * sizeof(char))) == 0) {
        printf("cmd is %s", cmd);
        printf("Malloc error in proc!\n");
        return;
    }
    strcpy(arg[0], tmp); /*command -> arg[0]*/
    while ((tmp = strtok(NULL, " ")) != NULL && i < 10) { /* separate commands by space */
        if ((arg[i] = (char *) malloc(strlen(tmp) * sizeof(char))) == 0) {
            printf("Malloc error in args!");
            return;
        }
        strcpy(arg[i++], tmp);
    }
    arg[i] = NULL;  /* NULL as end of arg*/
    //---------------------------------------------------------------------------------------------------------------------//
    // for(int j = 0;arg[j];j++){
    //     printf("arg[%d] : begin-%s-end\n",j,arg[j]);
    // }
    //---------------------------------------------------------------------------------------------------------------------//
    if (execvp(arg[0], arg) == -1) /* execute cmd */
        printf("EXECUTE ERROR!\n");
    exit(0);
}

/*
 * execute_redirect - executing redirect commands
 * cmd：command
 */
void execute_redirect(char *cmd) {
    //---------------------------------------------------------------------------------------------------------------------//
    // printf("execute_redirect - cmd : %s\n",cmd);
    //---------------------------------------------------------------------------------------------------------------------//
    int fd;
    pid_t pid;
    char *tmp;
    char **arg;
    int i = 1;
    char *c = (char *) malloc(3 * sizeof(char));

    if (is_redirect == 1) {
        strcpy(c, ">");
    } else if (is_redirect == -1) {
        strcpy(c, "<");
    } else if (is_redirect == 2) {
        strcpy(c, ">>");
    }

    arg = (char **) malloc(MAXARGS * sizeof(char *));

    tmp = strtok(cmd, c);
    arg[0] = (char *) malloc(strlen(tmp) * sizeof(char));
    int len = strlen(tmp);
    while (*tmp && (*tmp == ' ')) /* ignore leading spaces */
        tmp++;
    while (tmp[len - 1] == ' ') {
        tmp[len - 1] = '\0';
        len--;
    }
    strcpy(arg[0], tmp); /*command -> arg[0]*/

    tmp = strtok(NULL, c);
    arg[1] = (char *) malloc(strlen(tmp) * sizeof(char));
    len = strlen(tmp);
    while (*tmp && (*tmp == ' ')) /* ignore leading spaces */
        tmp++;
    while (tmp[len - 1] == ' ') {
        tmp[len - 1] = '\0';
        len--;
    }
    strcpy(arg[1], tmp); /*file -> arg[1]*/

    arg[2] = NULL;  /* NULL as end of arg*/
    //---------------------------------------------------------------------------------------------------------------------//
    // for(int j = 0;arg[j];j++){
    //     printf("arg[%d] : begin-%s-end\n",j,arg[j]);
    // }
    //---------------------------------------------------------------------------------------------------------------------//
    if (is_redirect == 1) { /* output redirect - overwrite */
        fd = open(arg[1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        dup2(fd, STDOUT_FILENO);
        close(fd);
        execute_simple(arg[0]);
        exit(0);
    } else if (is_redirect == -1) { /* input redirect */
        fd = open(arg[1], O_RDONLY);
        dup2(fd, STDIN_FILENO);
        close(fd);
        execute_simple(arg[0]);
        close(fd);
        exit(0);
    } else if (is_redirect == 2) { /* output redirect - append write */
        fd = open(arg[1], O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
        dup2(fd, STDOUT_FILENO);
        close(fd);
        execute_simple(arg[0]);
        exit(0);
    }

}

/*
 * print_memory - print memory information
 */
int print_memory(void) {
    FILE *fp;
    unsigned int pagesize;
    unsigned long total, free, largest, cached;

    if ((fp = fopen("/proc/meminfo", "r")) == NULL)
        return 0;

    if (fscanf(fp, "%u %lu %lu %lu %lu", &pagesize, &total, &free, &largest, &cached) != 5) {
        fclose(fp);
        return 0;
    }

    fclose(fp);

    printf("main memory: %ldK total, %ldK free, %ldK contig free, "
           "%ldK cached\n",
           (pagesize * total) / 1024, (pagesize * free) / 1024,
           (pagesize * largest) / 1024, (pagesize * cached) / 1024);

    return 1;
}

void sigchld_handler(int signo){
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        // printf("\nChild process %d terminated.\n",pid);
    }
    return;
}