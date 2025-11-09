#include "headers.h"

int numStorageServersConnected = 0;
storageServerRecord storageServers[MAX_NUM_STORAGE_SERVERS];

int numClientsConnected = 0;

int serverFdForSS = -1;
int serverFdForClient = -1;

pthread_mutex_t serverFdLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t storageServerLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clientLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fileLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t LRU_TrieLock = PTHREAD_MUTEX_INITIALIZER;

void handle_signal(int signum) {
    exit(signum);
}

// ----------------------------- Search ---------------------------------------------------------------
access_trieptr Root;

int search(char* path) {
    for (int i = 0; i < numStorageServersConnected; i++) {
        for (int j = 0; j < storageServers[i].response.numPaths; j++) {
            if (storageServers[i].response.availablePaths[j][strlen(storageServers[i].response.availablePaths[j]) - 1] == '\n') {
                storageServers[i].response.availablePaths[j][strlen(storageServers[i].response.availablePaths[j]) - 1] = '\0';
            }

            if (strcmp(path, storageServers[i].response.availablePaths[j]) == 0) {
                return i;
            }
        }
    }

    return ERR_NOFILE;
}

access_trieptr create_Node(void) {
    access_trieptr new_node = (access_trieptr)malloc(sizeof(struct all_access_path_trie));
    if (new_node != NULL) {
        new_node->storageServerNo = -1;
        for (int indexe = 0; indexe < 60;) {
            new_node->sub_letters[indexe++] = NULL;
        }
    }
    return new_node;
}

void insert(access_trieptr Root, char* path, int storageServerNo) {
    access_trieptr pres_ptr = Root;
    for (int i = 0; i < strlen(path);) {
        int indexe = (int)(path[i]);
        if (pres_ptr->sub_letters[indexe] == NULL) {
            pres_ptr->sub_letters[indexe] = create_Node();
        }
        pres_ptr = pres_ptr->sub_letters[indexe];
        i = i + 1;
    }
    pres_ptr->storageServerNo = storageServerNo;
}

access_trieptr copytrie(access_trieptr tocopy, int storageServerNo) {
    access_trieptr new_node = (access_trieptr)malloc(sizeof(struct all_access_path_trie));
    if (new_node != NULL) {
        new_node->storageServerNo = storageServerNo;
        int indexe = 0;
        while (indexe < 123) {
            new_node->sub_letters[indexe] = copytrie(tocopy->sub_letters[indexe], storageServerNo);
            indexe++;
        }
    }
    return new_node;
}

int searchTrie(access_trieptr Root, char* path) {
    int answer;
    for (int i = 0; i < strlen(path);) {
        int indexe = (int)(path[i]);
        if (Root->sub_letters[indexe] == NULL) {
            answer = -1;
            return answer;
        }
        Root = Root->sub_letters[indexe];
        i = i + 1;
    }
    answer = Root->storageServerNo;
    return answer;
}

Node Head, Tail;

void Update(Node Current) {
    Node Prev = Current->prev;
    if (Prev != NULL) {
        if (Current == Tail) {
            Tail = Prev;
        }
    }
    Node Next = Current->next;
    if (Next != NULL) {
        Next->prev = Prev;
    } else if (Next == NULL) {
        Prev->next = NULL;
    }
    if (Prev == NULL) {
        Next->prev = NULL;
    } else if (Prev != NULL) {
        Prev->next = Next;
    }

    Current->next = Head;
    Current->prev = NULL;

    if (!(Head == NULL)) {
        Head->prev = Current;
    }
    Head = Current;
    return;
}

int SearchLRU(char* path) {
    int answer = -1;
    Node temp = Head;
    int i = 0;
    while (i < LRU_SIZE && temp != NULL) {
        if (strcmp(temp->Path, path) == 0) {
            if (Head != temp) {
                Update(temp);
            }
            answer = temp->storageServerIdx;
            return answer;
        }
        temp = temp->next;
        i = i + 1;
    }
    return answer;
}

int len;

void AddLRU(char* path, int storageServerIdx) {
    Node NewNode = (Node)malloc(sizeof(struct NodeStruct));
    NewNode->next = Head;
    strcpy(NewNode->Path, path);
    NewNode->prev = NULL;
    NewNode->storageServerIdx = storageServerIdx;
    if (Head != NULL) {
        Head->prev = NewNode;
    } else if (Head == NULL) {
        Tail = NewNode;
    }
    Head = NewNode;
    if (LRU_SIZE < (++len)) {
        Tail = Tail->prev;
        if (Tail != NULL) {
            free(Tail->next);
            Tail->next = NULL;
        }
        len = LRU_SIZE;
    }
    return;
}

int searchUsingLRU(char* path) {
    int idx = SearchLRU(path);
    if (idx == -1) {
        int num = search(path);
        if (num == -1) {
            return -1;
        } else {
            AddLRU(path, num);
            idx = SearchLRU(path);
            return idx;
        }
    } else if (idx != -1) {
        return idx;
    }
    return idx;
}
// -----------------------------------------------------------------------------------------------------
int ssAccept() {
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((new_socket = accept(serverFdForSS, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
        perror("Server Error: ");
        printf("Error accepting connection. ERR_CODE: %d\n", ERR_ACCEPT);
        return ERR_ACCEPT;
    }

    return new_socket;
}

int initialiseNMForSS() {
    int servFd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servStruct;

    memset(&servStruct, 0, sizeof(servStruct));

    servStruct.sin_family = AF_INET;
    servStruct.sin_port = htons(NAMING_SERVER_PORT);
    servStruct.sin_addr.s_addr = inet_addr(NAMING_SERVER_IP);

    int errBind = bind(servFd, (struct sockaddr*)&servStruct, sizeof(servStruct));
    if (errBind < 0) {
        perror("Bind Error: ");
        printf("Error: Cannot Bind at port %d. ERR_CODE: %d\n", NAMING_SERVER_PORT, ERR_BIND);
        return ERR_BIND;
    }

    int errListen;
    if ((errListen = listen(servFd, MAX_NUM_STORAGE_SERVERS)) < 0) {
        perror("Listen Error: ");
        printf("Error: Cannot Listen at port %d. ERR_CODE: %d\n", NAMING_SERVER_PORT, ERR_LISTEN);
        return ERR_LISTEN;
    }

    pthread_mutex_lock(&serverFdLock);
    serverFdForSS = servFd;
    pthread_mutex_unlock(&serverFdLock);

    return 0;
}

int initialiseNMForClient() {
    int servFd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servStruct;

    memset(&servStruct, 0, sizeof(servStruct));

    servStruct.sin_family = AF_INET;
    servStruct.sin_port = htons(NAMING_SERVER_CLIENT_PORT);
    servStruct.sin_addr.s_addr = inet_addr(NAMING_SERVER_IP);

    int errBind = bind(servFd, (struct sockaddr*)&servStruct, sizeof(servStruct));
    if (errBind < 0) {
        perror("Bind Error: ");
        printf("Error: Cannot Bind at port %d. ERR_CODE: %d\n", NAMING_SERVER_CLIENT_PORT, ERR_BIND);
        return ERR_BIND;
    }

    int errListen;
    if ((errListen = listen(servFd, MAX_NUM_CLIENTS)) < 0) {
        perror("Listen Error: ");
        printf("Error: Cannot Listen at port %d. ERR_CODE: %d\n", NAMING_SERVER_CLIENT_PORT, ERR_LISTEN);

        return ERR_LISTEN;
    }
    // printf("\n\nNaming Server initialised\n. Waiting for storage servers to connect...\n");

    pthread_mutex_lock(&serverFdLock);
    serverFdForClient = servFd;
    pthread_mutex_unlock(&serverFdLock);

    return 0;
}

void* addStorageServer() {
    while (1) {
        int new_socket = ssAccept();
        if (new_socket == ERR_ACCEPT) {
            continue;
        }

        storageServerResponse response;
        for (int i = 0; i < MAX_NUM_PATHS; i++) {
            strcpy(response.availablePaths[i], "");
        }
        ssize_t errRead = recv(new_socket, &response, sizeof(response), 0);
        if (errRead < 0) {
            perror("Read Error: ");
            printf("Error: Failed to read from storage server. ERR_CODE: %d\n", ERR_READ);

            continue;
        }

        pthread_mutex_lock(&storageServerLock);

        printf("Storage server %d connected.\n", numStorageServersConnected);
        storageServerRecord record;
        record.idx = numStorageServersConnected;
        record.response = response;
        storageServers[numStorageServersConnected] = record;
        storageServers[numStorageServersConnected].serverSocket = new_socket;

        numStorageServersConnected++;
        pthread_mutex_unlock(&storageServerLock);

        pthread_mutex_lock(&fileLock);
        FILE* fp;
        fp = fopen("log.txt", "a");
        fprintf(fp, ">>  New SS Connection!\n");
        printf(">>  New SS Connection!\n");
        fprintf(fp, "Storage server %d connected.\n", numStorageServersConnected);
        printf("Storage server %d connected.\n", numStorageServersConnected);
        fprintf(fp, "IP Address: %s\nNM Port: %d\nClient Port: %d\n", response.ip, response.nmPort, response.clientPort);
        printf("IP Address: %s\nNM Port: %d\nClient Port: %d\n", response.ip, response.nmPort, response.clientPort);

        fprintf(fp, "Available Paths:\n");
        printf("Available Paths:\n");
        for (int i = 0; i < response.numPaths; i++) {
            fprintf(fp, "%s\n", response.availablePaths[i]);
            printf("%s\n", response.availablePaths[i]);

            pthread_mutex_lock(&LRU_TrieLock);
            insert(Root, response.availablePaths[i], record.idx);
            pthread_mutex_unlock(&LRU_TrieLock);
        }
        fprintf(fp, "\n>>\n");
        printf("\n>>\n");

        fclose(fp);
        pthread_mutex_unlock(&fileLock);
    }
    return NULL;
}

int createDeleteCopy(client c, char* path, char* prompt, operation op, char* dest) {
    int storageServerIdx1 = searchUsingLRU(path);
    if (storageServerIdx1 == -1) {
        printf("Error: File Does Not Exist, ERR_NO: %d\n", ERR_NOFILE);

        toClient cr;
        cr.clientPort = -1;

        ssize_t errWrite = write(c.clientSocket, &cr, sizeof(cr));
        if (errWrite < 0) {
            printf("Error: Client Disconnected ERR_NO: %d \n", ERR_CLIENT);
            return ERR_CLIENT;
        }

        pthread_mutex_lock(&fileLock);
        FILE* fp;
        fp = fopen("log.txt", "a");
        fprintf(fp, "Error: Could not find the given file %s in any of the storage servers.\n\n", path);
        printf("Error: Could not find the given file %s in any of the storage servers.\n\n", path);
        fclose(fp);
        pthread_mutex_unlock(&fileLock);

        ack ack;
        ack = ERR_NOFILE;
        errWrite = send(c.clientSocket, &ack, sizeof(ack), 0);
        if (errWrite < 0) {
            printf("Error: Client Disconnected ERR_NO: %d \n", ERR_CLIENT);
            return ERR_CLIENT;
        }

        return ERR_NOFILE;
    }

    int NMsocket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in SS_server;

    SS_server.sin_family = AF_INET;
    SS_server.sin_port = htons(storageServers[storageServerIdx1].response.nmPort);
    SS_server.sin_addr.s_addr = inet_addr(storageServers[storageServerIdx1].response.ip);

    int connectstatus = connect(NMsocket, (struct sockaddr*)&SS_server, sizeof(SS_server));
    if (connectstatus == -1) {
        perror("connect");
        printf("Error: Cannot connect to storage server %d. ERR_NO: %d\n", storageServerIdx1, ERR_CONNECT);

        pthread_mutex_lock(&fileLock);
        FILE* fp;
        fp = fopen("log.txt", "a");
        fprintf(fp, "Error: Cannot connect to storage server %d. ERR_NO: %d\n", storageServerIdx1, ERR_CONNECT);
        fclose(fp);
        pthread_mutex_unlock(&fileLock);
    }

    int errWrite = send(NMsocket, prompt, 256, 0);
    if (errWrite < 0) {
        perror("Write");
        printf("Error: Cannot write to storage server %d. ERR_NO: %d\n", storageServerIdx1, ERR_WRITE);
        return ERR_WRITE;
    }

    ack a;
    int errRead = recv(NMsocket, &a, sizeof(a), 0);
    if (errRead < 0) {
        perror("Read");
        printf("Error: Cannot read to storage server %d. ERR_NO: %d\n", storageServerIdx1, ERR_WRITE);
        return ERR_READ;
    }
    if (a != 0) {
        pthread_mutex_lock(&fileLock);
        printf("Error: Error in Storage Server, ERR_NO: %d\n", a);
        FILE* fp;
        fp = fopen("log.txt", "a");
        fprintf(fp, "Error: Error in Storage Server, ERR_NO: %d\n", a);
        fclose(fp);
        pthread_mutex_unlock(&fileLock);

        return a;
    }

    if (op == CREATE) {
        strcat(path, dest);

        pthread_mutex_lock(&storageServerLock);
        storageServers[storageServerIdx1].response.numPaths++;
        strcpy(storageServers[storageServerIdx1].response.availablePaths[storageServers[storageServerIdx1].response.numPaths - 1], path);
        pthread_mutex_unlock(&storageServerLock);

        pthread_mutex_lock(&LRU_TrieLock);
        insert(Root, path, storageServerIdx1);
        pthread_mutex_unlock(&LRU_TrieLock);

        pthread_mutex_lock(&fileLock);
        printf("Added The path %s in storage server %d\n", path, storageServerIdx1);
        FILE* fp;
        fp = fopen("log.txt", "a");
        fprintf(fp, "Added The path %s in storage server %d\n", path, storageServerIdx1);
        fclose(fp);
        pthread_mutex_unlock(&fileLock);

    } else if (op == DELETE) {
        // delete from trie
    }

    ack ack;
    ack = 0;
    errWrite = send(c.clientSocket, &ack, sizeof(ack), 0);
    if (errWrite < 0) {
        perror("Write");
        printf("Error: Cannot read from client. ERR_NO: %d\n", ERR_READ);
        return ERR_READ;
    }

    pthread_mutex_lock(&fileLock);
    FILE* fp;
    fp = fopen("log.txt", "a");
    fprintf(fp, ">>  %s Operation Successful!\n", prompt);
    printf(">>  %s Operation Successful!\n", prompt);
    fclose(fp);
    pthread_mutex_unlock(&fileLock);

    return 0;
}

void* handleClient(void* cl) {
    client* c = (client*)cl;
    // int id = c->id;

    pthread_mutex_lock(&fileLock);
    printf(">> A New Client Connected\n");
    FILE* fp;
    fp = fopen("log.txt", "a");
    fprintf(fp, ">> A New Client Connected\n");
    fclose(fp);
    pthread_mutex_unlock(&fileLock);

    while (1) {
        char path[256] = {0};
        ssize_t errRead = recv(c->clientSocket, path, sizeof(path) - 1, 0);
        if (errRead < 0) {
            printf("Error: Client Disconnected ERR_NO: %d \n", ERR_CLIENT);
            return (void*)ERR_CLIENT;
        }

        char prompt[256] = {0};
        strcpy(prompt, path);

        pthread_mutex_lock(&fileLock);
        FILE* fp;
        fp = fopen("log.txt", "a");
        fprintf(fp, ">> Client Prompts: %s", prompt);
        printf(">> Client Prompts: %s", prompt);
        fclose(fp);
        pthread_mutex_unlock(&fileLock);

        char* tok = strtok(path, " \n");
        if (tok == NULL) {
            continue;
        }

        char oper[20];
        strcpy(oper, tok);

        if (strcmp(tok, "READ") == 0 || strcmp(tok, "WRITE") == 0 || strcmp(tok, "INFO") == 0 || strcmp(tok, "LIST") == 0) {
            tok = strtok(NULL, " \n");
            if (tok == NULL) {
                continue;
            }
            strcpy(path, tok);
            readWriteInfo(*c, path);
        } else if (strcmp(tok, "CREATE") == 0 || strcmp(tok, "DELETE") == 0) {
            operation op;
            tok = strtok(NULL, " \n");
            if (tok == NULL) {
                printf("No Path is Specified");
                goto end;
            }
            strcpy(path, tok);
            tok = strtok(NULL, " \n");
            char dest[MAX_PATH_LENGTH] = {0};
            if (tok != NULL) {
                strcpy(dest, tok);
            } else {
                printf("No Destination specified\n");
                goto end;
            }

            if (strcmp(oper, "CREATE") == 0) {
                op = CREATE;
            } else if (strcmp(oper, "DELETE") == 0) {
                op = DELETE;
            }
            createDeleteCopy(*c, path, prompt, op, dest);
        } else if (strcmp(tok, "COPY") == 0) {
        }

    end:
        close(c->clientSocket);
        pthread_mutex_lock(&fileLock);
        fp = fopen("log.txt", "a");
        fprintf(fp, ">> Client Disconnected");
        printf(">> Client Disconnected");
        fclose(fp);
        pthread_mutex_unlock(&fileLock);
        break;
    }

    return NULL;
}

void* clientConnection() {
    while (1) {
        int new_socket;
        struct sockaddr_in address;
        int addrlen = sizeof(address);

        if ((new_socket = accept(serverFdForClient, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Server Error: ");
            printf("Error accepting connection. ERR_CODE: %d\n", ERR_ACCEPT);
            return (void*)ERR_ACCEPT;
        }

        client* c = (client*)malloc(sizeof(client));
        if (c == NULL) {
            perror("Memory allocation error: ");
            continue;
        }
        c->id = numClientsConnected;
        c->clientSocket = new_socket;

        pthread_mutex_lock(&clientLock);
        numClientsConnected++;
        pthread_mutex_unlock(&clientLock);

        pthread_t clientThread;
        if (pthread_create(&clientThread, NULL, handleClient, c) != 0) {
            perror("Thread creation error: ");
            free(c);
            continue;
        }
    }
    return NULL;
}

int readWriteInfo(client cl, char* path) {
    int idx = searchUsingLRU(path);
    if (idx == ERR_NOFILE) {
        printf("Error: File Does Not Exist. ERR_NO: %d\n", ERR_NOFILE);

        toClient c;
        c.clientPort = -1;

        ssize_t errWrite = write(cl.clientSocket, &c, sizeof(c));
        if (errWrite < 0) {
            perror("Write");
            printf("Error: Cannot write to client. ERR_NO: %d\n", ERR_READ);
            return ERR_READ;
        }

        return idx;
    }

    toClient c;

    strcpy(c.ip, storageServers[idx].response.ip);
    c.clientPort = storageServers[idx].response.clientPort;

    ssize_t errWrite = write(cl.clientSocket, &c, sizeof(c.clientPort) + sizeof(c.ip));
    if (errWrite < 0) {
        perror("Write");
        printf("Error: Cannot read from client. ERR_NO: %d\n", ERR_READ);
        return ERR_READ;
    }
    pthread_mutex_lock(&fileLock);
    FILE* fp;
    fp = fopen("log.txt", "a");
    fprintf(fp, ">>  Sent Client IP and Port of Storage Server %d\n", idx);
    printf(">>  Sent Client IP and Port of Storage Server %d\n", idx);
    fclose(fp);
    pthread_mutex_unlock(&fileLock);

    return 0;
}

// int copy(client c, int storageServerIdx1, int storageServerIdx2, char* path, char* dest) {
//     // need to complete this
// }

int main() {
    Root = create_Node();

    len = 0;
    Head = NULL;

    // create a file log.txt if it does not exist
    FILE* fp;
    fp = fopen("log.txt", "a");
    fprintf(fp,"----------------------------------------------------New Log----------------------------------------------------\n\n");
    fclose(fp);

    int errInit = initialiseNMForSS();
    if (errInit) {
        perror("Error initialising naming server: ");
        return errInit;
    }
    int errInitClient = initialiseNMForClient();
    if (errInitClient) {
        perror("Error initialising naming server: ");
        return errInitClient;
    }
    printf("Naming Server initialised. Waiting for storage servers to connect...\n");

    pthread_t storageServerThread;
    pthread_t clientThread;

    pthread_create(&storageServerThread, NULL, addStorageServer, NULL);
    pthread_create(&clientThread, NULL, clientConnection, NULL);

    pthread_join(storageServerThread, NULL);
    pthread_join(clientThread, NULL);

    return 0;
}