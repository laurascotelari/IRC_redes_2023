#pragma once

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <cstdlib>  // usar rand() e srand()
#include <ctime>  
#include <thread>
#include <mutex>
#include <csignal>

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

extern int client_socket;
extern int flag_fim;

int initializeChat(char* server_IP_addr);
void chat(int client_socket);
int isCommand(Comando& comando, string mensagem);
string getArgs(string mensagem);
void tratarComando(int client_socket, Comando comando, const string& argumento);
string generateNickname(string argumento, int flag);
void recebeMensagem(int client_socket);
void signalHandlerClient(int signal);
