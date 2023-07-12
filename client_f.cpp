#include "client.h"
#include <arpa/inet.h>
#include <csignal>

mutex mtx;
int client_socket;
int flag_fim = false;
string nickname;

int initializeChat(char* server_IP_addr){
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    inet_pton(AF_INET, server_IP_addr, &server_address.sin_addr);

    connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address));

    cout << "Você está conectado ao servidor!" << endl;

    return client_socket;
}

void chat(int client_socket){
    char buffer[MAX_MSG] = {0};
    string mensagem;
    nickname = generateNickname(nickname, 0);

    thread t1(recebeMensagem, client_socket);

    while (true) {
        getline(cin, mensagem);

        if (mensagem.length() > MAX_MSG) {
            cout << "A mensagem excede o tamanho máximo permitido de " << MAX_MSG << " caracteres." << endl;
            continue;
        }

        Comando comando;
        string argumento;
        if (isCommand(comando, mensagem)) {
            argumento = getArgs(mensagem);
            tratarComando(client_socket, comando, argumento);

            if (comando == Comando::Quit || flag_fim){
                flag_fim = true;
                break; 
            } 

            continue;
        }

        mensagem = nickname + ": " + mensagem;
               
        send(client_socket, mensagem.c_str(), mensagem.length(), 0);
    }

    t1.join();
}

int isCommand(Comando& comando, string mensagem) {
    if (mensagem == "/ping") {
        comando = Comando::Ping;
        return true;
    }
    else if (mensagem == "/quit") {
        comando = Comando::Quit;
        return true;
    }
    else if (mensagem.substr(0, 6) == "/join ") {
        comando = Comando::Join;
        return true;
    }
    else if (mensagem.substr(0, 10) == "/nickname ") {
        comando = Comando::Nickname;
        return true;
    }
    else if (mensagem.substr(0, 6) == "/kick ") {
        comando = Comando::Kick;
        return true;
    }
    else if (mensagem.substr(0, 6) == "/mute ") {
        comando = Comando::Mute;
        return true;
    }
    else if (mensagem.substr(0, 8) == "/unmute ") {
        comando = Comando::Unmute;
        return true;
    }
    else if (mensagem.substr(0, 7) == "/whois ") {
        comando = Comando::Whois;
        return true;
    }

    return false;
}

string getArgs(string mensagem){
    string argumento = mensagem.substr(mensagem.find(' ') + 1);
    return argumento;
}

void tratarComando(int client_socket, Comando comando, const string& argumento) {
    switch (comando) {
        case Comando::Ping:
            send(client_socket, "/ping", strlen("/ping"), 0);
            break;

        case Comando::Quit:
            send(client_socket, "/quit", strlen("/quit"), 0);
            flag_fim = true;
            break;

        case Comando::Join:
            {
                string nomeCanal = argumento;
                string comandoJoin = "/join " + nomeCanal;
                send(client_socket, comandoJoin.c_str(), comandoJoin.length(), 0);
            }
            break;

        case Comando::Nickname:
            {
                nickname = generateNickname(argumento, 1);
                cout << "Apelido definido como: " << nickname << endl;
            }
            break;

        case Comando::Kick:
            break;

        case Comando::Mute:
            break;

        case Comando::Unmute:
            break;

        case Comando::Whois:
            break;

        default:
            cout << "Comando inválido." << endl;
            break;
    }
}

string generateNickname(string argumento, int flag){
    srand(time(0));
    
    if(flag){
        return argumento.substr(0,50);
    }else{
        int randNum = rand()%100;

        string newNickname;
        newNickname = "cliente" + to_string(randNum);

        return newNickname;
    }
}

void recebeMensagem(int client_socket){
    char buffer[MAX_MSG] = {0};
    int num_bytes;

    while(true){
        if(flag_fim) break;

        mtx.lock();
        if ((num_bytes = recv(client_socket, buffer, MAX_MSG, 0)) <= 0){
            if (num_bytes < 0){
                cout << "Erro no recv\n";
            }
        }else{
            cout << "\t\t" << buffer << endl;
            memset(buffer, 0, sizeof(buffer));
        }
        mtx.unlock();
    }
}

void signalHandler(int signal) {
    if (signal == SIGINT) {
        cout << "Encerrando o cliente..." << endl;
        close(client_socket);
        exit(signal);
    }
}
