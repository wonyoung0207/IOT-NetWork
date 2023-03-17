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


struct card {//카드 하나를 의미함 -> 숫자와 모양으로 구성됨 
	int value; // 카드 숫자 (1~13)
	char suit;  // 카드 모양 -> 스페이드(s), 하트(h), 클로버(c), 다이아몬드(d)  ♡, ♧, ♤, ◇
};
struct gameInfo{ // manager와 player간 주고 받는 데이터 형식 
    struct card cards[52]; // player가 현재 소유한 카드 정보,//전역변수 : 카드 정보 
    int num_cards; // player가 현재 소유한 카드 개수
    struct card open_card; //현재 오픈되어 있는 카드, player는 오픈카드와 모양 또는 숫자를 낼 수 있다. 
    int manager_pid; // 매니저 프로세스의 pid
    int player_pid; // 플레이어(자신) 프로세스의 pid
};

// 카드 정보
struct card cards[52];//전역변수 
struct card open_card;//매니저가 오픈된 카드 정보를 알고있어야함 
int top = 0;

// 메세지 큐 key IDs, 메시지 큐(단방향)를 이용할 키값 
int msqid_p1_down; // manager -> player1
int msqid_p1_up; // player 1 -> manager
int msqid_p2_down;// manager -> player2
int msqid_p2_up; // player 1 -> manager2

// 플레이어들의 pid;
int p1_pid;
int p2_pid;


void make_cards(){//카드를 만드는 매소드 ->모양마다 13장씩 
    // make cards
    printf("Make cards");
	for (int i=0;i<13;i++) {
		cards[i].value=i%13+1;
		cards[i].suit='c';
	}
	for (int i=0;i<13;i++) {
		cards[i+13].value=i%13+1;
		cards[i+13].suit='d';
	}
	for (int i=0;i<13;i++) {
		cards[i+26].value=i%13+1;
		cards[i+26].suit='h';
	}
	for (int i=0;i<13;i++) {
		cards[i+39].value=i%13+1;
		cards[i+39].suit='s';
	}
    for (int i=0;i<52;i++) {
		printf("(%c,%d) ", cards[i].suit,cards[i].value);
	}
    printf("\n\n");
}
void shuffle(struct card *cards, int num){//0~51까지 랜덤한 숫자를 선택해서 섞는 것 
    //0~51까지 반복하면서 랜덤하게 선택된 카드와 위치를 바꾼다. 
    srand(time(NULL));
    struct card temp;
    int randInt;
    printf("Shuffling the cards\n");
    for (int i=0; i<num-1; i++){
        randInt = rand() % (num-i) + i;
        temp = cards[i];
        cards[i] = cards[randInt];
        cards[randInt] = temp;
    }
    for (int i=0;i<52;i++) {
		printf("(%c,%d) ", cards[i].suit,cards[i].value);//셔플된 상태를 확인 
	}
    printf("\n");
}

void childProcess(struct card send_card){//플레이어에게 보낼 카드 정보 
    struct card s;
    s.value = send_card.value;
    s.suit = send_card.suit;
    printf("open_card :%c\t %d\n",s.suit,s.value);
}

void turn_count(){
    printf("4턴이 지났습니다. ");
}

void stop_game(){//게임 일시정지 시간 -> 받는 프로세스 
    char buf[100] = "Stop Game!!! Please wait 3sec. ";

    if(mkfifo("myfifo",0777) == -1){//myfifo 파일의 권한을 모두에게 열수 있도록 하고, 파일 생성
        if(errno != EEXIST){
            printf("Creat fifo failed");
            exit(1);
        }
    }
    int fd = open("myfifo",O_WRONLY);//생성한 myfifo 파일을 열고
    if(write(fd,buf,sizeof(buf)) == -1){//파일에 쓴다. 
        return 2;
    }
    close(fd);
    printf("Restart Game!!!!");
}


int main(void) {
    // 카드 생성
    make_cards();
    // 카드 셔플
    shuffle(cards, 52);//랜덤한 숫자를 선택해서 섞는 것 
    printf("\n@@ cards setting---------------------------------\n");
    // message queue ID 확인
    key_t key_p1_down = 50001;
    key_t key_p1_up = 50002;
    key_t key_p2_down = 60001;
    key_t key_p2_up = 60002;
    // messge queue create
    if((msqid_p1_down=msgget(key_p1_down,IPC_CREAT|0666))==-1){return -1;}//서버가 p1에게 전송할 메시지 큐 생성 
    if((msqid_p1_up=msgget(key_p1_up,IPC_CREAT|0666))==-1){return -1;}//p1이 서버에게 전송할 메시지 큐 생성 
    if((msqid_p2_down=msgget(key_p2_down,IPC_CREAT|0666))==-1){return -1;}//서버가 p2에게 전송할 메시지 큐 생성 
    if((msqid_p2_up=msgget(key_p2_up,IPC_CREAT|0666))==-1){return -1;}//p2가 서버에게 전송할 메시지 큐 생성 
    // messge queue reset
    msgctl(key_p1_down, IPC_RMID, NULL);
    msgctl(key_p1_down, IPC_RMID, NULL);
    msgctl(key_p1_down, IPC_RMID, NULL);
    msgctl(key_p1_down, IPC_RMID, NULL);
    // messge queue create
    if((msqid_p1_down=msgget(key_p1_down,IPC_CREAT|0666))==-1){return -1;}
    if((msqid_p1_up=msgget(key_p1_up,IPC_CREAT|0666))==-1){return -1;}
    if((msqid_p2_down=msgget(key_p2_down,IPC_CREAT|0666))==-1){return -1;}
    if((msqid_p2_up=msgget(key_p2_up,IPC_CREAT|0666))==-1){return -1;}

    // manager와 player간 주고 받는 데이터 형식 
    struct gameInfo send_p1; // 플레이어 1에게 전달할 정보
    struct gameInfo send_p2; // 플레이어 2에게 전달할 정보
    struct gameInfo receive_p1; // 플레이어 1에게 전달받을 정보
    struct gameInfo receive_p2; // 플레이어 2에게 전달받을 정보

    // top에 위치한 카드 인덱스 표시
    top = 0;

    // 각 플레이어에게 전달할 초기 게임 정보 생성
    // player1,2 에게 처음에 5장의 카드 분배
    //player 1
    send_p1.num_cards = 0;//카드 주기전 초기화 
    send_p1.manager_pid = getpid();
    send_p1.player_pid = -1;
    for (int i=top; i<6; i++){ //5장의 카드를 p1에게 줌 
        send_p1.cards[i]= cards[i];
        send_p1.num_cards ++;
        top ++;
    } 
    // player 2
    send_p2.num_cards = 0;
    send_p2.manager_pid = getpid();
    send_p2.player_pid = -1;
    int i = 0;
    for (i=0; i < 6; i++){//5장의 카드를 p2에게 줌 
        send_p2.cards[i]= cards[i+top];
        send_p2.num_cards ++;   
    }
    top += i+1;//top에는 전체 카드에서 플레이어들에게 카드 나눠준 만큼 인덱스 증가시켜야 함 -> 따라서 남은카드 총 42장 중 1번째카드가 top에 있음 

    // open 카드 확인 및 전달할 정보에 표시
    open_card = cards[top]; //-> 처음에 오픈하는 카드, 이 카드를 시작으로 원카드 시작 
    send_p1.open_card = open_card;
    send_p2.open_card = open_card;
    top ++;

    // 게임 정보 전달
    printf("sending first game info\n");
    //msgsnd(큐 식별자, 전송할 데이터, 전송할 데이터 크기, 동작옵션) -> 성공 0, 실패 -1
    if(msgsnd(msqid_p1_down, &send_p1, sizeof(struct gameInfo), 0)==-1){return 0;};//player1에게 카드정보 전달 
    if(msgsnd(msqid_p2_down, &send_p2, sizeof(struct gameInfo), 0)==-1){return 0;};//player2에게 카드정보 전달 

    // 플레이어의 정보 수신 --> 플레이어의 pid 저장
    printf("receive the players pids\n");
    //msgrcv(큐 식별자, 전송받을 데이터, 전송받을 데이터 크기, 동작옵션)
    if(msgrcv(msqid_p1_up, &receive_p1, sizeof(struct gameInfo), 0, 0)==-1){return 1;};//manager에게 전송
    if(msgrcv(msqid_p2_up, &receive_p2, sizeof(struct gameInfo), 0, 0)==-1){return 1;};//manager에게 전송

    //get process ID of players -> player들의 process ID 받아옴
    p1_pid = receive_p1.player_pid;//전달받은 PID가 해당 플레이어의 process ID임 
    p2_pid = receive_p2.player_pid;

    int count = 0;
    signal(SIGALRM, turn_count);
    //signal(SIGTTOU), stop_game);
    // 게임 시작.
    while(1){
        ////////////////////////////////////////////////////////////////////////
        // Player 1의 차례//////////////////////////////////////////////////////
        //////////////////////////////////////////////////////////////////////
        if(count % 4 == 0){
            alarm(3);//3초뒤에 SIGALRM 호출 
            //일시정지 시간 
            //raise(SIGTTOU);
            
        }
        //sleep(2);//stopgame 의 오류를 방지하기 위해 2초간 정지 

        count++;//몇번째 턴인지 

        kill(p1_pid, SIGUSR1);//플레이어1에게 interrupt를 줘서 턴 시작 ,kill은 signal 신호를 보내는 것
        printf("Player 1's turn\n");
        // 1. 업데이트된 게임 정보 전달 (즉, 현재 오픈카드 정보)
        send_p1.open_card = open_card;
        
        //fork() 사용
        //fork 통해서 현재 프로세스와 같은 자식프로세스 만든 후 open_card정보 콘솔에 출력 -> pid = 0 이 자식프로세스가 된다. 
        pid_t child_pid;
        child_pid = fork();
        int status;
        //PIPE 사용 -> 하나의 프로세스의 출력을 다른 프로세스의 입력으로 연결하여 통신 -> 배열을이용 
        // 몇번째 턴인지 알려줌
        int fd[2];//f[1] 은 write할때, f[0]은 read할 떄 사용된다. 
        char count_turn_write[100];//몇번째 턴인지 저장 -> int형 count를 문자열로 바꿈 
        char count_turn_read[100];//몇번째 턴인지 읽어옴
        pipe(fd);//파이프 생성 
        sprintf(count_turn_write, "%d", count);

        if(child_pid == -1){
            perror("fork failed");
        }
        else if(child_pid !=0){
            //write(fd[1],count_turn_write,sizeof(count_turn_write));//파이프로 현재 몇번째 턴인지 데이터 보냄 
            child_pid = wait(&status);//고아 프로세스 방지 반환값으로 자식 프로세스ID 받음 -> 자식 끝날때까지 기다림 
            //execl("/home/b20155137/바탕화면/workspace/Project","card_print","NULL");//부모프로세스 실행중 다른 프로세스 생성하여 해당파일 실행 -> 부모 프로세스 종료시킴 
            //printf("**** NO Print Zone ****");
        }
        else{//부모프로세스가 먼저 실행되고 그다음 자식 프로세스가 실행된다.  -> pid가 0이 자식임 
            childProcess(open_card);

            // if(count % 4 == 0){//4턴이 지나면 출력 
            //     read(fd[0],count_turn_read,sizeof(count_turn_read));
            //     puts(count_turn_read);
            // }
        }
            // ******** popen을 이용한 pipe read ********
            // FILE *read_fp;
            // char count_turn_read[100];

            // if((read_fp = popen("명령어","r")) == NULL){
            //     perror("read popen failed");
            //     exit(1);
            // }

            // while(fgets(count_turn_read,sizeof(count_turn_read),read_fp)){
            //     fputs(count_turn_read,stdout);
            // }
            // pclose(read_fp);

            // ******** popen을 이용한 pipe write ********
            // if((write_fp = popen("명령어","w")) == NULL){
            //     perror("popen failed");
            //     exit(1);
            // }
            // fprintf(write_fp,count_turn_write);//해당경로의 파일에 turn 데이터 저장 
            // pclose(write_fp);
        

        

        
        // 1. 업데이트된 게임 정보 전달 (즉, 현재 오픈카드 정보)
        if(msgsnd(msqid_p1_down, &send_p1, sizeof(struct gameInfo), 0)==-1){return 0;};

        // 2. 플레이어 1의 게임 정보 수신 -> 여기서 manager은 플레이어1이 끝날떄까지 기다린다. 
        if(msgrcv(msqid_p1_up, &receive_p1, sizeof(struct gameInfo), 0, 0)==-1){return 1;};

        // 3. 게임 정보 업데이트 & 필요시 플레이어 1에게 새로운 카드 전달
        if (receive_p1.open_card.value == open_card.value && receive_p1.open_card.suit == open_card.suit){
            //manager가 보낸 카드정보가 플레이어1이 보낸 카드정보와 동일할 경우 플레이어1은 카드를 내려놓지 못한것임 
            //open 카드 정보가 같으면, 플레이어가 카드를 내려놓지 못한 것. --> 새로운 카드 전달.
            for(int i=0; i<receive_p1.num_cards; i++){
                send_p1.cards[i] = receive_p1.cards[i];
            }
            send_p1.cards[receive_p1.num_cards] = cards[top];
            send_p1.num_cards = receive_p1.num_cards+1;
            top ++;

            //여기서 플레이어1에게 manager가 새로운 카드 1장을 준다. 플레이어1은 오픈카드와 같은 카드가 없는것이기 떄문에 
            if(msgsnd(msqid_p1_down, &send_p1, sizeof(struct gameInfo), 0)==-1){return 0;};
        }
        else{
            //open 카드 정보가 다르면, 플레이어가 카드를 내려놓은 것. --> 현재 open 카드 정보 업데이트
            open_card = receive_p1.open_card;

        }

        // 4. 게임 종료 판단
        if (receive_p1.num_cards == 0){
            kill(p1_pid, SIGINT); // 승리//플레이어1에게 interrupt를 줘서 턴 시작 ,kill은 signal 신호를 보내는 것
            kill(p2_pid, SIGQUIT); // 패배
            printf("Player 1 Win\n");
            return(9);
        }
        if (receive_p1.num_cards > 20){
            kill(p1_pid, SIGQUIT); // 패배
            kill(p2_pid, SIGINT); // 승리
            printf("Player 2 Win\n");
            return(9);
        }
        if (top >= 51){
            //카드 다씀,
            if(receive_p1.num_cards > receive_p2.num_cards){
                kill(p1_pid, SIGINT); // 승리//플레이어1에게 interrupt를 줘서 턴 시작 ,kill은 signal 신호를 보내는 것
                kill(p2_pid, SIGQUIT); // 패배
            }
            else if(receive_p1.num_cards < receive_p2.num_cards){
                kill(p1_pid, SIGQUIT); // 패배//플레이어1에게 interrupt를 줘서 턴 시작 ,kill은 signal 신호를 보내는 것
                kill(p2_pid, SIGINT); // 승리
            }
            else{//무승부 
                kill(p2_pid, SIGILL);
                kill(p1_pid, SIGILL); 
            }

        }



        ////////////////////////////////////////////////////////////////////////
        // Player 2의 차례//////////////////////////////////////////////////////
        ///////////////////////////////////////////////////////////////////////
        count++;
        kill(p2_pid, SIGUSR1);//플레이어1에게 interrupt를 줘서 턴 시작 ,kill은 signal 신호를 보내는 것
        printf("Player 2's turn\n");

        // 1. 업데이트된 게임 정보 전달 (즉, 현재 오픈카드 정보)
        send_p2.open_card = open_card;
        if(msgsnd(msqid_p2_down, &send_p2, sizeof(struct gameInfo), 0)==-1){return 0;};

        // 2. 플레이어 2의 게임 정보 수신
        if(msgrcv(msqid_p2_up, &receive_p2, sizeof(struct gameInfo), 0, 0)==-1){return 1;};

        // 3. 게임 정보 업데이트 & 필요시 플레이어 2에게 새로운 카드 전달
        if (receive_p2.open_card.value == open_card.value && receive_p2.open_card.suit == open_card.suit){
            //open 카드 정보가 같으면, 플레이어가 카드를 내려놓지 못한 것. --> 새로운 카드 전달.
            for(int i=0; i<receive_p2.num_cards; i++){
                send_p2.cards[i] = receive_p2.cards[i];
            }
            send_p2.cards[receive_p2.num_cards] = cards[top];
            send_p2.num_cards = receive_p2.num_cards+1;
            top ++;
            if(msgsnd(msqid_p2_down, &send_p2, sizeof(struct gameInfo), 0)==-1){return 0;};
        }
        else{
            //open 카드 정보가 다르면, 플레이어가 카드를 내려놓은 것. --> 현재 open 카드 정보 업데이트
            open_card = receive_p2.open_card;

        }

        // 4. 게임 종료 판단
        if (receive_p2.num_cards == 0){
            kill(p2_pid, SIGINT); // 승리
            kill(p1_pid, SIGQUIT); // 패배
            printf("Player 2 Win\n");
            return(9);
        }
        if (receive_p2.num_cards > 20){
            kill(p2_pid, SIGQUIT); // 패배
            kill(p1_pid, SIGINT); // 승리
            printf("Player a Win\n");
            return(9);
        }
        if (top >= 51){
            //카드 다씀 
            if(receive_p1.num_cards > receive_p2.num_cards){
                kill(p1_pid, SIGINT); // 승리//플레이어1에게 interrupt를 줘서 턴 시작 ,kill은 signal 신호를 보내는 것
                kill(p2_pid, SIGQUIT); // 패배
            }
            else if(receive_p1.num_cards < receive_p2.num_cards){
                kill(p1_pid, SIGQUIT); // 패배//플레이어1에게 interrupt를 줘서 턴 시작 ,kill은 signal 신호를 보내는 것
                kill(p2_pid, SIGINT); // 승리
            }
            else{//카드 갯수 같을경우 무승부 
                kill(p2_pid, SIGILL);
                kill(p1_pid, SIGILL); 
            }
        }
    }
    return 9;
}
