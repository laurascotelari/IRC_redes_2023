#include "server.h"

//mapeia um nome de canal para uma lista de client_sockets
map<string, vector<int>> channels;
//mapeia um socket para um cliente
map<int, string> userChannels;
//armazena os sockets
vector<int> clientSockets;
//armazenas os clientes mutados
unordered_set<int> mutedClients;
//mapeia um socket para um nickname
map<int, string> userSocket;

int server_socket;
mutex mtx;

string getNickname(int client_socket, string buffer) {
    size_t pos = buffer.find(':');
    if (pos != string::npos) {
        string message = buffer.substr(0, pos);
        size_t nicknameStart = message.find(']');
        if (nicknameStart != string::npos) {
            return message.substr(nicknameStart + 2);
        }
    }
    return "";
}

void handleClient(int client_socket){
    cout << "Cliente " << client_socket << " conectado!" << endl;

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
                break;
            } else {
                cout << "Cliente " << client_socket << " desconectado!" << endl;
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
           //printando na tela a mensagem enviada pelo cliente
            string mensagem = buffer;
            cout << "MENSAGEM: " << mensagem << endl;

            Comando comando;
            string argumento;
            //tratando os comandos que chegam ao servidor
            if (isCommand(comando, mensagem)) {
                argumento = getArgs(mensagem);
                tratarComando(client_socket, comando, argumento);
                memset(buffer, 0, sizeof(buffer));

                continue;
            }
            if (mutedClients.find(client_socket) != mutedClients.end()) {
                cout << "Cliente está silenciado. Mensagem não enviada." << endl;
                memset(buffer, 0, sizeof(buffer));

                continue;
            }
            string nickname = getNickname(client_socket, mensagem);
            if (nickname.empty()) {
                cout << "Não foi possível obter o apelido do cliente " << client_socket << endl;
                close(client_socket);
                return;
            }
            if (userSocket.count(client_socket) == 0) {
                userSocket[client_socket] = nickname;
            } 

            //A mensagem recebida pelo servidor deve ser enviada para cada cliente conectado  
            for (int i = 0; i < clientSockets.size(); i++ ){

                //verificando se o canal do cliente é igual ao canal do cliente que enviou a mensagem
                if(userChannels[clientSockets[i]] == userChannels[client_socket]){
                    cout << "Enviando '" << mensagem << "' para " << clientSockets[i] << endl;
                    // Mandar mensagem para o cliente.
                    send(clientSockets[i], mensagem.c_str(), mensagem.length(), 0);

                }
            }
        }

        memset(buffer, 0, sizeof(buffer));
    }

    close(client_socket);
}

void signalHandlerServer(int signal) {
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
    for (const auto& pair : userSocket) {
        if (pair.second == nickname) {
            mutedClients.insert(pair.first);
            cout << "Cliente " << nickname << " foi silenciado." << endl;
            return;
        }
    }
    cout << "Cliente " << nickname << " não encontrado." << endl;
}

void unmuteClient(const string& nickname) {
    int socketToUnmute = -1;
    for (int socket : mutedClients) {
        if (userSocket[socket] == nickname) {
            socketToUnmute = socket;
            break;
        }
    }

    if (socketToUnmute != -1) {
        mutedClients.erase(socketToUnmute);
        cout << "Silenciamento removido para o cliente " << nickname << "." << endl;
    } else {
        cout << "Cliente " << nickname << " não encontrado ou não está silenciado." << endl;
    }
}

//funcao para adicionar cliente a um novo canal
void joinChannel(string channel, int client_socket){

    //verificando se o canal pedido existe
    if(channels.find(channel) == channels.end()){
        //se não, criamos um novo canal com um vetor apenas com o socket do cliente
        channels[channel] = vector<int>{client_socket};
    } else{
        //o canal ja existe, entao o client_socket só é adicionado à lista atrelada 
        //ao nome do canal
        channels[channel].push_back(client_socket);
    }
    //o map userChannel deve retornar o nome do canal do cliente dado o seu socket
    userChannels[client_socket] = channel;
    //envia mensagem de confirmação para o cliente
    string message = "server: Você entrou no canal " + channel + "\n";
    send(client_socket, message.c_str(), message.length(), 0);

}

bool isAdmin(int client_socket) {
    if(client_socket == clientSockets[0]) return true;
    else return false;
}

string getIPFromUsername(const string& nickname) {
     for (const auto& pair : userSocket) {
        if (pair.second == nickname) {
            struct sockaddr_in clientAddress;
            socklen_t clientAddressLength = sizeof(clientAddress);
            getpeername(pair.first, (struct sockaddr*)&clientAddress, &clientAddressLength);

            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientAddress.sin_addr), clientIP, INET_ADDRSTRLEN);
            string ip = clientIP;
            return " Endereço IP do usuário: " + ip;
        }
    }
    return "Usuário não encontrado";
}

// Verificar se a mensagem é um comando e atribuir o valor correspondente ao parâmetro 'comando'
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

    return false; // Não é um comando
}

// Retornar os argumentos do comando
string getArgs(string mensagem){
    string argumento = mensagem.substr(mensagem.find(' ') + 1);
    return argumento;
}

// Função para tratar cada comando individualmente
void tratarComando(int client_socket, Comando comando, const string& argumento) {
    switch (comando) {
        case Comando::Ping:
        {
            cout << "Comando ping detectado" << endl;
            string message = "server: pong";
            send(client_socket, message.c_str(), message.length(), 0);
        }
            break;

        case Comando::Quit:
        {
            cout << "Cliente " << client_socket << " desconectado\n";
        }
            break;

        case Comando::Join:
        {
            string nomeCanal = argumento;
            joinChannel(nomeCanal, client_socket);

        }
            break;

        case Comando::Nickname:
        {
            string novoNick = argumento;
            userSocket[client_socket] = novoNick;
        }
            break;

        case Comando::Kick:
        {
            if (isAdmin(client_socket)) {
                
            } else {
                string mensagem = "server: Comando /kick é permitido apenas para administradores.";
                send(client_socket, mensagem.c_str(), mensagem.length(), 0);
            }
        }
            break;

        case Comando::Mute:
        {
            if (isAdmin(client_socket)) {
                muteClient(argumento);
            } else {
                string mensagem = "server: Comando /mute é permitido apenas para administradores.";
                send(client_socket, mensagem.c_str(), mensagem.length(), 0);
            }
        }
            break;

        case Comando::Unmute: 
        {
            if (isAdmin(client_socket)) {
                unmuteClient(argumento);
            } else {
                string mensagem = "server: Comando /unmute é permitido apenas para administradores.";
                send(client_socket, mensagem.c_str(), mensagem.length(), 0);
            }
        }
            break;

        case Comando::Whois:
        {
            if (isAdmin(client_socket)) { 
                string nomeUsuario = argumento;
                string resposta = getIPFromUsername(nomeUsuario);
                string mensagem = "server: Consultado o usuário " + nomeUsuario + "\n" + resposta;
                send(client_socket, mensagem.c_str(), mensagem.length(), 0);
            } else {
                string mensagem = "server: Comando /whois é permitido apenas para administradores.";
                send(client_socket, mensagem.c_str(), mensagem.length(), 0);
            }
        }
            break;

        default:
            cout << "Comando inválido." << endl;
            break;
    }
}