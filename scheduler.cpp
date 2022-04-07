//
// Created by Shane on 2021/4/11.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>

using namespace std;

//Named Constant
#define SCHEDULER_TCP_PORT 34165     // the port for TCP with client
#define SCHEDULER_UDP_PORT 33165 // the port use UDP with hospital
#define A_UDP_PORT 30165	   // the port users will be connecting to Hospital A
#define B_UDP_PORT 31165	   // the port users will be connecting to Hospital B
#define C_UDP_PORT 32165	   // the port users will be connecting to Hospital C
#define LOCAL_HOST "127.0.0.1"  // local IP address
#define MAXSIZE 1024 // max number of bytes we can get at once

//Define the message struct
struct AssignmentQueryInfo
{
    int vertex_idx;
};
struct Ini_status
{
    int hospital_loc_idx;
    int total_capacity;
    int ini_occupancy;
};
struct Status_book_keep
{
    bool ini_flag;
    int capacity;
    int occupation;
    float availability;
};
struct ScoreQueryInfo
{
    // assignment_flag = 0, query score
    //                 = 1, assignment to this hospital
    int assignment_flag;
    int input_vertex_idx;
};
struct ScoreResponseInfo
{
    char hospital_idx;
    float distance;
    float score;
};
struct AssignmentResponseInfo
{
    // if location is illegal, hospital_idx = 'N'
    char hospital_idx;
    bool located_in_map;
    float corresp_score;
};
struct Scores
{
    // score = -1 aka none
    // distance = 0 aka none
    float A_score;
    float A_distance;
    float B_score;
    float B_distance;
    float C_score;
    float C_distance;

};

//Defined global variables
int scheduler_TCP_sockfd = 0, TCP_recv_sockfd = 0; // Parent socket and Child socket for TCP
int scheduler_UDP_sockfd = 0; // UDP socket
struct sockaddr_in scheduler_TCP_addr = {0}, scheduler_UDP_addr = {0}; // local addresses of scheduler
struct sockaddr_in UDP_remote_addr = {0}, TCP_remote_addr = {0}; // client address
struct Status_book_keep hospital_A, hospital_B, hospital_C;
struct Ini_status iniStatus;
struct AssignmentQueryInfo ass_input;
struct ScoreQueryInfo score_query_info;
struct ScoreResponseInfo score_received_info;
struct Scores scores;
struct AssignmentResponseInfo ass_output;

//Defined functions
void boot_up();
void ini_process(); // initial the book keep for each hospital
void send_to_hospital(int port_number); // send client location to the hospital
void inform_hospital(int port_number); // inform the hospital when assignment has done
void receive_from_hospital(); // Receive score from the hospital
void assignment();

//*** Beej Guide ***
//create UDP and TCP socket for further using
void boot_up() {
    // create a udp socket, act as udp server
    scheduler_UDP_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (scheduler_UDP_sockfd == -1) {
        perror("[ERROR] Scheduler: fail to CREAT UDP socket");
        exit(1);
    }
    memset(&scheduler_UDP_addr, 0, sizeof scheduler_UDP_addr); // make sure the struct is empty
    scheduler_UDP_addr.sin_family = AF_INET; // IPv4 address
    scheduler_UDP_addr.sin_port = htons(SCHEDULER_UDP_PORT); // Scheduler port for UDP
    scheduler_UDP_addr.sin_addr.s_addr = inet_addr(LOCAL_HOST); // Host IP address

    // bind the created udp socket with scheduler_UDP_addr, act as tcp server
    if ((bind(scheduler_UDP_sockfd, (struct sockaddr*) &scheduler_UDP_addr, sizeof scheduler_UDP_addr))== -1) {
        perror("[ERROR] Scheduler: fail to BIND UDP socket");
        exit(1);
    }

    scheduler_TCP_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (scheduler_TCP_sockfd == -1) {
        perror("[ERROR] Scheduler: fail to CREAT TCP socket for client");
        exit(1);
    }
    scheduler_TCP_addr.sin_family = AF_INET;
    scheduler_TCP_addr.sin_port = htons(SCHEDULER_TCP_PORT);
    scheduler_TCP_addr.sin_addr.s_addr = inet_addr(LOCAL_HOST);

    // bind the created TCP socket with local addr
    int bind_flag = bind(scheduler_TCP_sockfd, (struct sockaddr*)&scheduler_TCP_addr, sizeof(scheduler_TCP_addr));
    if(bind_flag == -1)
    {
        perror("[ERROR] Scheduler: fail to BIND TCP socket for client \n");
        exit(1);
    }else{
        printf("The Scheduler is up and running \n");
    }
}

//receive the initial message from hospital and store these info into 'ini_status'
void ini_process(){
    hospital_A.ini_flag = false;
    hospital_B.ini_flag = false;
    hospital_C.ini_flag = false;
    int i = 0;
    while( i < 3){
        char recv_buf[MAXSIZE] = {0};
        memset(recv_buf,0,MAXSIZE);
        socklen_t UDP_remote_addr_len = sizeof UDP_remote_addr;
        if(recvfrom(scheduler_UDP_sockfd,recv_buf, MAXSIZE, 0, (struct sockaddr *)&UDP_remote_addr,&UDP_remote_addr_len) == -1){
            perror("[ERROR] Scheduler: fail to RECEIVE TCP socket for hospital \n");
            exit(1);
        }
        memcpy(&iniStatus, recv_buf, sizeof iniStatus);
        char hospital;
        switch(iniStatus.hospital_loc_idx){
            case 1:
                // Hospital A
                hospital = 'A';
                hospital_A.ini_flag = true;
                hospital_A.capacity = iniStatus.total_capacity;
                hospital_A.occupation = iniStatus.ini_occupancy;
                hospital_A.availability = (float) (hospital_A.capacity - hospital_A.occupation) / hospital_A.capacity;
                break;
            case 2:
                hospital = 'B';
                hospital_B.ini_flag = true;
                hospital_B.capacity = iniStatus.total_capacity;
                hospital_B.occupation = iniStatus.ini_occupancy;
                hospital_B.availability = (float) (hospital_B.capacity - hospital_B.occupation) / hospital_B.capacity;
                break;
            case 3:
                hospital = 'C';
                hospital_C.ini_flag = true;
                hospital_C.capacity = iniStatus.total_capacity;
                hospital_C.occupation = iniStatus.ini_occupancy;
                hospital_C.availability = (float) (hospital_C.capacity - hospital_C.occupation) / hospital_C.capacity;
                break;
        }
        printf("The Scheduler has received information from Hospital %c: total capacity is %d, and initial occupancy is %d \n", hospital, iniStatus.total_capacity, iniStatus.ini_occupancy);
        i++;
    }
}

//send message to the hospital specified by the input parameter (port_number)
void send_to_hospital(int port_number) {
    // build score query information by ass_input(received from TCP socket)
    char send_buff[MAXSIZE];
    score_query_info.assignment_flag = 0; // score query
    score_query_info.input_vertex_idx = ass_input.vertex_idx;
    memset(send_buff, 0, MAXSIZE);
    memcpy(send_buff, &score_query_info, sizeof(score_query_info));

    // send message to corresponding hospital via udp
    memset(&UDP_remote_addr,0, sizeof UDP_remote_addr);
    UDP_remote_addr.sin_family = AF_INET;
    UDP_remote_addr.sin_port = htons(port_number);
    UDP_remote_addr.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    socklen_t UDP_remote_addr_len = sizeof(UDP_remote_addr);
    if(sendto(scheduler_UDP_sockfd, send_buff, sizeof(send_buff), 0, (struct sockaddr *)&UDP_remote_addr, UDP_remote_addr_len) == -1){
        perror("[ERROR] Scheduler: fail to SENT UDP socket to Hospitals \n");
        exit(1);
    } else {
        // print onscreen
        char hospital;
        switch(port_number){
            case A_UDP_PORT:
                hospital = 'A';
                break;
            case B_UDP_PORT:
                hospital = 'B';
                break;
            case C_UDP_PORT:
                hospital = 'C';
                break;
        }
        printf("The Scheduler has sent client location to Hospital %c using UDP over port %d \n", hospital, SCHEDULER_UDP_PORT);
    }

}

//send assignment result to the hospital which is assigned to the client
void inform_hospital(int port_number) {
    char send_buff[MAXSIZE];
    score_query_info.assignment_flag = 1; // assignment to this hospital
    memset(send_buff, 0, MAXSIZE);
    memcpy(send_buff, &score_query_info, sizeof(score_query_info));
    // send message to corresponding hospital via udp
    memset(&UDP_remote_addr,0, sizeof UDP_remote_addr);
    UDP_remote_addr.sin_family = AF_INET;
    UDP_remote_addr.sin_port = htons(port_number);
    UDP_remote_addr.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    sendto(scheduler_UDP_sockfd, (void*) &send_buff, sizeof(send_buff), 0, (struct sockaddr *) &UDP_remote_addr, sizeof UDP_remote_addr);
    printf("The Scheduler has sent the result to Hospital %c using UDP over port %d \n", ass_output.hospital_idx, SCHEDULER_UDP_PORT);
}

//receive the UDP socket from hospital
void receive_from_hospital() {
    // build the container that will receive the msg from hospital
    char recv_buff[MAXSIZE] = {0};
    memset(recv_buff,0,MAXSIZE);
    socklen_t UDP_remote_addr_len = sizeof UDP_remote_addr;
    if(recvfrom(scheduler_UDP_sockfd,recv_buff, MAXSIZE, 0, (struct sockaddr *)&UDP_remote_addr,&UDP_remote_addr_len) == -1){
        perror("[ERROR] Scheduler: fail to RECEIVE TCP socket for hospital \n");
        exit(1);
    }

    memset(&score_received_info, 0, sizeof score_received_info);
    memcpy(&score_received_info, recv_buff, sizeof score_received_info);
    // put the score into struct scores for further operation
    char hospital;
    float returned_score = score_received_info.score;
    float returned_distance = score_received_info.distance;
    switch(score_received_info.hospital_idx){
        case 'A':
            hospital = 'A';
            scores.A_score = returned_score;
            scores.A_distance = returned_distance;
            break;
        case 'B':
            hospital = 'B';
            scores.B_score = returned_score;
            scores.B_distance = returned_distance;
            break;
        case 'C':
            hospital = 'C';
            scores.C_score = returned_score;
            scores.C_distance = returned_distance;
            break;
    }
    // output onscreen message
    if (returned_distance == -1){
        printf("The Scheduler has received map information from Hospital %c, the score = None and the distance = None \n", hospital);
    } else {
        printf("The Scheduler has received map information from Hospital %c, the score = %f and the distance = %f \n", hospital, returned_score, returned_distance);
    }

}

//make assignment by receiving scores and distance
void assignment(){
    int assignment_result;
    float max_score;
    float min_dist;
    float score_array[3] = {scores.A_score, scores.B_score, scores.C_score};
    float dist_array[3] = {scores.A_distance, scores.B_distance, scores.C_distance};
    // fill the ass_output
    if(scores.A_distance < 0 || scores.B_distance < 0 || scores.C_distance < 0 ){
        // the location is illegal
        // reply None
        ass_output.located_in_map = false;
        ass_output.hospital_idx = 'N';
        printf("The Scheduler has assigned None to the client. \n");
    } else{
        ass_output.located_in_map = true;
        max_score = score_array[0];
        min_dist = dist_array[0];
        assignment_result = 1;
        for (int i = 1; i < 3; ++i) {
            if(score_array[i] == max_score) {
                if (dist_array[i] < min_dist) {
                    min_dist = dist_array[i];
                    assignment_result = i + 1;
                }
            }
            if(score_array[i] > max_score) {
                    max_score = score_array[i];
                    min_dist = dist_array[i];
                    assignment_result = i + 1;
            }
        }
        if(max_score == 0 || !isfinite(max_score)){
            // all scores == None,reply None
            // score = inf, aka the client is already at hospital
            ass_output.hospital_idx = 'N';
            printf("The Scheduler has assigned None to the client. \n");
        } else{
            ass_output.corresp_score = max_score;
            switch (assignment_result) {
                case 1:
                    ass_output.hospital_idx = 'A';
                    printf("The Scheduler has assigned Hospital A to the client. \n");
                    break;
                case 2:
                    ass_output.hospital_idx = 'B';
                    printf("The Scheduler has assigned Hospital B to the client. \n");
                    break;
                case 3:
                    ass_output.hospital_idx = 'C';
                    printf("The Scheduler has assigned Hospital C to the client. \n");
                    break;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    // Phase 1: Boot-up
    boot_up();
    // Step3, receive UDP sockets(the initial status) from hospitals
    ini_process();
    bool ini_flag = hospital_A.ini_flag && hospital_B.ini_flag && hospital_C.ini_flag;
    if (ini_flag = false){
        perror("[ERROR] Scheduler server: fail to INITIAL all the hospitals \n");
        exit(1);
    }
    // Step4, listen for TCP connection from Client, queue limit is 5
    if (listen(scheduler_TCP_sockfd, SOMAXCONN) == -1) {
        perror("[ERROR] Scheduler server: fail to LISTEN for client socket \n");
        exit(1);
    }
    // Phase 2: Forward
    while (true) {
        // Step1: accept incoming TCP socket
        socklen_t TCP_remote_addr_len = sizeof(TCP_remote_addr);
        TCP_recv_sockfd = accept(scheduler_TCP_sockfd, (struct sockaddr *) &TCP_remote_addr, &TCP_remote_addr_len);
        if (TCP_recv_sockfd == -1) {
            perror("[ERROR] Scheduler server: fail to ACCEPT for client socket");
            exit(1);
        }
        // Step2: receive message form Client and then put it in ass_input
        char recv_TCP_buff[MAXSIZE];
        memset(recv_TCP_buff, 0, MAXSIZE);
        if (int read_bytes = recv(TCP_recv_sockfd, recv_TCP_buff, sizeof(recv_TCP_buff), 0) < 0) {
            perror("[ERROR] Scheduler server: fail to READ the client socket");
            exit(1);
        }
        memset(&ass_input, 0, sizeof(ass_input));
        memcpy(&ass_input, recv_TCP_buff, sizeof(recv_TCP_buff));
        printf("The Scheduler has received client at location %d from the client using TCP over port %d \n",
               ass_input.vertex_idx, SCHEDULER_TCP_PORT);
        // Step3: send it using UDP to hospitals by their availability, act as UDP client
        //      clear the scores
        //      returned score can not be 0, so we can initialize it as 0
        //      returned distance can not be 0. if so, the score would be inf. Thus, we can initialize it as 0
        scores.A_score = scores.B_score = scores.C_score = 0;
        scores.A_distance = scores.B_distance = scores.C_distance = 0;
        int send_num = 0;
        if (hospital_A.availability > 0) {
            // send to A
            send_to_hospital(A_UDP_PORT);
            while(true) {
                receive_from_hospital();
                break;
            }
        }
        if (hospital_B.availability > 0) {
            // send to B
            send_to_hospital(B_UDP_PORT);
            while(true) {
                receive_from_hospital();
                break;
            }
        }
        if (hospital_C.availability > 0) {
            // send to C
            send_to_hospital(C_UDP_PORT);
            while(true) {
                receive_from_hospital();
                break;
            }
        }
        // Phase 4: reply
        // Step1: chose by returned info
        //      initialize ass_output
        ass_output.hospital_idx = 'Z'; // 'Z' for initialization, 'N' for reply None
        ass_output.located_in_map = false;
        ass_output.corresp_score = 0;
        assignment();
        // Step2: inform client, then close the child socket
        char send_TCP_buff[MAXSIZE];
        memset(send_TCP_buff, 0, MAXSIZE);
        memcpy(send_TCP_buff, &ass_output, sizeof(ass_output));
        if (send(TCP_recv_sockfd, send_TCP_buff, sizeof(send_TCP_buff), 0) == -1) {
            perror("[ERROR] Scheduler: TCP socket SEND Error");
            exit(1);
        } else {
            printf("The Scheduler has sent the result to client using TCP over port %d \n", SCHEDULER_TCP_PORT);
        }
        close(TCP_recv_sockfd);
        // Step3: inform the hospital
        // Step4: change the book-keep status
        switch (ass_output.hospital_idx) {
            case 'A':
                hospital_A.occupation++;
                hospital_A.availability = (float) (hospital_A.capacity - hospital_A.occupation) / hospital_A.capacity;
                inform_hospital(A_UDP_PORT);
                break;
            case 'B':
                hospital_B.occupation++;
                hospital_B.availability = (float) (hospital_B.capacity - hospital_B.occupation) / hospital_B.capacity;
                inform_hospital(B_UDP_PORT);
                break;
            case 'C':
                hospital_C.occupation++;
                hospital_C.availability = (float) (hospital_C.capacity - hospital_C.occupation) / hospital_C.capacity;
                inform_hospital(C_UDP_PORT);
                break;
        }
        printf("-------------------------------------------------------------------- \n");
    }
    // Step5: close TCP parent socket
    close(scheduler_TCP_sockfd);
    return 0;
}
