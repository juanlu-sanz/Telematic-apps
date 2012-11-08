#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#define SERVER_MODE 0
#define CLIENT_TCP_MODE 1
#define CLIENT_UDP_MODE 2
#define DEBUG(d) if(debug_mode==1) {printf("[DEBUG %d] ", getpid()); printf(d); printf("\n");}
#define ERROR(e) printf("[ERROR FATAL] "); printf(e); exit(-1);

int sockfd;
int acceptfd;
char debug_mode;

int help(int argc, char *argv[]) {
    printf("\nEste programa puede ser usado de dos formas: como cliente o servidor\n");
    printf("El uso es el siguiente: atdate [-h serverhost] [-p port] [-m cu | ct | s ] [-d]\n");
    printf("\t[-h serverhost] indica a que servidor conectarse. Solo cuando esta en modo cliente.\n");
    printf("\t[-p port] inidica a que puerto del servidor (especificado con -h) debe conectarse. Solo en modo cliente.\n");
    printf("\t[-m modo] indica el modo de ejecucion\n");
    printf("\t  ├── cu: cliente en UDP\n");
    printf("\t  ├── ct: cliente en TCP\n");
    printf("\t  └── s:  servidor\n");
    return 0;
}

void signal_handler(int signal) {
    printf("[CTRL+C] Finalizando...\n");
    close(sockfd);
    DEBUG("Cerrando socket..\n");
    close(acceptfd);
    DEBUG("Socket cerrado.\n");
    exit(1);

}

void sigpipe_handler(int signal) {
    int pid = getpid();
    printf("[%d] Conexion cerrada.\n", pid);
    close(sockfd);
    exit(1);
}

void new_client (char* serverhost_str, int type_of_connection, int port){

    if (type_of_connection == CLIENT_UDP_MODE) {
        printf("Iniciando modo cliente UDP a servidor %s\n", serverhost_str); 
    } else {
        printf("Iniciando modo cliente TCP a servidor %s\n", serverhost_str); 
    }

    struct hostent *he;
    struct in_addr a;
    he = gethostbyname (serverhost_str);
    if (he) {
        printf("Preguntando a %s ", he->h_name);
        bcopy(*he->h_addr_list++, (char *) &a, sizeof(a));
        printf("en %s\n", inet_ntoa(a));
    }

    time_t time_int;
    uint32_t recvt;
    struct sockaddr_in client_address;
    client_address.sin_family = AF_INET;
    client_address.sin_addr.s_addr = inet_addr(inet_ntoa(a));
    client_address.sin_port = htons(port);
    if(type_of_connection == CLIENT_UDP_MODE) {
        sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    } else {
        sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    }
    memset(&(client_address.sin_zero), '\0', 8);

    int result = connect(sockfd, (struct sockaddr *)&client_address, sizeof(struct sockaddr));
    if(type_of_connection == CLIENT_TCP_MODE && result == -1) {ERROR("Error al abrir conexión.")};
    if(type_of_connection == CLIENT_UDP_MODE) {
        DEBUG("Enviando datagrama vacio...");
        send(sockfd, NULL, 0, 0); //Enviamos datagrama vacío
        recv(sockfd, &recvt, 4, 0);
        DEBUG("Recibida fecha..");
        time_int = ntohl(recvt) - 2208988800; //Timestamp correspondiente al 0 para Linux (70 años)
        printf("%s", ctime(&time_int));
    }else{
        while(recv(sockfd, &recvt, 4, 0) != 0) {
            time_int = ntohl(recvt) - 2208988800; //Timestamp correspondiente al 0 para Linux (70 años)
            printf("%s", ctime(&time_int));
        }
    }
}

void server(int port) {

    struct sockaddr_in serveraddr;

    unsigned int clientlen = 20;
    time_t current_time;

    if(port == 37) port = 6002;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    //Usamos SO_REUSEADDR para que no tenga que esperar a que se libere el bind() del puerto
    //en caso de detención brusca.
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); //Escuchamos en cualquier dirección
    serveraddr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {ERROR("Error en bind")};
    if (listen(sockfd, 10) < 0) {ERROR("Error en listen")};
    printf("Servidor iniciado y escuchando en el puerto %d.\n", port);
    uint32_t send_buffer;
    while (1) {
        struct sockaddr_in clientaddr;
        acceptfd = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
        if(fork() == 0) {
            int pid = getpid();
            int ret;
            printf("[%d] [%s] Conexión abierta. Enviando informacion.\n", pid, inet_ntoa((struct in_addr)clientaddr.sin_addr));
            do{
                pid = getpid();
                time(&current_time);
                send_buffer = htonl(current_time + 2208988800);
                sleep(1);
                printf("[%d] [%s] Enviando fecha.\n", pid, inet_ntoa((struct in_addr)clientaddr.sin_addr));
                ret = send(acceptfd, &send_buffer, sizeof(uint32_t), 0);
                if(debug_mode==1) printf("[DEBUG] %d bytes enviados\n", ret);
            }while( ret >0 );
            printf("[%d] Conexion cerrada!\n", pid);
            return;
        }
    }
}
int main(int argc, char *argv[]) {
    /* Parseamos los argumentos */
    if (argc == 1) {
        printf("ERROR - Parametros incorrectos.\n");
        printf("Sintaxis: %s [-h serverhost] [-p port] [-m cu | ct | s ] [-d]\n", argv[0]);
        return -1;
    }
    signal(SIGPIPE, sigpipe_handler);
    signal(SIGINT, signal_handler);

    int param_c;
    char* serverhost_str;
    char* port_str;
    char* mode_str;
    char debug_mode = 0;
    int port = 37;
    for (param_c = 1; param_c < argc; param_c++) {
        if (strcmp(argv[param_c], "--help") == 0) {
            return help(argc, argv);
        }
        if (strcmp("-d", argv[param_c]) == 0) {
            debug_mode = 1;
        } else {
            if (param_c + 2 > argc) {
                printf("ERROR: Parametros invalidos.\n");
                return -1;
            }
            if (strcmp("-h", argv[param_c]) == 0) {
                serverhost_str = strdup(argv[param_c + 1]);
            } else if (strcmp("-m", argv[param_c]) == 0) {
                mode_str = strdup(argv[param_c + 1]);
            } else if (strcmp("-p", argv[param_c]) == 0) {
                port_str = strdup(argv[param_c + 1]);
                port = atoi(port_str);
            } else {
                printf("ERROR: Parametro %s no reconocido.\n", argv[param_c]);
                return -1;
            }

            param_c++;
        }

    }

    if (strcmp(mode_str, "cu") == 0) {
        new_client(serverhost_str, CLIENT_UDP_MODE, port);
    }else if (strcmp(mode_str, "ct") == 0) {
        new_client(serverhost_str, CLIENT_TCP_MODE, port);
    }else if (strcmp(mode_str, "s") == 0) {
     server(port);
 }else{
    printf("Modo incorrecto.\n");
    help(argc, argv);
}
return 0;
}

