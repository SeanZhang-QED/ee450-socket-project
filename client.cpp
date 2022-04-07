//
// Created by Shane on 2021/4/10.
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
#define LOCAL_HOST "127.0.0.1"
#define SCHEDULER_TCP_PORT 34165

//Define the message struct
struct AssignmentQueryInfo
{
    int vertex_idx;
};
struct AssignmentResponseInfo
{
    char hospital_idx;
    bool located_in_map;
    float corresp_score;
};

//Define global variables
struct sockaddr_in scheduler_addr; //scheduler server address
int client_TCP_socketfd = 0; // Client socket
struct AssignmentQueryInfo ass_info;
struct AssignmentResponseInfo ass_res_info;

//defined functions
void show_response(AssignmentResponseInfo);
//print the assignment result of client
void show_response(AssignmentResponseInfo received_info) {
    if (!received_info.located_in_map){
        printf("Location %d not found\n", ass_info.vertex_idx);
    } else if (received_info.corresp_score == 0 || !isfinite(received_info.corresp_score)) {
        printf("The client has received results from the Scheduler: No assignment. \n");
    }  else{
        printf("The client has received results from the Scheduler: assigned to Hospital %c\n", received_info.hospital_idx);
    }
}

int main(int argc,char *argv[]){
    int client_vertex_idx = atoi(argv[1]);
    ass_info.vertex_idx = client_vertex_idx;
    // boot up section
    client_TCP_socketfd = socket(AF_INET, SOCK_STREAM,0);
    if(client_TCP_socketfd>= 0)
    {
        printf("The client is up and running.\n");
    } else {
        perror("[ERROR] Client: fail to CREAT TCP socket \n");
    }

    scheduler_addr.sin_family = AF_INET;
    scheduler_addr.sin_port  = htons(SCHEDULER_TCP_PORT);
    scheduler_addr.sin_addr.s_addr = inet_addr(LOCAL_HOST);

    // TCP connection
    // *** Beej Guide ***
    if ((connect(client_TCP_socketfd, (struct sockaddr*)&scheduler_addr, sizeof(scheduler_addr))) == -1){
        perror("[ERROR] Client: fail to CONNECT with Scheduler \n");
        exit(1);
    }

    // send client location to scheduler
    // *** Beej Guide ***
    char send_buf[1024];
    memset(send_buf, 0, 1024);
    memcpy(send_buf, &ass_info, sizeof(ass_info));
    if (send(client_TCP_socketfd, send_buf, sizeof send_buf, 0) == -1) {
        perror("[ERROR] Client: TCP socket SEND Error \n");
        close(client_TCP_socketfd);
        exit(1);
    } else{
        printf("The client has sent query to Scheduler using TCP: client location %d \n", client_vertex_idx);
    }

    // receive result from scheduler
    // *** Beej Guide ***
    char recv_buf[1024];
    memset(recv_buf, 0, 1024);
    if (recv(client_TCP_socketfd, recv_buf, sizeof(recv_buf), 0) == -1) {
        perror("[ERROR] Client: fail to RECEIVE TCP socket from Scheduler \n");
        exit(1);
    }
    memset(&ass_res_info, 0, sizeof ass_res_info);
    memcpy(&ass_res_info, recv_buf, sizeof ass_res_info);
    show_response(ass_res_info);

    // close client socket
    close(client_TCP_socketfd);
    return 0;
}
