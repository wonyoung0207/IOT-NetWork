#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>

struct card {
	int value; // 카드 숫자 (1~13)
	char suit; // 카드 문양
};
struct gameInfo{ // manager와 player간 주고 받는 데이터 형식 
    struct card cards[52]; // player가 현재 소유한 카드 정보
    int num_cards; // player가 현재 소유한 카드 개수
    struct card open_card; // 현재 open card 정보
    int manager_pid; // 데이터를 보내는 프로세스의 pid
    int player_pid; // 데이터를 받는 프로세스의 pid
};

void my_turn(int sig){
    printf("Its your turn\n");
}
void win_sig(int sig){
    printf("You are the winner!\n");
    exit(0);
}
void lose_sig(int sig){
    printf("You are the loser...\n");
    exit(0);
}
void tie_sig(int sig){
    printf("Its tie....\n");
    exit(0);
}


int main(void) {
    // message queue setting
    struct gameInfo my_info;
    struct gameInfo received_info;
    int msqid_down;
    int msqid_up;
    key_t key_down = 50001;
    key_t key_up = 50002;
    if((msqid_down=msgget(key_down,IPC_CREAT|0666))==-1){return -1;}
    if((msqid_up=msgget(key_up,IPC_CREAT|0666))==-1){return -1;}

    // 초기 정보 수신
    if(msgrcv(msqid_down, &my_info, sizeof(struct gameInfo), 0, 0)==-1){return 1;}
    for (int i=0; i<my_info.num_cards; i++) {
                printf("%d:(%c,%d), ", i, my_info.cards[i].suit, my_info.cards[i].value);
    }
    printf("\n You got six cards.\n");

    my_info.player_pid = getpid();  
    // 나의 프로세스 id 전달
    if(msgsnd(msqid_up, &my_info, sizeof(struct gameInfo), 0)==-1){return 0;}

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////play game//////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////
    printf("Game Strats \n");
    signal(SIGINT, win_sig); // 승리 시그널
    signal(SIGQUIT, lose_sig); //패배 시그널
    signal(SIGILL, tie_sig); //패배 시그널

    while(1){
        // 시그널 올 때까지 대기
        signal(SIGUSR1, my_turn);
        pause();
        // 현재 정보 수신  //msgrcv(큐 식별자, 전송받을 데이터, 전송받을 데이터 크기, 동작옵션)
        if(msgrcv(msqid_down, &received_info, sizeof(struct gameInfo), 0, 0)==-1){return 1;}
        my_info.open_card = received_info.open_card;
        printf("Current Open Card: (%c,%d)\n", my_info.open_card.suit, my_info.open_card.value);
        // 현재 내 카드 정보 출력
        printf("You have %d cards\n", my_info.num_cards);//카드 몇장있는지 
        printf("Curren Cards List\n");
        for (int i=0; i<my_info.num_cards; i++) {//현재 무슨 카드 있는지 다 보여줌 
                printf("%d:(%c,%d), ", i, my_info.cards[i].suit, my_info.cards[i].value);r
        }
        printf("\n");
        // 드롭할 카드 선택
        int idx;
        printf("Select card index: ");
        scanf("%d", &idx);//카드 입력받음 
        if (my_info.cards[idx].suit==my_info.open_card.suit || my_info.cards[idx].value==my_info.open_cad.value){
            //입력한 카드랑 오픈 카드랑 같은지 비교 , 모양이나 숫자 같으면 실행 
            //카드 드롭
            if (my_info.num_cards == 1){
                // idx 0, num cards가 1일때 오류 발생하는거 방지. (my_info.cards 정리하며 오류 발생.)
                my_info.num_cards=0;
            }
            else{

                struct card temp = my_info.cards[idx];
                for (int i=idx; i < my_info.num_cards; i++){ // 카드 리스트에서 드롭할 카드 빼기
                    my_info.cards[i].value = my_info.cards[i+1].value;
                    my_info.cards[i].suit = my_info.cards[i+1].suit;
                }
                my_info.num_cards -= 1;
                my_info.open_card = temp;
                printf("Card (%c,%d) is dropped\n", temp.suit, temp.value);// 드롭한 카드 정보 
                
            }
            // 업데이트 된 정보 전송 
            if(msgsnd(msqid_up, &my_info, sizeof(struct gameInfo), 0)==-1){return 0;};//서버로 내 전체 카드 정보 전송 
        }
        else{//입력한 카드랑 오픈 카드랑 다를경우 실행 
            //카드 드롭 X
            printf("You cannot drop this card. You will have another card.\n");
            // 업데이트 된 정보 전송
            if(msgsnd(msqid_up, &my_info, sizeof(struct gameInfo), 0)==-1){return 0;};
            // 새로운 카드가 포함된 정보 수신. -> 수신시 한장의 카드가 추가된 카드정보를 받아온다. 
            if(msgrcv(msqid_down, &received_info, sizeof(struct gameInfo), 0, 0)==-1){return 1;}
            my_info.num_cards = received_info.num_cards;
            for (int i=0; i < my_info.num_cards; i++){ // 카드정보 갱신
                my_info.cards[i].value = received_info.cards[i].value;
                my_info.cards[i].suit = received_info.cards[i].suit;
                printf("Card (%c,%d) is added\n", my_info.cards[i].suit, my_info.cards[i].value);
            }
        }
        printf("Your turn is over. Waiting for the next turn.\n");
        printf("--------------------------------------------------\n");
    }
    return 0;
}

