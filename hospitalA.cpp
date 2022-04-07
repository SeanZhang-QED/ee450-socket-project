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
#include <sstream>
#include <fstream>
#include <limits.h>
#include <algorithm>
#include <cfloat>

using namespace std;

//Named Constants
#define LOCAL_HOST "127.0.0.1" // Host address
#define SCHEDULER_UDP_PORT 33165 // Scheduler (server) UDP port number
#define A_UDP_PORT 30165 //  Hospital A (server) UDP port number
#define MAXSIZE 1024 //size of recv and send buff

//Define the message struct
struct Ini_status
{
    int hospital_loc;
    int total_capacity;
    int ini_occupancy;
};
struct Status
{
    int location_idx;
    int capacity;
    int occupation;
};
struct ScoreResponseInfo
{
    char hospital_idx;
    float distance;
    float score;
};
struct ScoreQueryInfo
{
    // assignment_flag = 0, query score
    //                 = 1, assignment to this hospital
    int assignment_flag;
    int input_vertex_idx;
};

//Defined global variables
int A_UDP_sockfd, A_server_sockfd; // Hospital A datagram socket
struct sockaddr_in A_addr, scheduler_UDP_addr; // Scheduler address as a server & as a client
struct Status A_status;
struct Ini_status ini_A_status;
struct ScoreQueryInfo A_query;
vector<int> locations; // store the nodes in the map.txt
vector<int> :: iterator it;
float** adjMatrix;
int numVertices;
float min_dist;


//Defined functions
void read_and_getidx();
void read_and_creat_matrix();
void prep_hospitalA_socket();
void send_to_Scheduler(char buff[], int size);
int minDistance(float dist[], bool sptSet[]);
int reindexing(int original_index);
//read and get the verter which contain all the nodes in given map
void read_and_getidx(){
    ifstream map_file("map.txt", ios::in);
    string l;
    int i = 0; // store the # of line counting
    int min_idx, max_idx; // store the local min and max of location index
    while (getline(map_file, l)) {
        // split current line with space and save each line into cur_line
        vector <string> cur_line;
        const char *delim = " ";
        char *strs = new char[l.length() + 1];
        strcpy(strs, l.c_str());
        char *p = strtok(strs, delim);
        while (p) {
            string s = p;
            cur_line.push_back(s);
            p = strtok(NULL, delim);
        }
        int first_end = atoi(cur_line[0].c_str());
        int second_end = atoi(cur_line[1].c_str());
        float weight = atof(cur_line[2].c_str());
        // add the vertex into the vector
        it = find(locations.begin(),locations.end(),first_end);
        if(it == locations.end()){
            locations.push_back(first_end);
        }
        it = find(locations.begin(),locations.end(),second_end);
        if(it == locations.end()){
            locations.push_back(second_end);
        }
        i++;
    }
    // sort the vector get the nodes number
    sort(locations.begin(), locations.end()); // from small to large
    numVertices = 0;
    for (it = locations.begin(); it!= locations.end();it ++) {
        numVertices ++;
    }
}
//read and creat the Adjacency Matrix representation of map
void read_and_creat_matrix(){
    // initialize the adjacency Matrix to 0
    adjMatrix = new float*[numVertices];
    for (int i = 0; i < numVertices; i++) {
        adjMatrix[i] = new float[numVertices];
        for (int j = 0; j < numVertices; j++)
            adjMatrix[i][j] = 0;
    }
    ifstream map_file("map.txt", ios::in);
    string l;
    int i = 0; // store the # of line counting
    int min_idx, max_idx; // store the local min and max of location index
    while (getline(map_file, l)) {
        // split current line with space and save each line into cur_line
        vector <string> cur_line;
        const char *delim = " ";
        char *strs = new char[l.length() + 1];
        strcpy(strs, l.c_str());
        char *p = strtok(strs, delim);
        while (p) {
            string s = p;
            cur_line.push_back(s);
            p = strtok(NULL, delim);
        }
        int first_end = atoi(cur_line[0].c_str());
        int second_end = atoi(cur_line[1].c_str());
        float weight = atof(cur_line[2].c_str());

        int u = reindexing(first_end);
        int v = reindexing(second_end);
        adjMatrix[u][v] = weight;
        adjMatrix[v][u] = weight;

    }
}
//boot up phase, prepare the UDP socket
void prep_hospitalA_socket() {
    if ((A_UDP_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("[ERROR] Hospital A: can not open socket \n");
        exit(1);
    }
    // for send socket
    memset(&scheduler_UDP_addr, 0, sizeof(scheduler_UDP_addr)); //  make sure the struct is empty
    scheduler_UDP_addr.sin_family = AF_INET; // Use IPv4 address family
    scheduler_UDP_addr.sin_addr.s_addr = inet_addr(LOCAL_HOST); // Host IP address
    scheduler_UDP_addr.sin_port = htons(SCHEDULER_UDP_PORT); // Destination port num in Scheduler
    // show on screen message
    printf("Hospital A is up and running using UDP on port %d \n", A_UDP_PORT);
}
//send UDP socket to Scheduler
void send_to_Scheduler(char buff[], int size){
    socklen_t scheduler_UDP_addr_len = sizeof(scheduler_UDP_addr);
    if (sendto(A_UDP_sockfd, buff, size, 0, (struct sockaddr *)&scheduler_UDP_addr, scheduler_UDP_addr_len) == -1) {
        perror("[ERROR] Hospital A: pocket SEND error \n");
        exit(1);
    }
}
// reindex the input vertex into the corresponding index in the vector
int reindexing(int original_index){
    for (int i = 0; i < numVertices; i++) {
        if(locations[i] == original_index){
            return i;
        }
    }
    return -1;
}
//****the function below refers to GreeksforGreeks****//
// https://www.geeksforgeeks.org/dijkstras-shortest-path-algorithm-greedy-algo-7/
// A utility function to find the vertex with minimum distance value, from
// the set of vertices not yet included in shortest path tree
int minDistance(float dist[], bool sptSet[])
{
    // Initialize min value
    float min = FLT_MAX, min_index;

    for (int v = 0; v < numVertices; v++)
        if (sptSet[v] == false && dist[v] <= min)
            min = dist[v], min_index = v;

    return min_index;
}


int main(int argc,char *argv[]) {
    // Phase 1: boot up
    //      Step1: get info about A from CLI
    read_and_getidx();
    read_and_creat_matrix();
    prep_hospitalA_socket();
    char send_buf[MAXSIZE] = {0};
    ini_A_status.hospital_loc = 1;
    ini_A_status.total_capacity = atoi(argv[2]) ;
    ini_A_status.ini_occupancy = atoi(argv[3]);
    A_status.location_idx = atol(argv[1]);
    A_status.capacity = ini_A_status.total_capacity;
    A_status.occupation = ini_A_status.ini_occupancy;
    printf("Hospital A has total capacity %d and initial occupancy is %d \n", ini_A_status.total_capacity, ini_A_status.ini_occupancy);
    //      Step2: creat and bind UDP socket, then send to Scheduler
    memset(send_buf, 0, MAXSIZE);
    memcpy(send_buf, &ini_A_status, sizeof(ini_A_status));
    send_to_Scheduler(send_buf, sizeof(send_buf));
    close(A_UDP_sockfd);

    // Phase 2: Forward
    // creat a udp socket act as UDP server
    if ((A_server_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("[ERROR] Hospital A: can not open socket \n");
        exit(1);
    }
    memset(&A_addr, 0, sizeof A_addr); // make sure the struct is empty
    A_addr.sin_family = AF_INET; // IPv4 address
    A_addr.sin_port = htons(A_UDP_PORT); // Scheduler port for UDP
    A_addr.sin_addr.s_addr = inet_addr(LOCAL_HOST); // Host IP address

    // bind the created udp socket with local address
    if ((bind(A_server_sockfd, (struct sockaddr*) &A_addr, sizeof A_addr))== -1) {
        perror("[ERROR]: fail to BIND UDP socket");
        exit(1);
    }
    // receive the information from client(scheduler)
    while(true){
        //      Step1: recv UDP socket from Scheduler
        // build the container (A_query) that will receive the msg from Scheduler
        socklen_t scheduler_UDP_addr_len = sizeof(scheduler_UDP_addr);
        char recv_buff[MAXSIZE];
        memset(recv_buff, 0, MAXSIZE);
        if(recvfrom(A_server_sockfd, recv_buff, MAXSIZE, 0, (struct sockaddr *)&scheduler_UDP_addr, &scheduler_UDP_addr_len) == -1) {
            perror("[ERROR] Hospital A: UDP pocket RECEIVE error \n");
            exit(1);
        }
        memset(&A_query, 0, sizeof(A_query));
        memcpy(&A_query, recv_buff, sizeof(A_query));
        // if the query is to ask to score
        if(A_query.assignment_flag == 0){
            struct ScoreResponseInfo A_response;
            A_response.hospital_idx = 'A';
            printf("-------------------------------------------------------------------- \n");
            printf("Hospital A has received input from client at location %d \n", A_query.input_vertex_idx);
            //      Step2: location finding
            // if not found in the map, then  A_response.distance = -1;
            if(reindexing(A_query.input_vertex_idx) == -1){
                A_response.distance = -1;
                printf("Hospital A does not have the location %d in map. \n", A_query.input_vertex_idx);
                char send_buf[MAXSIZE] = {0};
                memset(send_buf, 0, MAXSIZE);
                memcpy(send_buf, &A_response, sizeof(A_response));
                send_to_Scheduler(send_buf, sizeof(send_buf));
                printf("Hospital A has sent 'location not found' to the Scheduler. \n");
            } else{
                // found the vertex in the map
                // Phase 3: Score
                //      Step1: availability
                float availability = (float) (A_status.capacity - A_status.occupation) / A_status.capacity;
                printf("Hospital A has capacity = %d, occupation = %d, availability = %f \n", A_status.capacity, A_status.occupation, availability);
                //      Step2: min distance
                //****the codes about dijkstra alg refers to GreeksforGreeks****//
                // https://www.geeksforgeeks.org/dijkstras-shortest-path-algorithm-greedy-algo-7/
                int src = reindexing(A_status.location_idx);
                int dst = reindexing(A_query.input_vertex_idx);
                int V = numVertices;
                float dist[V];
                bool sptSet[V];
                for (int i = 0; i < V; i++)
                    dist[i] = FLT_MAX, sptSet[i] = false;
                dist[src] = 0;
                for (int count = 0; count < V - 1; count++) {
                    int u = minDistance(dist, sptSet);
                    sptSet[u] = true;
                    for (int v = 0; v < V; v++)
                        if (!sptSet[v] && adjMatrix[u][v] && dist[u] != FLT_MAX
                            && dist[u] + adjMatrix[u][v] < dist[v])
                            dist[v] = dist[u] + adjMatrix[u][v];
                }
                min_dist = dist[dst];
                printf("Hospital A has found the shortest path to client, distance = %f \n", min_dist);
                //      Step3: Score
                float score;
                score = 1/(min_dist * (1.1 - availability));
                printf("Hospital A has score = %f \n", score);
                //      Step4: send udp to scheduler
                A_response.score = score;
                A_response.distance = min_dist;
                char send_buf[MAXSIZE] = {0};
                memset(send_buf, 0, MAXSIZE);
                memcpy(send_buf, &A_response, sizeof(A_response));
                send_to_Scheduler(send_buf,sizeof(send_buf));
                printf("Hospital A has sent score = %f and distance = %f to the Scheduler.\n", score, min_dist);
            }
        }
        // if the query is to inform the assignment
        if(A_query.assignment_flag == 1) {
            A_status.occupation ++;
            float availability = (float) (A_status.capacity - A_status.occupation) / A_status.capacity;
            printf("Hospital A has been assigned to a client, occupation is updated to %d, availability is updated to %f\n", A_status.occupation, availability);
        }
    }
    close(A_UDP_sockfd);
    return 0;
}

