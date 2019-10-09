#include <linux/init.h>  
#include <linux/module.h> 
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include<termios.h>
#include<sys/stat.h>
#include<fcntl.h>

#define BAUDRATE B115200
#define MODEMDEVICE "/dev/ttyS2"
#define R_BUF_LEN (256)
void printtid(void);  
void* com_read(void* pstatu)  
{  
    printtid();  
    int i=0;  
    int fd,c=0,num;  
    struct termios oldtio,newtio;  
    char buf[R_BUF_LEN];  
    printf("start.../n");  
    /*開啟PC機的COM1通訊埠*/
    fd=open(MODEMDEVICE,O_RDWR | O_NOCTTY | O_NONBLOCK/*| O_NDELAY*/);  
    if(fd<0)  
    {  
        perror(MODEMDEVICE);  
        exit(1);  
    }  
    printf("open .../n");  
    /*將目前終端機的結構儲存至oldtio結構*/
    tcgetattr(fd,&oldtio);  
    /*清除newtio結構，重新設定通訊協議*/
    bzero(&newtio,sizeof(newtio));  
    /*通訊協議設為8N1,8位資料位,N沒有效驗,1位結束位*/
    newtio.c_cflag = BAUDRATE |CS8|CLOCAL|CREAD;  
    newtio.c_iflag = IGNPAR;  
    newtio.c_oflag = 0;  
    /*設定為正規模式*/
    newtio.c_lflag=ICANON;  
    /*清除所有佇列在串列埠的輸入*/
    tcflush(fd,TCIFLUSH);   /*新的termios的結構作為通訊埠的引數*/
    tcsetattr(fd,TCSANOW,&newtio);  
    printf("reading.../n");  
    while(*(int*)pstatu)  
    {  
        num = read(fd,buf, R_BUF_LEN);  
        buf[R_BUF_LEN-1] = 0;  
        if(num > 0 && num <= R_BUF_LEN)  
        {   
            buf[num]=0;  
            printf("%s", buf);  
            fflush(stdout);  
        }  
    }  
    printf("close.../n");  
    close(fd);  
    /*恢復舊的通訊埠引數*/
    tcsetattr(fd,TCSANOW,&oldtio);  
}  
void* com_send(void* p)  
{  
    printtid();  
    int fd,c=0;  
    struct termios oldtio,newtio;  
    char ch;  
    staticchar s1[20];  
    printf("Start.../n ");  
    /*開啟arm平臺的COM1通訊埠*/
    fd=open(MODEMDEVICE,O_RDWR | O_NOCTTY);  
    if(fd<0)  
    {  
        perror(MODEMDEVICE);  
        exit(1);  
    }  
    printf(" Open.../n ");  
     /*將目前終端機的結構儲存至oldtio結構*/
       tcgetattr(fd,&oldtio);  
    /*清除newtio結構，重新設定通訊協議*/
    bzero(&newtio,sizeof(newtio));  
    /*通訊協議設為8N1*/
    newtio.c_cflag =BAUDRATE |CS8|CLOCAL|CREAD; //波特率 8個數據位 本地連線 接受使能
    newtio.c_iflag=IGNPAR;                      //忽略奇偶校驗錯誤
    newtio.c_oflag=0;  
    /*設定為正規模式*/
    newtio.c_lflag=ICANON;                     //規範輸入
    /*清除所有佇列在串列埠的輸出*/
    tcflush(fd,TCOFLUSH);  
    /*新的termios的結構作為通訊埠的引數*/
    tcsetattr(fd,TCSANOW,&newtio);  
    printf("Writing.../n ");  
    ///*
    while(*(char*)p != 0)  
    {  
        int res = 0;  
        res = write(fd,(char*)p, 1);  
        if(res != 1) printf("send %c error/n", *(char*)p);  
        else printf("send %c ok/n", *(char*)p);  
        ++p;  
    }  
    printf("Close.../n");  
    close(fd);  
    /*還原舊的通訊埠引數*/  
    tcsetattr(fd,TCSANOW,&oldtio);  
    printf("leave send thread/n");  
}  
/* 
    開始執行緒 
    thread_fun  執行緒函式 
    pthread     執行緒函式所在pthread變數 
    par     執行緒函式引數 
    COM_STATU   執行緒函式狀態控制變數 1:執行 0:退出 
*/
int start_thread_func(void*(*func)(void*), pthread_t* pthread, void* par, int* COM_STATU)  
{  
    *COM_STATU = 1;  
    memset(pthread, 0, sizeof(pthread_t));  
    int temp;  
        /*建立執行緒*/
        if((temp = pthread_create(pthread, NULL, func, par)) != 0)  
        printf("執行緒建立失敗!/n");  
        else
    {  
        int id = pthread_self();  
                printf("執行緒%u被建立/n", *pthread);  
    }  
    return temp;  
}  
/* 
    結束執行緒 
    pthread     執行緒函式所在pthread變數 
    COM_STATU   執行緒函式狀態控制變數 1:執行 0:退出 
*/
int stop_thread_func(pthread_t* pthread, int* COM_STATU)  
{  
    printf("prepare stop thread %u/n", *pthread);  
    *COM_STATU = 0;  
    if(*pthread !=0)   
    {  
                pthread_join(*pthread, NULL);  
    }  
    printf("執行緒%d退出!/n", *COM_STATU);  
}  
void printtid(void)  
{  
    int id = pthread_self();  
        printf("in thread %u/n", id);  
}  
int main()  
{  
    pthread_t thread[2];  
    printtid();  
    constint READ_THREAD_ID = 0;  
    constint SEND_THREAD_ID = 1;  
    int COM_READ_STATU = 0;  
    int COM_SEND_STATU = 0;  
    if(start_thread_func(com_read, &thread[READ_THREAD_ID],  &COM_READ_STATU, &COM_READ_STATU) != 0)  
    {  
        printf("error to leave/n");  
        return -1;  
    }  
    printf("wait 3 sec/n");  
    sleep(3);  
    printf("wake after 3 sec/n");  
    if(start_thread_func(com_send, &thread[SEND_THREAD_ID], "ABCDEFGHIJKLMNOPQRST", &COM_SEND_STATU) != 0)  
    {  
        printf("error to leave/n");  
        return -1;  
    }  
    printtid();  