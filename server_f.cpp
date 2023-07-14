#include "server.h"

map<string, Channel> channels; // Mapa de canais, onde a chave é o nome do canal e o valor é uma estrutura Channel
map<int, string> userChannels; // Mapeamento entre o socket do cliente e o canal atual em que o cliente está conectado
vector<int> clientSockets; // Vetor contendo os sockets dos clientes conectados
unordered_set<int> mutedClients; // Conjunto de clientes silenciados
map<int, string> userSocket; // Mapeamento entre o socket do cliente e seu apelido

int server_socket; // Socket do servidor
mutex mtx; // Mutex para controle de concorrência

// Obtém o apelido do cliente a partir do buffer de mensagem recebido
string getNickname(int client_socket, string buffer) {
    // Procura pelo primeiro caractere ':' no buffer
    size_t pos = buffer.find(':');
    if (pos != string::npos) {
        string message = buffer.substr(0, pos);
        // Procura pelo caractere ']' na mensagem para identificar o início
        size_t nicknameStart = message.find(']');
        if (nicknameStart != string::npos) {
            // Retorna o apelido encontrado na mensagem
            return message.substr(nicknameStart + 2);
        }
    }
    // Caso não encontre um apelido válido, retorna uma string vazia
    return "";
}

// Verifica se o usuário está convidado para o canal
bool isUserInvited(int client_socket, const string& channelName) {
    // Verifica se o canal existe no mapa de canais
    if (channels.find(channelName) != channels.end()) {
        const Channel& channel = channels[channelName];
        // Percorre a lista de usuários convidados do canal
        for (int socket : channel.invitedUsers) {
            // Se encontrar o socket do cliente na lista de convidados, retorna true
            if (socket == client_socket) {
                return true;
            }
        }
    }
    // Caso contrário, retorna false
    return false;
}

// Lida com as operações do cliente
void handleClient(int client_socket) {
    cout << "Cliente " << client_socket << " conectado!" << endl;

    mtx.lock();
    clientSockets.push_back(client_socket);
    mtx.unlock();

    char buffer[MAX_MSG] = {0};
    string message;

    while (true) {
        memset(buffer, 0, sizeof(buffer));

        // Recebe a mensagem do cliente
        int num_bytes = recv(client_socket, buffer, MAX_MSG, 0);
        if (num_bytes <= 0) {
            if (num_bytes < 0) {
                cout << "Erro no recv do cliente " << client_socket << ": " << strerror(errno) << endl;
            } else {
                cout << "Cliente " << client_socket << " desconectado!" << endl;
            }
            break;
        } else {
            string mensagem = buffer;
            cout << "MENSAGEM: " << mensagem << endl;

            Comando comando;
            string argumento;

            // Verifica se a mensagem é um comando
            if (isCommand(comando, mensagem)) {
                argumento = getArgs(mensagem);
                // Trata o comando recebido
                tratarComando(client_socket, comando, argumento);
                continue;
            }

            // Verifica se o cliente está silenciado
            if (mutedClients.find(client_socket) != mutedClients.end()) {
                cout << "Cliente está silenciado. Mensagem não enviada." << endl;
                continue;
            }

            // Obtém o apelido do cliente a partir da mensagem
            string nickname = getNickname(client_socket, mensagem);
            if (nickname.empty()) {
                cout << "Não foi possível obter o apelido do cliente " << client_socket << endl;
                close(client_socket);
                return;
            }

            // Adiciona o apelido do cliente ao mapeamento de sockets e apelidos
            if (userSocket.count(client_socket) == 0) {
                userSocket[client_socket] = nickname;
            }

            // Obtém o canal atual do cliente
            string channelName = userChannels[client_socket];
            if (channels.find(channelName) != channels.end()) {
                Channel& channel = channels[channelName];
                // Envia a mensagem para todos os usuários no canal
                for (int socket : channel.users) {
                    cout << "Enviando '" << mensagem << "' para " << socket << endl;
                    send(socket, mensagem.c_str(), mensagem.length(), 0);
                }
            }
        }
    }
    // Remove o cliente ao finalizar a conexão
    removeUser(client_socket);
}

// Função de tratamento de sinal do servidor
void signalHandlerServer(int signal) {
    if (signal == SIGINT) {
        cout << "Encerrando o servidor..." << endl;

        // Fecha os sockets dos clientes conectados
        for (int i = 0; i < clientSockets.size(); i++) {
            close(clientSockets[i]);
        }

        // Fecha o socket do servidor
        close(server_socket);
        exit(signal);
    }
}

// Silencia o cliente com o apelido fornecido
void muteClient(const string& nickname) {
    // Procura o socket do cliente com base no apelido
    for (const auto& pair : userSocket) {
        if (pair.second == nickname) {
            // Insere o socket do cliente no conjunto de clientes silenciados
            mutedClients.insert(pair.first);
            cout << "Cliente " << nickname << " foi silenciado." << endl;
            return;
        }
    }
    cout << "Cliente " << nickname << " não encontrado." << endl;
}

// Remove o silenciamento do cliente com o apelido fornecido
void unmuteClient(const string& nickname) {
    int socketToUnmute = -1;
     // Procura o socket do cliente silenciado com base no apelido
    for (int socket : mutedClients) {
        if (userSocket[socket] == nickname) {
            socketToUnmute = socket;
            break;
        }
    }

    if (socketToUnmute != -1) {
        // Remove o socket do cliente do conjunto de clientes silenciados
        mutedClients.erase(socketToUnmute);
        cout << "Silenciamento removido para o cliente " << nickname << "." << endl;
    } else {
        cout << "Cliente " << nickname << " não encontrado ou não está silenciado." << endl;
    }
}

// Verifica se o cliente é um administrador
bool isAdmin(int client_socket) {
    string nickname = userSocket[client_socket];
    if (channels.find(userChannels[client_socket]) != channels.end()) {
        const Channel& channel = channels[userChannels[client_socket]];
        // Verifica se o apelido do cliente corresponde ao apelido do administrador do canal
        if (channel.admin == nickname) {
            return true;
        }
    }
    return false;
}

// Conecta o cliente ao canal fornecido
void joinChannel(const string& channel, int client_socket) {
    if (channels.find(channel) == channels.end()) {
        // Se o canal não existir, crie uma nova estrutura Channel
        Channel newChannel;
        newChannel.name = channel;
        newChannel.users.push_back(client_socket);
        newChannel.admin = userSocket[client_socket];  // Atribua o apelido do cliente como administrador do canal
        channels[channel] = newChannel;
    } else {
        // O canal já existe
        if (isUserInvited(client_socket, channel) || isAdmin(client_socket)) {  // Verifica se o usuário está convidado ou é o administrador
            // Adiciona o cliente à lista de usuários
            channels[channel].users.push_back(client_socket);
        } else {
            // O usuário não está convidado, envie uma mensagem de erro
            string message = "server: Você não está convidado para este canal.\n";
            send(client_socket, message.c_str(), message.length(), 0);
            return;
        }
    }

    // Atualiza o mapeamento do cliente para o canal atual
    userChannels[client_socket] = channel;

    // Envia mensagem de confirmação para o cliente
    string message = "server: Você entrou no canal " + channel + "\n";
    send(client_socket, message.c_str(), message.length(), 0);
}


// Obtém o endereço IP do usuário com base no apelido fornecido
string getIPFromUsername(const string& nickname) {
    for (const auto& pair : userSocket) {
        if (pair.second == nickname) {
            struct sockaddr_in clientAddress;
            socklen_t clientAddressLength = sizeof(clientAddress);
            // Obtém as informações do endereço do cliente
            getpeername(pair.first, (struct sockaddr*)&clientAddress, &clientAddressLength);

            char clientIP[INET_ADDRSTRLEN];
            // Converte o endereço IP para uma string legível
            inet_ntop(AF_INET, &(clientAddress.sin_addr), clientIP, INET_ADDRSTRLEN);
            string ip = clientIP;
            return " Endereço IP do usuário: " + ip;
        }
    }
    return "Usuário não encontrado";
}

// Expulsa o usuário com o apelido fornecido do canal atual
void kickUser(int client_socket, const string& argumento) {
    string message;
    string channelName = userChannels[client_socket];

    if (channels.find(channelName) != channels.end()) {
        Channel& channel = channels[channelName];

        // Verifica se o cliente que chamou o comando é o administrador do canal
        if (!isAdmin(client_socket)) {
            message = "server: Você não tem permissão para usar o comando /kick.\n";
            send(client_socket, message.c_str(), message.length(), 0);
            return;
        }

        // Verifica se o usuário a ser expulso existe no canal
        int userSocketToKick = -1;
        for (int socket : channel.users) {
            if (userSocket[socket] == argumento) {
                userSocketToKick = socket;
                break;
            }
        }

        if (userSocketToKick != -1) {
            // Envia mensagem informando que o usuário foi expulso para o cliente que chamou o comando
            message = "server: " + argumento + " foi expulso do canal.\n";
            send(client_socket, message.c_str(), message.length(), 0);

            // Envia mensagem informando que o usuário foi expulso para os demais usuários do canal
            message = "server: Você foi expulso do canal por " + userSocket[client_socket] + ".\n";
            send(userSocketToKick, message.c_str(), message.length(), 0);

            // Remove o usuário da lista de usuários do canal
            channel.users.erase(remove(channel.users.begin(), channel.users.end(), userSocketToKick), channel.users.end());

            // Remove o usuário da lista de usuários convidados do canal
            channel.invitedUsers.erase(remove(channel.invitedUsers.begin(), channel.invitedUsers.end(), userSocketToKick), channel.invitedUsers.end());
        } else {
            message = "server: Usuário " + argumento + " não encontrado no canal.\n";
            send(client_socket, message.c_str(), message.length(), 0);
        }
    } else {
        message = "server: Você não está em nenhum canal.\n";
        send(client_socket, message.c_str(), message.length(), 0);
    }
}


// Obtém a lista de clientes conectados
vector<int> getConnectedUsers() {
    return clientSockets;
}

// Envia a lista de clientes conectados para o administrador
void sendConnectedUsers(int client_socket, const vector<int>& connectedUsers) {
    string message = "server: Clientes conectados disponíveis para convite:\n";

    // Percorre a lista de clientes conectados e envia suas informações
    for (int socket : connectedUsers) {
        string nickname = userSocket[socket];
        message += "Cliente " + to_string(socket) + ": " + nickname + "\n";
    }

    send(client_socket, message.c_str(), message.length(), 0);
}


// Convida o usuário com o apelido fornecido para o canal especificado
void inviteUser(const string& channelName, int client_socket, const string& nickname) {
    if (channels.find(channelName) != channels.end()) {
        Channel& channel = channels[channelName];
        if (isAdmin(client_socket) || channel.admin == userSocket[client_socket]) {
            for (const auto& pair : userSocket) {
                if (pair.second == nickname) {
                    int invitedClientSocket = pair.first;
                    // Adiciona o socket do cliente à lista de usuários convidados do canal
                    channel.invitedUsers.push_back(invitedClientSocket);
                    string message = "server: Você foi convidado para o canal " + channelName + "\n";
                    // Envia uma mensagem de convite para o cliente convidado
                    send(invitedClientSocket, message.c_str(), message.length(), 0);
                    return;
                }
            }
        } else {
            string message = "server: Comando /invite é permitido apenas para administradores.";
            send(client_socket, message.c_str(), message.length(), 0);
            return;
        }
    }
    string message = "server: Usuário " + nickname + " não encontrado ou canal " + channelName + " não existe.\n";
    send(client_socket, message.c_str(), message.length(), 0);
}

// Obtém a lista de usuários convidados para o canal especificado
vector<int> listInvitedUsers(const string& channelName) {
    vector<int> invitedUsers;

    if (channels.find(channelName) != channels.end()) {
        const Channel& channel = channels[channelName];
        invitedUsers = channel.invitedUsers;
    }

    return invitedUsers;
}

// Atualiza usuários convidados
void updateInvitedUsers(int client_socket) {
    for (auto& pair : channels) {
        Channel& channel = pair.second;
        channel.invitedUsers.erase(remove(channel.invitedUsers.begin(), channel.invitedUsers.end(), client_socket), channel.invitedUsers.end());
    }
}

// Remove o cliente do servidor
void removeUser(int client_socket) {
    close(client_socket);
    // Remove o socket do cliente do vetor de sockets dos clientes conectados
    clientSockets.erase(remove(clientSockets.begin(), clientSockets.end(), client_socket), clientSockets.end());
    // Remove o socket do cliente do mapeamento de sockets e apelidos
    userSocket.erase(client_socket);
    // Remove o socket do cliente do mapeamento de sockets e canais
    userChannels.erase(client_socket);
}

// Verifica se a mensagem recebida é um comando válido
int isCommand(Comando& comando, string mensagem) {
    if (mensagem == "/ping") {
        comando = Comando::Ping;
        return true;
    } else if (mensagem == "/quit") {
        comando = Comando::Quit;
        return true;
    } else if (mensagem.substr(0, 6) == "/join ") {
        comando = Comando::Join;
        return true;
    } else if (mensagem.substr(0, 10) == "/nickname ") {
        comando = Comando::Nickname;
        return true;
    } else if (mensagem.substr(0, 6) == "/kick ") {
        comando = Comando::Kick;
        return true;
    } else if (mensagem.substr(0, 6) == "/mute ") {
        comando = Comando::Mute;
        return true;
    } else if (mensagem.substr(0, 8) == "/unmute ") {
        comando = Comando::Unmute;
        return true;
    } else if (mensagem.substr(0, 7) == "/whois ") {
        comando = Comando::Whois;
        return true;
    } else if (mensagem.substr(0, 8) == "/invite ") {
        comando = Comando::Invite;
        return true;
    } else if (mensagem == "/consultUsers") {
        comando = Comando::ConsultUsers;
        return true;
    }

    return false;
}

// Obtém os argumentos de um comando a partir da mensagem recebida
string getArgs(string mensagem) {
    string argumento = mensagem.substr(mensagem.find(' ') + 1);
    return argumento;
}

// Trata o comando recebido do cliente
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
            removeUser(client_socket);
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
                cout << "Comando Kick detectado!\n";
                kickUser(client_socket, argumento);
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

        case Comando::Invite:
        {
            if (isAdmin(client_socket)) {
                size_t pos = argumento.find(' ');
                if (pos != string::npos) {
                    string channelName = argumento.substr(0, pos);
                    string nickname = argumento.substr(pos + 1);
                    inviteUser(channelName, client_socket, nickname);
                } else {
                    string mensagem = "server: Uso incorreto do comando /invite. Formato esperado: /invite <canal> <apelido>";
                    send(client_socket, mensagem.c_str(), mensagem.length(), 0);
                }
            } else {
                string mensagem = "server: Comando /invite é permitido apenas para administradores.";
                send(client_socket, mensagem.c_str(), mensagem.length(), 0);
            }
        }
            break;

        case Comando::ConsultUsers:
        {
            if (isAdmin(client_socket)) {
                // Chame uma função para obter a lista de clientes conectados
                vector<int> connectedUsers = getConnectedUsers();

                // Envie a lista de clientes para o administrador
                sendConnectedUsers(client_socket, connectedUsers);
            } else {
                string mensagem = "server: Comando /consultUsers é permitido apenas para administradores.";
                send(client_socket, mensagem.c_str(), mensagem.length(), 0);
            }
        }
            break;
            
        default:
            cout << "Comando inválido." << endl;
            break;
    }
}

