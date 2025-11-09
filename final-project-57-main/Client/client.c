#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define Ipaddr "127.0.0.1"
#define PORT 8082

#define MAX_LENGTH 1024
#define MAX_FILES 100

#define ERR_NOEMPTYDIR -1
#define ERR_NODIR -2
#define ERR_NOFILE -3

typedef struct DirectoryInfo {
    char filenames[MAX_FILES][MAX_LENGTH];
    int count;
} DirectoryInfo;

typedef struct Info {
    long int size;
    int permission_id;
    char access_time[MAX_LENGTH];
    char modify_time[MAX_LENGTH];
    char extension[MAX_LENGTH];
} Info;

struct Ipaddress {
    char Ipaddress[20];
    int port;
};

int NMsocket;

typedef struct Ipaddress Ipaddress;
void handle_signal(int signum) {
    close(NMsocket);
    exit(0);
}

void recieveRead(int SSsocket) {
    char* output = (char*)calloc(sizeof(char), 1024);

    printf("Response from the Storage Server: \n");

    do {
        int recvstatus = recv(SSsocket, output, 1023, 0);
        if (recvstatus < 0) {
            perror("recv");
            break;
        }
        if (output[0] == 0) {
            break;
        }
        if (strcmp(output, "FILE DOES NOT EXIST") == 0) {
            printf("Error: FILE DOES NOT EXIST\n");
            return;
        }
        printf("%s", output);
    } while (output[0] != '\0');

    printf("\n\nEnd Response.\n");
}

void client() {
    while (1) {
        NMsocket = socket(AF_INET, SOCK_STREAM, 0);
        if (NMsocket < 0) {
            perror("recv");
            printf("Error: Please Reconnect.\n");
            exit(0);
        }

        struct sockaddr_in NMser_addr;

        NMser_addr.sin_family = AF_INET;
        NMser_addr.sin_port = htons(PORT);
        NMser_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        int connectstatus;
        if ((connectstatus = connect(NMsocket, (struct sockaddr*)&NMser_addr, sizeof(NMser_addr))) == -1) {
            perror("recv");
            printf("Error: Please Reconnect.\n");
            exit(0);
        }
       
        char input[1024];
        printf(">> ");

        fgets(input, 1024, stdin);
        if (input[0] == '\n') {
            goto cont;
        }

        char* string;
        char* tempinput;
        string = (char*)calloc(sizeof(char), 20);
        tempinput = (char*)calloc(sizeof(char), 1024);

        strcpy(tempinput, input);

        strcpy(string, strtok(tempinput, " \n"));

        free(tempinput);

        if (strcmp(string, "READ") == 0 || strcmp(string, "WRITE") == 0 || strcmp(string, "INFO") == 0 || strcmp(string, "LIST") == 0) {
            int sendstatus = send(NMsocket, input, 1024, 0);
            if (sendstatus < 0) {
                perror("send");
                printf("Error: Please Reconnect.\n");
                exit(0);
            }

            Ipaddress SS_info;


            int recvstatus = recv(NMsocket, (Ipaddress*)&SS_info, sizeof(SS_info.Ipaddress) + sizeof(SS_info.port), 0);
            if (recvstatus < 0) {
                perror("recv");
                printf("Error: Please Reconnect.\n");
                exit(0);
            }
            if (SS_info.port == -1) {
                printf("Error: Sorry, File Not Found\n");
                goto cont;
            }

            printf("\nFound the Correct Storage Server.\n");

            int SSsocket = socket(AF_INET, SOCK_STREAM, 0);
            if (SSsocket < 0) {
                perror("Storage Server Socket");
                printf("Error: Cannot Connect to Storage Server\n");
                goto cont;
            }

            struct sockaddr_in SSser_addr;

            SSser_addr.sin_family = AF_INET;
            SSser_addr.sin_port = htons(SS_info.port);
            SSser_addr.sin_addr.s_addr = inet_addr(SS_info.Ipaddress);

            int connectstatus;
            if ((connectstatus = connect(SSsocket, (struct sockaddr*)&SSser_addr, sizeof(SSser_addr))) == -1) {
                printf("Error: Cannot Connect to Storage Server. Please Retry your request.\n");
                close(SSsocket);
            }

            sendstatus = send(SSsocket, input, 1024, 0);
            if (sendstatus < 0) {
                perror("Storage Server Socket");
                printf("Error: Cannot Connect to Storage Server\n");
                goto cont;
            }

            if (strcmp(string, "READ") == 0) {
                recieveRead(SSsocket);
                int ack;
                recvstatus = send(SSsocket, &ack, sizeof(ack), 0);
                if (recvstatus < 0) {
                    perror("Storage Server Socket");
                    printf("Error: Cannot Connect to Storage Server\n");
                    goto cont;
                }

            } else if (strcmp(string, "WRITE") == 0) {
                int ack;
                recvstatus = recv(SSsocket, &ack, sizeof(ack), 0);
                if (recvstatus < 0) {
                    perror("Storage Server Socket");
                    printf("Error: Cannot Connect to Storage Server\n");
                    goto cont;
                }
                if (ack != 0) {
                    printf("Error: Operation Failed, Please Retry. ERR_NO: %d\n",ack);
                } else {
                    printf("Operation Successfull!\n");
                }
            } else if (strcmp(string, "INFO") == 0) {
                Info info;
                recvstatus = recv(SSsocket, &info, sizeof(info), 0);
                if (recvstatus < 0) {
                    perror("Storage Server Socket");
                    printf("Error: Cannot Connect to Storage Server\n");
                    goto cont;
                }
                if (info.permission_id == -1) {
                    printf("Error: File Not Found\n");

                    int ack = -1;
                    recvstatus = send(SSsocket, &ack, sizeof(ack), 0);
                    if (recvstatus < 0) {
                        perror("recv");
                    }

                    close(SSsocket);
                    goto cont;
                }

                else {
                    printf("File Info:\n\n");
                    printf("Size: %ld\n", info.size);
                    printf("Permission: %o\n", info.permission_id);
                    printf("Access Time: %s\n", info.access_time);
                    printf("Modify Time: %s\n", info.modify_time);
                    printf("Extension: %s\n", info.extension);
                    printf("\n");

                    // send ack 0 to client
                    int ack = 0;
                    recvstatus = send(SSsocket, &ack, sizeof(ack), 0);
                    if (recvstatus < 0) {
                        perror("Storage Server Socket");
                        printf("Error: Cannot Connect to Storage Server\n");
                        goto cont;
                    }
                }
            } else if (strcmp(string, "LIST") == 0) {
                DirectoryInfo dirInfo;
                recvstatus = recv(SSsocket, &dirInfo, sizeof(dirInfo), 0);
                if (recvstatus < 0) {
                    perror("recv");
                }
                if (dirInfo.count == -1) {
                    printf("Error: File Not Found\n");

                    int ack = ERR_NOFILE;
                    recvstatus = send(SSsocket, &ack, sizeof(ack), 0);
                    if (recvstatus < 0) {
                        perror("Storage Server Socket");
                        printf("Error: Cannot Connect to Storage Server\n");
                        goto cont;
                    }

                    close(SSsocket);
                    goto cont;
                }

                else {
                    printf("Files in the directory:\n");
                    for (int i = 0; i < dirInfo.count; i++) {
                        printf("%s\n", dirInfo.filenames[i]);
                    }

                    // send ack 0 to client
                    int ack = 0;
                    recvstatus = send(SSsocket, &ack, sizeof(ack), 0);
                    if (recvstatus < 0) {
                        perror("Storage Server Socket");
                        printf("Error: Cannot Connect to Storage Server\n");
                        goto cont;
                    }
                }
            }
            close(SSsocket);

        } else if (strcmp(string, "CREATE") == 0) {
            int sendstatus = send(NMsocket, input, 1024, 0);
            if (sendstatus < 0) {
            }

            int ack;
            int recvstatus = recv(NMsocket, &ack, sizeof(ack), 0);
            if (recvstatus < 0) {
                perror("recv");
                printf("Error: Please Reconnect.\n");
                exit(0);
            }
            if (ack != 0) {
                printf("Error: CREATE Operation failed. ERR_NO: %d\n",ack);
            }
            else{
                printf("Operation Successfull!\n");
            }
            

        } else if (strcmp(string, "DELETE") == 0) {
            int sendstatus = send(NMsocket, input, 1024, 0);
            if (sendstatus < 0) {
            }

            int ack;
            int recvstatus = recv(NMsocket, &ack, sizeof(ack), 0);
            if (recvstatus < 0) {
                perror("recv");
                printf("Error: Please Reconnect.\n");
                exit(0);
            }
            if (ack != 0) {
                printf("Error: DELETE Operation failed. ERR_NO: %d\n",ack);
            }
            else{
                printf("Operation Successfull!\n");
            }
        } else if (strcmp(string, "COPY") == 0) {
        } else {
            printf("Error: No command: %s\n", input);
        }

    cont:
        int confirm = close(NMsocket);
        if (confirm < 0) {
            perror("recv");
            printf("Error: Please Reconnect.\n");
            exit(0);
        }
    }
}

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("Connceting to NM......\n\n");

    printf("Connected to NM.\n");
    while (1) {
        client();
    }

    return 0;
}