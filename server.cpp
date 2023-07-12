#include <iostream>
#include <csignal>
#include "server.h"

using namespace std;

int main() {
    signal(SIGINT, signalHandlerServer);

    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8080);

    bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));

    listen(server_socket, 3);

    cout << "Server iniciado. Esperando conexões...\n";

    vector<thread> clientThreads;

    while(true){
        int client_socket;
        struct sockaddr_in client_address;
        socklen_t addrlen = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &addrlen);

        clientThreads.emplace_back(handleClient, client_socket);
    }

    for(auto& thread : clientThreads){
        thread.join();
    }

    close(server_socket);

    return 0;
}
