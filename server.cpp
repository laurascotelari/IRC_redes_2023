#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <vector>
#include <mutex>
#include <csignal>

using namespace std;

#define MAX_MSG 4096

//armazena os sockets dos clientes conectados 
vector<int> clientSockets;
int server_socket;

mutex mtx;

//Função para lidar com as conexões entre cliente e servidor. Essa função será 
//executada em diferentes threads, cada uma com o seu respectivo client_socket 
void handleClient(int client_socket){
    /**
     * Estabelecida a conexão, podemos informar isto pelo terminal. 
     */
    cout << "Cliente " << client_socket << " conectado!" << endl;

    //devido ao fato do vetor de sockets estar em uma area de memória compartilhada
    //(race condition) é necessário utilizar o mutex para assegurar que não haverá
    //inconsistência na escrita
    mtx.lock();
    clientSockets.push_back(client_socket);
    mtx.unlock();

    /**
     * Com a conexão estabelecida, 3-way handshake do TCP e tudo mais, podemos começar
     * a troca de mensagens através do uso das primitivas de socket 'send' e 'recv'.
     * 
     * Precisamos primeiro configurar o buffer que vai receber as mensagens com um tamanho arbitrário de 1024.
     * As mensagens vão sendo recebidas neste buffer, o qual podemos consumir para ler.
     */
    char buffer[MAX_MSG] = {0};
    string message;

    while (true) {
        // Primeiro recebemos a mensagem que o cliente nos enviou.
        recv(client_socket, buffer, MAX_MSG, 0);

        //printando na tela a mensagem enviada pelo cliente
        cout << buffer << endl;

        //A mensagem recebida pelo servidor deve ser enviada para cada cliente conectado  
        for (int i = 0; i < clientSockets.size(); i++ ){
            cout << "Enviando '" << buffer << "' para " << clientSockets[i] << endl;
            // Mandar mensagem para o cliente.
            send(clientSockets[i], buffer, sizeof(buffer), 0);
        }

        // Lida e impressa a mensagem, podemos limpar o buffer para preparar para novas mensagens.
        memset(buffer, 0, sizeof(buffer));
    }

    // Fechar os sockets
    close(client_socket);
}

//caso o usuário aperte Ctrl + C
void signalHandler(int signal) {
    if (signal == SIGINT) {
        cout << "Encerrando o servidor..." << endl;

        //fechando os sockets abertos
        for (int i = 0; i < clientSockets.size(); i++ ){
            close(clientSockets[i]);
        }

        close(server_socket);
        exit(signal);
    }
}

int main() {

    signal(SIGINT, signalHandler);
    /** 
     * Cria um socket para o servidor
     * // AF_INET diz que vamos usar IPv4
     * // SOCK_STREAM diz que vamos usar um stream confiável de bytes (TCP)
     * // IPPROTO_TCP diz que vamos usar TCP. Poderíamos passar zero em 
     *   seu lugar, já que SOCK_STREAM por padrão usarái TCP.
    */
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    /**
     * A struct sockaddr_in server_address vai armazenar as informações
     * do socket que estamos criando para o servidor. No caso, vamos ser um
     * servidor que roda no localhost (127.0.0.1) e escuta na porta 8080.
     */
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8080);

    /**
     * Como somos um servidor, vamos utilizar a primitiva de socket 'bind' para 
     * atrelar nosso socket com as informações de endereço e porta específicos,
     * no caso IP '127.0.0.1' e porta '8080'. 
     */
    bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));

    /**
     * Vamos agora explicitamente escutar e aguardar por conexões.
     * O 3 indica conseguimos receber até 3 conexões que ficam aguardando em uma fila.
     */
    listen(server_socket, 3);

    cout << "Server iniciado. Esperando conexões...\n";


    //vetor de threads para controlar as conexões abertas
    //a cada nova conexão aceita, uma nova thread é criada
    //e adicionada ao vetor
    vector<thread> clientThreads;

    //para o servidor lidar com múltiplas conexões, deve haver um while para detectar
    //uma nova conexao e criar uma thread que lide com a comunicacao com o cliente
    while(true){
        /**
         * Acima dissemos que vamos escutar por conexões. Agora precisamos aceitar estas 
         * conexões para poder lidar com cada uma delas.
         */
        int client_socket;
        struct sockaddr_in client_address;
        socklen_t addrlen = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &addrlen);

        //cria uma nova thread para lidar com a conexao
        clientThreads.emplace_back(handleClient, client_socket);
    }

    //Threads irão alternar seu funcionamento
    for(auto& thread : clientThreads){
        thread.join();
    }

    close(server_socket);

    return 0;
}