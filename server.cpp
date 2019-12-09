#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>
#include <cstdio>
#include <thread>
#include <unistd.h>
#include <vector>
#include <mutex>
#include <ctime>
#include <sys/epoll.h>

#define ACCEPT "ACCEPT"
#define REFUSE "REFUSE"

using namespace std;
const int one = 1;

int sock;
vector<int> waitingClients, playingClients;

thread newClientsThread, gameThread;
mutex waitingClientMtx;

void handleNewConnections() {
    int clientSock;
    while(true) {
        clientSock = accept(sock, nullptr, nullptr);

        if (clientSock == -1) {
            perror("Client socket creation error");
            continue;
        }

        write(clientSock, ACCEPT, sizeof(ACCEPT));

        waitingClientMtx.lock();
        waitingClients.push_back(clientSock);
        waitingClientMtx.unlock();
    }
}

void handleGame() {
    clock_t begClk, endClk;
    while (true) {
        // waitingClients moved from waiting room to new game
        // playingClients moved from last game to waiting room
        waitingClientMtx.lock();
        swap(waitingClients, playingClients);
        waitingClientMtx.unlock();

        // set epoll for new session countdown
        int newGameEpoll = epoll_create1(0);

        epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = sock;

        for (int i=0; i<waitingClients.size(); ++i)
            epoll_ctl(newGameEpoll, EPOLL_CTL_ADD, waitingClients.at(i), &event);

        // run countdown to start new game session
        begClk = clock();
        endClk = clock();
        while ((endClk - begClk) / CLOCKS_PER_SEC < 30) {
            while (epoll_wait(newGameEpoll, &event, 1, -1) > 0) {
                // TODO
            }
        }

        // TODO: kick clients that are not ready
        // TODO: run game

    }
}

int main() {
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock == -1) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(8080);

    if (bind(sock, (sockaddr*) &addr, sizeof(addr)) == -1) {
        perror("Bind socket error");
        exit(EXIT_FAILURE);
    }

    listen(sock, 1);

    newClientsThread = thread(handleNewConnections);
    gameThread = thread(handleGame);

    newClientsThread.join();
    gameThread.join();

    return 0;
}