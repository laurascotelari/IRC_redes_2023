#include <iostream>
#include <csignal>
#include "client.h"

using namespace std;

int main(int argc, char **argv) {
    signal(SIGINT, signalHandler);

    if (argc != 2) {
        cout << "use: ./client <IPaddress>\n";
        return -1;
    }

    int flagConnect = false;

    cout << "Comandos importantes:\n";
    cout << "\t/connect : Estabelece conexão com o servidor\n";
    cout << "\t/quit    : Cliente fecha a conexão e fecha a aplicação\n";
    cout << "\t/ping    : Servidor retorna pong assim que receber a mensagem\n";

    string mensagemLida;

    do{
        cout << "\nDigite /connect para começar: ";
        getline(cin, mensagemLida);
    }while(mensagemLida != "/connect");

    cout << "-------- Iniciando Chat --------\n";

    int client_socket = initializeChat(argv[1]);
    chat(client_socket);
    close(client_socket);

    return 0;
}