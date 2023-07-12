#include "server.h"

vector<int> clientSockets;
unordered_set<int> mutedClients;
map<int, string> userSocket;
int server_socket;

string getNickname(int client_socket) {
    char buffer[MAX_MSG] = {0};
    int num_bytes = recv(client_socket, buffer, MAX_MSG, 0);
    if (num_bytes > 0) {
        string message = buffer;
        size_t pos = message.find(':');
        if (pos != string::npos) {
            return message.substr(0, pos);
        }
    }
    return "";
}

void handleClient(int client_socket){
    string nickname = getNickname(client_socket);
    if (nickname.empty()) {
        cout << "Não foi possível obter o apelido do cliente " << client_socket << endl;
        close(client_socket);
        return;
    }
    userSocket.insert(make_pair(client_socket, nickname));
    cout << "Cliente " << nickname << " conectado!" << endl;

    mtx.lock();
    clientSockets.push_back(client_socket);
    mtx.unlock();

    char buffer[MAX_MSG] = {0};
    string message;

    while (true) {
        int num_bytes;

        if( (num_bytes = recv(client_socket, buffer, MAX_MSG, 0)) <= 0){
            if(num_bytes < 0){
                cout << "Erro no recv\n";
            } else {
                cout << "Cliente " << nickname << " desconectado!" << endl;
                mtx.lock();
                clientSockets.erase(
                    remove(clientSockets.begin(), clientSockets.end(), client_socket),
                    clientSockets.end()
                );
                userSocket.erase(client_socket);
                mtx.unlock();
            }
            break;
        } else{
            if (mutedClients.find(client_socket) != mutedClients.end()) {
                cout << "Cliente está silenciado. Mensagem não enviada." << endl;
                continue;
            }

            cout << buffer << endl;

            if(!strcmp(buffer,"/ping")){
                cout << "Comando ping detectado" << endl;
                message = "server: pong";
                send(client_socket, message.c_str(), sizeof(buffer), 0);

            }else{
                for (int i = 0; i < clientSockets.size(); i++ ){
                    cout << "Enviando '" << buffer << "' para " << userSocket[clientSockets[i]] << endl;
                    send(clientSockets[i], buffer, sizeof(buffer), 0);
                }
            }
        }

        memset(buffer, 0, sizeof(buffer));
    }

    close(client_socket);
}

void signalHandler(int signal) {
    if (signal == SIGINT) {
        cout << "Encerrando o servidor..." << endl;

        for (int i = 0; i < clientSockets.size(); i++ ){
            close(clientSockets[i]);
        }

        close(server_socket);
        exit(signal);
    }
}

void muteClient(const string& nickname) {
    for (int socket : clientSockets) {
        if (getNickname(socket) == nickname) {
            mutedClients.insert(socket);
            cout << "Cliente " << nickname << " foi silenciado." << endl;
            return;
        }
    }
    cout << "Cliente " << nickname << " não encontrado." << endl;
}

void unmuteClient(const string& nickname) {
    for (int socket : clientSockets) {
        if (getNickname(socket) == nickname) {
            mutedClients.erase(socket);
            cout << "Silenciamento removido para o cliente " << nickname << "." << endl;
            return;
        }
    }
    cout << "Cliente " << nickname << " não encontrado." << endl;
}
