#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#if 0
int main(int argc,char* argv[])
{
#if 1
	int fd;
        int c;
	 long ret;
	 fd = open("/dev/iobank", O_RDONLY);
	 if(fd) {
	   c = ioctl(fd, 0x90, 0);
	   if (c < 0)
		 printf("error: %d, errno: %d, meaning: %s\n", c, errno, strerror(errno));

#if 0	   
	   c = ioctl(fd, 0x90, 1);
	   if (c < 0)
		 printf("error: %d, errno: %d, meaning: %s\n", c, errno, strerror(errno));

	   
	   c = ioctl(fd, 0x90, 2);
	   if (c < 0)
		 printf("error: %d, errno: %d, meaning: %s\n", c, errno, strerror(errno));

	   
	   c = ioctl(fd, 0x90, 3);
	   if (c < 0)
		 printf("error: %d, errno: %d, meaning: %s\n", c, errno, strerror(errno));
#endif	   
	   
	   close(fd);
	 }
#else
    int fd;
    fd=open("/dev/hellp");
    if(fd) {
    printf("12312");
#endif
	 return;
}

//  mknod /dev/iobank c 239  10
#endif

#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>







#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>



#define BAUDRATE B115200
#define MODEMDEVICE "/dev/ttyS2"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE; 

// tinylogin getty -L 115200 ttyS1 vt100 

void signal_handler_IO (int status);   /* definition of signal handler */
///--int wait_flag=TRUE;                    /* TRUE while no signal received */
int wait_flag=FALSE;                    /* TRUE while no signal received */


/* Change this to whatever your daemon is called */
#define DAEMON_NAME "mydaemon"

/* Change this to the user under which to run */
#define RUN_AS_USER "daemon"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1


static void child_handler(int signum)
{
    switch(signum) {
    case SIGALRM: exit(EXIT_FAILURE); break;
    case SIGUSR1: exit(EXIT_SUCCESS); break;
    case SIGCHLD: exit(EXIT_FAILURE); break;
    }
}

static void daemonize( const char *lockfile )
{
    pid_t pid, sid, parent;
    int lfp = -1;

    /* already a daemon */
    if ( getppid() == 1 ) return;

    /* Create the lock file as the current user */
    if ( lockfile && lockfile[0] ) {
        lfp = open(lockfile,O_RDWR|O_CREAT,0640);
        if ( lfp < 0 ) {
            syslog( LOG_ERR, "unable to create lock file %s, code=%d (%s)",
                    lockfile, errno, strerror(errno) );
            exit(EXIT_FAILURE);
        }
    }

    /* Drop user if there is one, and we were run as root */
    if ( getuid() == 0 || geteuid() == 0 ) {
        struct passwd *pw = getpwnam(RUN_AS_USER);
        if ( pw ) {
            syslog( LOG_NOTICE, "setting user to " RUN_AS_USER );
            setuid( pw->pw_uid );
        }
    }

    /* Trap signals that we expect to recieve */
    signal(SIGCHLD,child_handler);
    signal(SIGUSR1,child_handler);
    signal(SIGALRM,child_handler);

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        syslog( LOG_ERR, "unable to fork daemon, code=%d (%s)",
                errno, strerror(errno) );
        exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0) {

        /* Wait for confirmation from the child via SIGTERM or SIGCHLD, or
           for two seconds to elapse (SIGALRM).  pause() should not return. */
        alarm(2);
        pause();

        exit(EXIT_FAILURE);
    }

    /* At this point we are executing as the child process */
    parent = getppid();

    /* Cancel certain signals */
    signal(SIGCHLD,SIG_DFL); /* A child process dies */
    signal(SIGTSTP,SIG_IGN); /* Various TTY signals */
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    signal(SIGHUP, SIG_IGN); /* Ignore hangup signal */
    signal(SIGTERM,SIG_DFL); /* Die on SIGTERM */

    /* Change the file mode mask */
    umask(0);

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        syslog( LOG_ERR, "unable to create a new session, code %d (%s)",
                errno, strerror(errno) );
        exit(EXIT_FAILURE);
    }

    /* Change the current working directory.  This prevents the current
       directory from being locked; hence not being able to remove it. */
    if ((chdir("/")) < 0) {
        syslog( LOG_ERR, "unable to change directory to %s, code %d (%s)",
                "/", errno, strerror(errno) );
        exit(EXIT_FAILURE);
    }

    /* Redirect standard files to /dev/null */
    freopen( "/dev/null", "r", stdin);
    freopen( "/dev/null", "w", stdout);
    freopen( "/dev/null", "w", stderr);

    /* Tell the parent process that we are A-okay */
    kill( parent, SIGUSR1 );
}

main()
{
int fd,c, res;
struct termios oldtio,newtio;
struct sigaction saio;           /* definition of signal action */
char buf[255];

printf("2222222222222222222\n");

///--daemonize( "/var/lock/subsys/" DAEMON_NAME );

/* open the device to be non-blocking (read will return immediatly) */
fd = open(MODEMDEVICE, O_RDWR | O_NOCTTY | O_NONBLOCK);
if (fd <0) {perror(MODEMDEVICE); exit(-1); }
printf("33333333333\n");

/* install the signal handler before making the device asynchronous */
saio.sa_handler = signal_handler_IO;
///--saio.sa_mask = 0;
(void)sigemptyset(&saio.sa_mask);
saio.sa_flags = 0;
saio.sa_restorer = NULL;
sigaction(SIGIO,&saio,NULL);
  
/* allow the process to receive SIGIO */
fcntl(fd, F_SETOWN, getpid());
/* Make the file descriptor asynchronous (the manual page says only 
   O_APPEND and O_NONBLOCK, will work with F_SETFL...) */
fcntl(fd, F_SETFL, FASYNC);

tcgetattr(fd,&oldtio); /* save current port settings */
/* set new port settings for canonical input processing */
newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
newtio.c_iflag = IGNPAR | ICRNL;
newtio.c_oflag = 0;
newtio.c_lflag = ICANON;
newtio.c_cc[VMIN]=1;
newtio.c_cc[VTIME]=0;
tcflush(fd, TCIFLUSH);
tcsetattr(fd,TCSANOW,&newtio);
 printf("1111111111111111111111111\n");
/* loop while waiting for input. normally we would do something
   useful here */ 
///--while (STOP==FALSE) {
while(1){
  //printf(".\n");usleep(100000);
  printf(".\n");
  write(fd,".\n",2); usleep(100000);
  /* after receiving SIGIO, wait_flag = FALSE, input is available
     and can be read */
  if (wait_flag==FALSE) { 
    res = read(fd,buf,255);
    buf[res]=0;
    //printf(":%s:%d\n", buf, res);
	write(fd,buf,res);
	
    if (res==1) STOP=TRUE; /* stop loop if only a CR was input */
    ///--wait_flag = TRUE;      /* wait for new input */
  }
}
/* restore old port settings */
tcsetattr(fd,TCSANOW,&oldtio);
}

/***************************************************************************
* signal handler. sets wait_flag to FALSE, to indicate above loop that     *
* characters have been received.                                           *
***************************************************************************/

void signal_handler_IO (int status)
{
printf("received SIGIO signal.\n");
wait_flag = FALSE;
}

