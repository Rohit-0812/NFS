#ifndef __HEADERS_H
#define __HEADERS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <dirent.h>



// define error codes
#define ERR_SOCKET -17
#define ERR_BIND -16
#define ERR_LISTEN -15
#define ERR_ACCEPT -14
#define ERR_CONNECT -13
#define ERR_READ -12
#define ERR_WRITE -11
#define ERR_CLOSE -10
#define ERR_IP -9
#define ERR_NOFILE -1
#define ERR_CLIENT -2
#define ERR_NOEMPTYDIR -3
#define ERR_NODIR -4
#define ERR_ADDINFO -5
#define ERR_FILEOPEN -6
#define ERR_RECV -7
#define ERR_READDIR -8
#define ERR_SS -18


#define NAMING_SERVER_PORT 8080
#define NAMING_SERVER_CLIENT_PORT 8082
#define NAMING_SERVER_IP "127.0.0.1"
#define MAX_NUM_STORAGE_SERVERS 1000
#define MAX_NUM_CLIENTS 10
#define MAX_PATH_LENGTH 256
#define MAX_NUM_PATHS 100


#define LRU_Size 10

typedef struct {
    char ip[20];
    int nmPort;
    int clientPort;
    int numPaths;
    char availablePaths[MAX_NUM_PATHS][MAX_PATH_LENGTH];   
    
} storageServerResponse;


typedef struct {
    int id;
    int clientSocket;
} client;

typedef struct {
    int idx;
    int serverSocket;
    storageServerResponse response;
} storageServerRecord;

typedef struct {
    char ip[20];
    int clientPort;
} toClient;

typedef enum operation {
    CREATE,
    DELETE,
    COPY
} operation;

typedef struct {
    operation op;
    char path[MAX_PATH_LENGTH];
    char destPath[MAX_PATH_LENGTH];
} toSS;

typedef struct NodeStruct
{
    int storageServerIdx;
    char Path[1024];
    struct NodeStruct* next;
    struct NodeStruct* prev;
}NStruct;

typedef NStruct* Node;

typedef struct all_access_path_trie
{
    int storageServerNo;
    struct all_access_path_trie * sub_letters[123];
}all_access_path_trie;

typedef all_access_path_trie *access_trieptr;

#define LRU_SIZE 10

typedef int ack;

int initialiseNMForSS();
int initialiseNMForClient();
int readWriteInfo(client cl, char* path);

#endif