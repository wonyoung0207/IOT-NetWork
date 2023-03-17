#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>//랜덤한 것을 생성하기 위해 
#include <sys/types.h>
#include <sys/ipc.h>//메시지큐를 이용하기 위해 사용 
#include <sys/msg.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

int main(int argc, char *argv[]){//stop game 됐을때 실행시킬 파일 

    int fd = open("myfifo",O_RDONLY);
    char buf[100] = {'\0'};
    for(int i =0; i<100;i++){
        if(read(fd,buf[i],sizeof(buf)) == -1){
            return 2;
        }
        printf("%c",buf[i]);
    }

    close(fd);
    return 0;
}