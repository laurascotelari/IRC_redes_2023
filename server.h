#pragma once

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <csignal>
#include <algorithm>
#include <unordered_set>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

#define MAX_MSG 4096

extern vector<int> clientSockets;
extern unordered_set<int> mutedClients;
extern map<int, string> userSocket;
extern int server_socket;

mutex mtx;

string getNickname(int client_socket);
void handleClient(int client_socket);
void signalHandler(int signal);
void muteClient(const string& nickname);
void unmuteClient(const string& nickname);
