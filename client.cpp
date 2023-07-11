#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <cstdlib>  // usar rand() e srand()
#include <ctime>  
#include <thread>
#include <mutex>
#include <csignal>

using namespace std;

#define MAX_MSG 4096

mutex mtx;
int client_socket;
//flag que controla o fim da conexão (vira true quando o usuário digita /quit)
int flag_fim = false;

/*Funcao initializeChat:
*   Recebe o endereco IP do servidor pelo argv[1], cria um socket e estabelece a conexao
*   com o servidor na porta 8080
*
* Parâmetros:
*   char* server_IP_addr : IP do servidor
*
* Retorno:
*   socket do cliente
*/
int initializeChat(char* server_IP_addr);

/*Funcao chat:
*   Com a conexão já estabelecida no client_socket, a função chat realiza a troca de mensagem
*   entre o servidor e o cliente através das primitivas send() e receive()
*
* Parâmetros:
*   int client_socket : socket com a conexão já estabelecida
*/
void chat(int client_socket);

/*Funcao generateNickname:
*   flag = 0: Gera o apelido do usuário através da junção da string cliente + um número
*   aleatório entre 0 e 100. 
*   flag = 1: Apenas retorna o apelido digitado pelo usuário. 
*
* Parâmetros:
*   int flag : controla o tipo de apelido
*
* Retorno:
*   String de apelido gerada
*/
string generateNickname(string nickname, int flag);
void recebeMensagem(int client_socket);
//caso o usuário aperte Ctrl + C
void signalHandler(int signal);
//verifica se a mensagem digitada e um comando
int isCommand(string mensagem);

int main(int argc, char **argv) {

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


int initializeChat(char* server_IP_addr){
    /** 
     * Cria um socket para o cliente
     * // AF_INET diz que vamos usar IPv4
     * // SOCK_STREAM diz que vamos usar um stream confiável de bytes
     * // IPPROTO_TCP diz que vamos usar TCP. Poderíamos passar zero em 
     *   seu lugar, já que SOCK_STREAM por padrão usarái TCP.
    */
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    /**
     * A struct sockaddr_in server_address vai armazenar as informações
     * do socket a que iremos nos conectar. No caso, nos conectaremos ao
     * localhost (127.0.0.1) na porta 8080 (o servidor deve estar escutando)
     * nesta porta.
     */
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_port = htons(8080); // Porta 8080
    inet_pton(AF_INET, server_IP_addr, &server_address.sin_addr); // Associação com IP do servidor

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

    return client_socket;
}


void chat(int client_socket){
    /**
     * Com a conexão estabelecida, 3-way handshake do TCP e tudo mais, podemos começar a troca de mensagens
     * através do uso das primitivas de socket 'send' e 'recv'.
     * 
     * Precisamos primeiro configurar o buffer que vai receber as mensagens com um tamanho arbitrário de 1024.
     * As mensagens vão sendo recebidas neste buffer, o qual podemos consumir para ler.
     */
    char buffer[MAX_MSG] = {0};

    //mensagem formatada: apelido + mensagemLida
    string mensagem;
    //apelido do cliente 
    //  * flag 0: gera arbritariamente no tipo cliente + numero_aleatorio
    //  * flag 1: recebe o nickname escolhido usando /nickname (ainda nao implementado) 
    string nickname;
    nickname = generateNickname(nickname, 0);

    thread t1(recebeMensagem, client_socket);

    while (true) {
        // Ler entrada do usuário para mandar para o servidor.
        getline(cin, mensagem);
        
        //se a mensagem nao for um comando, devemos adicionar o nickname
        if(!isCommand(mensagem)){
            //formatando a mensagem do usuario adicionando o seu nickname
            mensagem = nickname + ": " + mensagem; 
        }

        // Mandar mensagem ao servidor.
        send(client_socket, mensagem.c_str(), mensagem.length(), 0);

        // Checar se o usuário quer sair caso a mensagem enviada ao servidor tenha sido o comando '/exit'.
        if (mensagem == "/quit" || flag_fim){
            flag_fim = true;
            break; 
        } 

    }

    //finalizando a thread
    t1.join();
}

//verifica se a mensagem digitada e um comando
int isCommand(string mensagem){

    if(mensagem == "/ping" ||
       mensagem == "/quit" ){
        return true;
    }
    return false;
}

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


void recebeMensagem(int client_socket){

    char buffer[MAX_MSG] = {0};
    int num_bytes;

    while(true){

        //verificando se o usuario digitou /quit
        if(flag_fim) break;

        mtx.lock();
        // Receber resposta do servidor.
        if( (num_bytes = recv(client_socket, buffer, MAX_MSG, 0)) <= 0){
            //erro ou conexão foi fechada pelo cliente
            if(num_bytes < 0){
                cout << "Erro no recv\n";
            }

        }else{
            cout << "\t\t" << buffer << endl;

            // Limpar buffer de recepção para se preparar para próxima mensagem
            memset(buffer, 0, sizeof(buffer));

        }
        mtx.unlock();


    }
}

//caso o usuário aperte Ctrl + C
void signalHandler(int signal) {
    if (signal == SIGINT) {
        cout << "Encerrando o cliente..." << endl;

        //fecha o socket
        close(client_socket);

        exit(signal);
    }
}