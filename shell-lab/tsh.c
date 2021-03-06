/* 
 * tsh - A tiny shell program with job control
 * 
 * <Put your name and login ID here> Andrew 1
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h> 
#include <errno.h>
#include <stdbool.h>

// #include <sigaction.h>

/* Misc manifest constants */
#define MAXLINE 1024   /* max line size */
#define MAXARGS 128    /* max args on a command line */
#define MAXJOBS 16     /* max jobs at any point in time */
#define MAXJID 1 << 16 /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;   /* defined in libc */
char prompt[] = "tsh> "; /* command line prompt (DO NOT CHANGE) */
int verbose = 0;         /* if true, print additional output */
int nextjid = 1;         /* next job ID to allocate */
char sbuf[MAXLINE];      /* for composing sprintf messages */

struct job_t
{                          /* The job struct */
    pid_t pid;             /* job PID */
    int jid;               /* job ID [1, 2, ...] */
    int state;             /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE]; /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */

sigset_t global_mask_all, global_mask_one, global_prev_one;

/* End global variables */

/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv);
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid);
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

void Kill(pid_t pid, int signum);
pid_t Fork(void);
void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
void Sigemptyset(sigset_t *set);
void Sigfillset(sigset_t *set);
void Sigaddset(sigset_t *set, int signum);
struct job_t *get_job_from_argc(char *argc);
void clear(void);//not used, use system("clear") insted.

/**
 * my tool functions
 **/

void Kill(pid_t pid, int signum)
{
    int rc;

    if ((rc = kill(pid, signum)) < 0)
        unix_error("Kill error");
}

pid_t Fork(void)
{
    pid_t pid;

    if ((pid = fork()) < 0)
        unix_error("Fork error");
    return pid;
}

void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    if (sigprocmask(how, set, oldset) < 0)
        unix_error("Sigprocmask error");
    return;
}

void Sigemptyset(sigset_t *set)
{
    if (sigemptyset(set) < 0)
        unix_error("Sigemptyset error");
    return;
}

void Sigfillset(sigset_t *set)
{
    if (sigfillset(set) < 0)
        unix_error("Sigfillset error");
    return;
}

void Sigaddset(sigset_t *set, int signum)
{
    if (sigaddset(set, signum) < 0)
        unix_error("Sigaddset error");
    return;
}



void clear(void){
    struct winsize size;
    int i;  
    if (isatty(STDOUT_FILENO) == 0)  
        exit(1);  
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &size)<0) 
    {
        perror("ioctl TIOCGWINSZ error");
        exit(1);
    }
    for(i=0;i<size.ws_row;i++)
        printf("\n");
}

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv)
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF)
    {
        switch (c)
        {
        case 'h': /* print help message */
            usage();
            break;
        case 'v': /* emit additional diagnostic info */
            verbose = 1;
            break;
        case 'p':            /* don't print a prompt */
            emit_prompt = 0; /* handy for automatic testing */
            break;
        default:
            usage();
        }
    }

    /*init the global sigset_t var*/
    Sigfillset(&global_mask_all);
    Sigemptyset(&global_mask_one);
    Sigaddset(&global_mask_one, SIGCHLD);

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT, sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler); /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler); /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler);

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1)
    {
        /* Read command line */
        if (emit_prompt)
        {
            printf("%s", prompt);
            fflush(stdout);
        }

        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
        {
            app_error("fgets error");
        }
        if (feof(stdin))
        { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    }

    exit(0); /* control never reaches here */
}

/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline)
{
    char *argv[MAXARGS]; //将命令行输入的字符串解析放到字符串数组argv中
    char buf[MAXLINE];   //保存原始的命令字符串
    int bg;              //标志job运行于前台还是后台的标志
    pid_t pid;

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
        return;

    if (!builtin_cmd(argv)){
        Sigprocmask(SIG_BLOCK, &global_mask_one, &global_prev_one);
        if ((pid = Fork()) == 0){
            Sigprocmask(SIG_SETMASK, &global_prev_one, NULL);
            setpgid(0, 0); //创建一个以该子进程进程id为进程组id的进程组，然后将该子进程放入新创建的进程组。
            if (execve(argv[0], argv, environ) < 0){
                printf("%s: Command not found.\n", argv[0]);
                fflush(stdout);
                exit(0);
            }
        }
        int status = (bg) ? BG : FG;
        //无论是前台还是后台进程都要加入到job list中
        Sigprocmask(SIG_BLOCK, &global_mask_all, NULL);
        addjob(jobs, pid, status, cmdline);
        Sigprocmask(SIG_SETMASK, &global_prev_one, NULL);

        // if (!bg)
        // { //前台进程直接在此处回收,以阻塞主进程的执行

        //     if (waitpid(pid, NULL, 0) < 0)
        //         unix_error("waitfg: waitpid error");
        //     else
        //         deletejob(jobs, pid);
        // }
        if (!bg)
            waitfg(pid);
        else{ 
            fprintf(stdout, "[%d] (%d) %s", pid2jid(pid), pid, cmdline);
            fflush(stdout);
        }
    }
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv)
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf) - 1] = ' ';   /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'')
    {
        buf++;
        delim = strchr(buf, '\'');
    }
    else
    {
        delim = strchr(buf, ' ');
    }

    while (delim)
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* ignore spaces */
            buf++;

        if (*buf == '\'')
        {
            buf++;
            delim = strchr(buf, '\'');
        }
        else
        {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;

    if (argc == 0) /* ignore blank line */
        return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
    {
        argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */

int builtin_cmd(char **argv)
{
    if (!strcmp(argv[0], "quit"))
    { //quit the shell
        exit(0);
    }
    if (!strcmp(argv[0], "jobs"))
    {
        listjobs(jobs);
        return 1;
    }
    if ((!strcmp(argv[0], "bg")) || (!strcmp(argv[0], "fg")))
    {
        do_bgfg(argv);
        return 1;
    }
    if (!strcmp(argv[0], "clear"))
    {
        // clear();
        system("clear");
        return 1;
    }
    return 0; /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */

struct job_t *get_job_from_argc(char *argc)
{
    bool is_pid = ((argc[0] == '%') ? false : true);
    if (is_pid){
        pid_t pid = (pid_t)atoi(argc);
        if(!pid){
            printf("argument must be a PID or %%jobid\n");
            return NULL;
        }
        struct job_t *job = getjobpid(jobs, pid);
        if(job==NULL){
            printf("(%s): No such process\n",argc);
            return NULL;
        }
        return job;
    }
    else{
        char jid_dest[strlen(argc) - 1];
        strncpy(jid_dest, argc + 1, strlen(argc) - 1);
        int jid = (int)atoi(jid_dest);
        if(!jid){
            printf("argument must be a PID or %%jobid\n");
            return NULL;
        }
        struct job_t *job = getjobjid(jobs, jid);
        if(job==NULL){
            printf("%s: No such job\n",argc);
            return NULL;
        }
        return job;
    }
}

/**
 * bg <job>: Change a stopped background job to a running background job.
`* fg <job>: Change a stopped or running background job to a running in the foreground.
 **/
void do_bgfg(char **argv)
{
    if(argv[1]==NULL){
        printf("%s command requires PID or %%jobid argument\n",argv[0]);
        return;
    }
    if (!strcmp(argv[0], "bg"))
    {
        struct job_t *job = get_job_from_argc(argv[1]);
        if(!job)
            return;
        if (job->state == ST)
        {
            //    if (kill(job->pid, SIGCONT) < 0)
            //         fprintf(stderr, "bg Job[%d],Pid[%d] error\n",job->jid,job->pid);
            //     else
            //         job->state=BG;
            Kill(-(job->pid), SIGCONT);
            job->state = BG;
            fprintf(stdout, "[%d] (%d) %s", job->jid, job->pid, job->cmdline);
            fflush(stdout);

        }
        else
        {
            printf("Job[%d],Pid[%d]当前的状态不为stopped background job,不能对其进行'bg'操作\n", job->jid, job->pid);
            fflush(stdout);
        }
    }
    else if (!strcmp(argv[0], "fg"))
    {
        struct job_t *job = get_job_from_argc(argv[1]);
        if(!job)
            return;
        if (job->state == ST)
        {
            // if (kill(job->pid, SIGCONT) < 0)
            //     fprintf(stderr, "bg Job[%d],Pid[%d] error\n",job->jid,job->pid);
            // else{
            //     job->state=FG;
            //     waitfg(job->pid);//变成前台作业则需要等待它执行完毕
            // }
            Kill(-(job->pid), SIGCONT);
            job->state = FG;
            waitfg(job->pid);
        }
        else if (job->state == BG)
        {
            job->state = FG;
            waitfg(job->pid);
            //??? how to change a running backgroung job to a running foreground job
        }
        else
        {
            printf("Job[%d],Pid[%d]当前的状态不为stopped or running background job,不能对其进行'fg'操作\n", job->jid, job->pid);
            fflush(stdout);
        }
    }
    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */

/*另外一种想法，前台进程直接在主进程中用waitpid回收，然后从job list中删除掉对应的job
这样既阻塞了主进程的执行又回收了子进程，但是这种做法不够统一，统一的做法是将所有的子进程都
加入到job list中，然后回收僵尸进程的工作都留给SIGCHLD信号处理程序完成，等待前台进程的
方法如下*/
void waitfg(pid_t pid)
{
    while (pid == fgpid(jobs))
        sleep(0);
    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig)
{
    int olderron = errno;
    sigset_t mask_all, prev_all;
    pid_t pid;
    int status;

    Sigfillset(&mask_all);

    //回收子进程，从job列表中删除对应的job
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        if (WIFEXITED(status))
        { //正常终止
            //WEXITSTATUS(status)拿到终止状态
            Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            deletejob(jobs, pid);
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
        else if (WIFSIGNALED(status))
        {
            Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
            deletejob(jobs, pid);
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid, WTERMSIG(status));
            fflush(stdout);
        }
        else if (WIFSTOPPED(status))
        {
            struct job_t *job = getjobpid(jobs, pid);
            if (job != NULL)
                job->state = ST;
            printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid, WSTOPSIG(status));
        }
    }

    // if (errno != ECHILD){
    //     fprintf(stderr,"waitpid error");
    //     fflush(stderr);
    // }

    errno = olderron;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig)
{
    int olderrno = errno;
    pid_t current_fgpid = fgpid(jobs);

    if (current_fgpid == 0)
    { //当前没有前台进程，
        //no effect
    }
    else
    { //当前有前台进程，杀死该前台进程及其子进程
        Kill(-current_fgpid, SIGINT);
        //回收子进程的工作留给信号处理程序
    }

    errno = olderrno;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig)
{
    int olderrno = errno;
    pid_t current_fgpid = fgpid(jobs);

    if (current_fgpid == 0)
    { //当前没有前台进程，
        //no effect
    }
    else
    { //当前有前台进程，停止该进程组的所有进程
        Kill(-current_fgpid, SIGTSTP);
        // struct job_t* fgjob=getjobpid(jobs,current_fgpid);
        // fgjob->state=ST;
        // fprintf(stdout, "Job [%d] (%d) suspended by signal SIGTSTP\n", pid2jid(current_fgpid), current_fgpid);
        // fflush(stdout);
    }

    errno = olderrno;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job)
{
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs)
{
    int i, max = 0;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid > max)
            max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline)
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == 0)
        {
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(jobs[i].cmdline, cmdline);
            if (verbose)
            {
                printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid == pid)
        {
            clearjob(&jobs[i]);
            nextjid = maxjid(jobs) + 1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].state == FG)
            return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid)
{
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
            return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid)
{
    int i;

    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].jid == jid)
            return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid)
{
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].pid == pid)
        {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs)
{
    int i;

    for (i = 0; i < MAXJOBS; i++)
    {
        if (jobs[i].pid != 0)
        {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
            switch (jobs[i].state)
            {
            case BG:
                printf("Running ");
                break;
            case FG:
                printf("Foreground ");
                break;
            case ST:
                printf("Stopped ");
                break;
            default:
                printf("listjobs: Internal error: job[%d].state=%d ",
                       i, jobs[i].state);
            }
            printf("%s", jobs[i].cmdline);
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/

/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void)
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    fflush(stdout);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}
