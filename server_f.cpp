#include "server.h"

map<string, Channel> channels;
map<int, string> userChannels;
vector<int> clientSockets;
unordered_set<int> mutedClients;
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

bool isUserInvited(int client_socket, const string& channelName) {
    if (channels.find(channelName) != channels.end()) {
        const Channel& channel = channels[channelName];
        for (int socket : channel.invitedUsers) {
            if (socket == client_socket) {
                return true;
            }
        }
    }
    return false;
}

void handleClient(int client_socket) {
    cout << "Cliente " << client_socket << " conectado!" << endl;

    mtx.lock();
    clientSockets.push_back(client_socket);
    mtx.unlock();

    char buffer[MAX_MSG] = {0};
    string message;

    while (true) {
        memset(buffer, 0, sizeof(buffer));

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

            if (isCommand(comando, mensagem)) {
                argumento = getArgs(mensagem);
                tratarComando(client_socket, comando, argumento);
                continue;
            }

            if (mutedClients.find(client_socket) != mutedClients.end()) {
                cout << "Cliente está silenciado. Mensagem não enviada." << endl;
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

            string channelName = userChannels[client_socket];
            if (channels.find(channelName) != channels.end()) {
                Channel& channel = channels[channelName];
                for (int socket : channel.users) {
                    cout << "Enviando '" << mensagem << "' para " << socket << endl;
                    send(socket, mensagem.c_str(), mensagem.length(), 0);
                }
            }
        }
    }

    removeUser(client_socket);
}

void signalHandlerServer(int signal) {
    if (signal == SIGINT) {
        cout << "Encerrando o servidor..." << endl;

        for (int i = 0; i < clientSockets.size(); i++) {
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

bool isAdmin(int client_socket) {
    string nickname = userSocket[client_socket];
    if (channels.find(userChannels[client_socket]) != channels.end()) {
        const Channel& channel = channels[userChannels[client_socket]];
        if (channel.admin == nickname) {
            return true;
        }
    }
    return false;
}

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

void kickUser(int client_socket, const string& argumento) {
    string message = "server: " + argumento + " foi expulso do chat.\n";
    string channelName = userChannels[client_socket];

    if (channels.find(channelName) != channels.end()) {
        Channel& channel = channels[channelName];

        channel.users.erase(remove(channel.users.begin(), channel.users.end(), client_socket), channel.users.end());
        channel.invitedUsers.erase(remove(channel.invitedUsers.begin(), channel.invitedUsers.end(), client_socket), channel.invitedUsers.end());

        send(client_socket, message.c_str(), message.length(), 0);

        for (int socket : channel.users) {
            send(socket, message.c_str(), message.length(), 0);
        }
    }
}

void inviteUser(const string& channelName, int client_socket, const string& nickname) {
    if (channels.find(channelName) != channels.end()) {
        Channel& channel = channels[channelName];
        if (isAdmin(client_socket) || channel.admin == userSocket[client_socket]) {
            for (const auto& pair : userSocket) {
                if (pair.second == nickname) {
                    int invitedClientSocket = pair.first;
                    channel.invitedUsers.push_back(invitedClientSocket);
                    string message = "server: Você foi convidado para o canal " + channelName + "\n";
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


vector<int> listInvitedUsers(const string& channelName) {
    vector<int> invitedUsers;

    if (channels.find(channelName) != channels.end()) {
        const Channel& channel = channels[channelName];
        invitedUsers = channel.invitedUsers;
    }

    return invitedUsers;
}

void updateInvitedUsers(int client_socket) {
    for (auto& pair : channels) {
        Channel& channel = pair.second;
        channel.invitedUsers.erase(remove(channel.invitedUsers.begin(), channel.invitedUsers.end(), client_socket), channel.invitedUsers.end());
    }
}

void removeUser(int client_socket) {
    close(client_socket);
    clientSockets.erase(remove(clientSockets.begin(), clientSockets.end(), client_socket), clientSockets.end());
    userSocket.erase(client_socket);
    userChannels.erase(client_socket);
}

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
    }

    return false;
}

string getArgs(string mensagem) {
    string argumento = mensagem.substr(mensagem.find(' ') + 1);
    return argumento;
}

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

        default:
            cout << "Comando inválido." << endl;
            break;
    }
}

