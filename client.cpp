#include <iostream>
#include <csignal>
#include "client.h"

using namespace std;

int main(int argc, char **argv) {
    signal(SIGINT, signalHandlerClient);

    //caso o usuario nao tenha digitado o endereco do server
    if (argc != 2) {
        cout << "use: ./client <IPaddress>\n";
        return -1;
    }

    //controla se o usuario ja mandou o comando /connect
    int flagConnect = false;

    cout << "Comandos importantes:\n";
    cout << "\t/connect : Estabelece conexão com o servidor\n";
    cout << "\t/quit    : Cliente fecha a conexão e fecha a aplicação\n";
    cout << "\t/ping    : Servidor retorna pong assim que receber a mensagem\n";
    cout << endl;
    cout << "Comandos possíveis:\n";
    cout << "\t/join nomeCanal - Entra no canal\n";
    cout << "\t/nickname apelidoDesejado - O cliente passa a ser reconhecido pelo apelido especificado\n";
    cout << "\t/ping - O servidor retorna \"pong\" assim que receber a mensagem\n";
    cout << endl;
    cout << "Comandos apenas para administradores de canais:\n";
    cout << "\t/kick nomeUsuario - Fecha a conexão de um usuário especificado\n";
    cout << "\t/mute nomeUsuario - Faz com que um usuário não possa enviar mensagens neste canal\n";
    cout << "\t/unmute nomeUsuario - Retira o mute de um usuário\n";
    cout << "\t/whois nomeUsuario - Retorna o endereço IP do usuário apenas para o administrador\n";
    cout << "\t/invite nomeCanal nomeUsuario - Convida um usuário para o canal\n";
    cout << "\t/consultUsers - Mostra os clientes conectados disponíveis para serem convidados para o canal\n";

    string mensagemLida;

    //enquanto o usuario nao digitar /connect, ele nao pode 
    //prosseguir para o chat
    do{
        cout << "\nDigite /connect para começar: ";
        getline(cin, mensagemLida);
    }while(mensagemLida != "/connect");

    cout << "-------- Iniciando Chat --------\n";

    //estabelece a conexao e retorna o socket criado
    int client_socket = initializeChat(argv[1]);
    //troca de mensagens com o servidor
    chat(client_socket);
    //fecha o socket
    close(client_socket);

    return 0;
}