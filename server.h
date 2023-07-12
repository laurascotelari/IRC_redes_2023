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

enum class Comando {
    Ping,
    Quit,
    Join,
    Nickname,
    Kick,
    Mute,
    Unmute,
    Whois
};

extern vector<int> clientSockets;
extern unordered_set<int> mutedClients;
extern map<int, string> userSocket;
extern int server_socket;

string getNickname(int client_socket, string buffer);
void handleClient(int client_socket);
void signalHandlerServer(int signal);
void muteClient(const string& nickname);
void unmuteClient(const string& nickname);
int isCommand(Comando& comando, string mensagem);
void removeUser(int client_socket);
string getArgs(string mensagem);
void joinChannel(string channel, int client_socket);
void tratarComando(int client_socket, Comando comando, const string& argumento);
void kickUser(const string& argumento);