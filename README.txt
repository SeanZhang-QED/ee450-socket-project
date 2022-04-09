Full name: 	Xuan Zhang

Student ID:	----------

1. What you have done in the assignment?
I have implemented a distributed system with five components, including client, scheduler, hospitalA, hospitalB, and hospitalC.
Firstly, I connect three hospital with scheduler through UDP sockets. So, hospitals can send its initial status to scheduler.
After that, I connect client to scheduler by TCP. That means the client can send appointment query to scheduler.
Then the scheduler will forward the query to available hospitals by UDP and get the return matching score from those hospitals.
According to the scores, the scheduler makes the decision and finally informs the client and hospitals with the result of assignment.

2. What your code files are and what each one of them does?
client.cpp
    User interface.
    Ask for students to input its location vertex.
    Send query to Scheduler by TCP.
    Get the result of assignment from scheduler.
scheduler.cpp
    Task scheduler.
    Receive the hospitals' initial status information by UDP.
    Receive the client's requests by TCP.
    Forward query information to available hospital servers by UDP.
    Take assignments the appropriate hospital to the client by returned matching scores.
    Inform the client and corresponding hospital server about the results.
hospitalA.cpp, hospitalB.cpp, hospitalC.cpp
    Scoring server.
    Read and creat given map.
    Ask for its manager to input its basic information and send it to Scheduler by UDP.
    Find the shortest distance from the its location vertex to client location vertices using dijkstra’s.
    Calculate the matching score by the shortest distance and its availability.
    Except the UDP port number, these three files are almost the same.

3. The format of all the messages exchanged:
Phase 1 Boot_up:
    hospital A/B/C to scheduler: < hospital index | total capacity | initial occupancy>
Phase 2 Forward:
    client to scheduler: <client's location vertex>
    scheduler to hospital A/B/C: <assignment flag = 0 (means request scoring) | client's location vertex>
Phase 3 Scoring:
    hospital A/B/C to scheduler: <hospital index | shortest distance | matching score>
Phase 4 Reply:
    scheduler to hospital which is assigned to: <assignment flag = 1 (means to update its occupancy) | client's location vertex>
    scheduler to client: < assigned hospital index | client is located in the given map or not | returned matching score>
Note: "|" means concatenation.

4. Idiosyncrasy in project:
To sent and receive through socket, the max size of buffer is fixed to 1024. Thus, if a single message exceeds this size, the program would crash.

5. Reused Code:
a. Beej's Guide to Network Programming:
    To implement of TCP and UDP socket communication.
b. https://www.geeksforgeeks.org/dijkstras-shortest-path-algorithm-greedy-algo-7/:
    code segment, which implements Dijkstra Algorithm is to find the shortest distance for adjacency matrix representation of the graph.
