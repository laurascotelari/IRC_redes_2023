#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <string>

#include <cstdlib>  // usar rand() e srand()
#include <ctime>  

using namespace std;

#define MAX_MSG 4096

string generateNickname(string nickname, int flag){
    srand(time(0));
    
    //gera um nickname aleatorio: cliente + numero aleatorio
    if(flag){
        return nickname;
    }else{
        //pegando um numero aleatrio entre 0 e 100
        int randNum = rand()%100;

        string newNickname;
        newNickname = "cliente" + to_string(randNum);

        return newNickname;
    }
}

int main(int argc, char **argv) {

    //caso o usuario nao tenha digitado o endereco do server
	if (argc != 2) {
		cout << "use: ./client <IPaddress>";
        return -1;
    }

    /** 
     * Cria um socket para o cliente
     * // AF_INET diz que vamos usar IPv4
     * // SOCK_STREAM diz que vamos usar um stream confiável de bytes
     * // IPPROTO_TCP diz que vamos usar TCP. Poderíamos passar zero em 
     *   seu lugar, já que SOCK_STREAM por padrão usarái TCP.
    */
    int client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    /**
     * A struct sockaddr_in server_address vai armazenar as informações
     * do socket a que iremos nos conectar. No caso, nos conectaremos ao
     * localhost (127.0.0.1) na porta 8080 (o servidor deve estar escutando)
     * nesta porta.
     */
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_port = htons(8080); // Porta 8080
    inet_pton(AF_INET, argv[1], &server_address.sin_addr); // Associação com IP do servidor

    /**
     * Vamos agora de fato nos conectar ao servidor. Passamos os nosso socket que criamos (veja que não
     * especificamos em que porta estamos escutando, ou seja, não utilizamos bind, porque nós, como clientes
     * não nos importamos com isso. Temos nosso IP, independentemente de a partir de que porta, queremos nos)
     * conectar ao servidor num endereço IP e porta específica. No caso, sabemos que o servidor ao qual queremos
     * nos conector está rodando no endereço "127.0.0.1" escutando na porta 8080). Passamos em seguida as 
     * informações de endereço do socket do servidor, que configuramos acima. Por fim, o tamanho desta estrutura
     * do socket do servidor (por algum motivo, o C++ precisa disso). 
     */
    connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address));

    /**
     * Uma vez que a conexão foi estabelecida a conexão, informamos isso com uma mensagem no terminal.
     */
    cout << "Você está conectado ao servidor!" << endl;

    /**
     * Com a conexão estabelecida, 3-way handshake do TCP e tudo mais, podemos começar a troca de mensagens
     * através do uso das primitivas de socket 'send' e 'recv'.
     * 
     * Precisamos primeiro configurar o buffer que vai receber as mensagens com um tamanho arbitrário de 1024.
     * As mensagens vão sendo recebidas neste buffer, o qual podemos consumir para ler.
     */
    char buffer[MAX_MSG] = {0};

    //mensagem que sera lida da stdin
    string mensagemLida;
    //mensagem formatada: apelido + mensagemLida
    string mensagem;
    //apelido do cliente 
    //  * flag 0: gera arbritariamente no tipo cliente + numero_aleatorio
    //  * flag 1: recebe o nickname escolhido usando /nickname (ainda nao implementado) 
    string nickname;
    nickname = generateNickname(nickname, 0);

    //controla se o usuario ja mandou o comando /connect
    int flagConnect = false;

    cout << "Comandos importantes:\n";
    cout << "\t/connect : Estabelece conexão com o servidor\n";
    cout << "\t/quit    : Cliente fecha a conexão e fecha a aplicação\n";
    cout << "\t/ping    : Servidor retorna pong assim que receber a mensagem\n";

    do{
        cout << "\nDigite /connect para começar: ";
        getline(cin, mensagemLida);

    }while(mensagemLida != "/connect");

    cout << "-------- Iniciando Chat --------\n";

    while (true) {
        // Ler entrada do usuário para mandar para o servidor.
        cout << "\nEscreva sua mensagem: ";
        getline(cin, mensagemLida);

        //formatando a mensagem do usuario adicionando o seu nickname
        mensagem = nickname + ": " + mensagemLida; 

        cout << "\n\t" << mensagem << "\n\n";  

        // Mandar mensagem ao servidor.
        send(client_socket, mensagem.c_str(), mensagem.length(), 0);

        // Checar se o usuário quer sair caso a mensagem enviada ao servidor tenha sido o comando '/exit'.
        if (mensagemLida == "/quit") break;

        // Receber resposta do servidor.
        recv(client_socket, buffer, MAX_MSG, 0);
        cout << "\tMensagem do Servidor: " << buffer << endl;

        // Limpar buffer de recepção para se preparar para próxima mensagem
        memset(buffer, 0, sizeof(buffer));
    }

    // Fechar o socket
    close(client_socket);

    return 0;
}