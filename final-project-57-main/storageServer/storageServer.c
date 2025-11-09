#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#define MAX_LENGTH 1024
#define MAX_FILES 100

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


typedef struct Info
{
    long int size;
    int permission_id;
    char access_time[MAX_LENGTH];
    char modify_time[MAX_LENGTH];
    char extension[MAX_LENGTH];
} Info;

typedef struct DirectoryInfo {
    char filenames[MAX_FILES][MAX_LENGTH];
    int count;
} DirectoryInfo;

int NMsocketID;
int SSsocketID_NM;
int SSsocketID_client;

pthread_mutex_t mutex;
pthread_cond_t cond;
int readers = 0;

#define IP_addr "127.0.0.1"
#define NM_PORT 1234
#define Client_PORT 4321
#define MAX_PATHS 10

typedef struct threadArg {
    int clientsocket;
    char filename[256];
    int op;
} threadArg;

/* Signal handler in case Ctrl-Z or Ctrl-D is pressed -> so that the socket gets closed */
void handle_signal(int signum) {
    close(NMsocketID);
    close(SSsocketID_NM);
    close(SSsocketID_client);
    exit(signum);
}

/* Function to close the socket */
void closeSocket() {
    close(NMsocketID);
    exit(1);
}

typedef struct {
    char ip[20];
    int nmPort;
    int clientPort;
    int numPaths;
    char availablePaths[100][256];

} storageServerResponse;

DirectoryInfo* listing_all_files_and_folders(char folder_name[MAX_LENGTH]) {
    DIR* dir = opendir(folder_name);
    if (dir == NULL) {
        perror("opendir");
        return NULL;
    }

    DirectoryInfo* info = malloc(sizeof(DirectoryInfo));
    info->count = 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;  // Skip . and ..
        }
        strcpy(info->filenames[info->count], entry->d_name);

        if (entry->d_type == 4) {
            strcat(info->filenames[info->count], "/");  
        }

        info->count++;
    }

    closedir(dir);
    return info;
}

void extractSubstring(const char* input, const char* startDelimiter, const char* endDelimiter, char* output, size_t outputSize) {

    const char* start = strstr(input, startDelimiter);
    if (start == NULL) {
        fprintf(stderr, "Start delimiter not found\n");
        return;
    }

    start += strlen(startDelimiter);

    const char* end = strstr(start, endDelimiter);
    if (end == NULL) {
        fprintf(stderr, "End delimiter not found\n");
        return;
    }

    size_t length = end - start;

    if (length + 1 > outputSize) {
        fprintf(stderr, "Output buffer too small\n");
        return;
    }

    strncpy(output, start, length);
    output[length] = '\0';
}

int additional_information_of_file(char file_name[MAX_LENGTH], Info *details)
{
    int identify = 0;
    char file_path[MAX_LENGTH];
    strcpy(file_path, file_name);
    struct stat file_stat;

    if (stat(file_path, &file_stat) == -1)
    {
        identify = ERR_ADDINFO; // Error in getting the information
    }
    else
    {
        printf("File details fetched successfully.\n");

        details->size = file_stat.st_size;
        details->permission_id = file_stat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
        strcpy(details->access_time, ctime(&file_stat.st_atime));
        strcpy(details->modify_time, ctime(&file_stat.st_mtime));
        // printf("Last Status Change Time: %s", ctime(&file_stat.st_ctime));

        const char *file_extension = strrchr(file_path, '.');
        if (file_extension)
        {
            strcpy(details->extension, file_extension);
            printf("File Extension: %s\n", details->extension);
        }
        else
        {
            strcpy(details->extension,"None");
        }
    }
    return identify;
}

int write_file(char* name, char* content) {
    int identify = 0;
    char* write_buffer;
    write_buffer = (char*)malloc(sizeof(char) * 1024);
    if (name[strlen(name) - 1] == '\n') {
        name[strlen(name) - 1] = '\0';
    }
    FILE* fd = fopen(name, "a");
    if (fd == NULL) {
        identify = ERR_FILEOPEN;
        fprintf(stderr, "Unable to open the fd.\n");
        strcpy(write_buffer, "File does not exist");
    } else {
        fprintf(fd, "%s", content);
        strcpy(write_buffer, "Content is written successfully into the file");
    }

    fclose(fd);

    return identify;
}


int create_file(char* name, char* tok) {
    int identify = 0;
    if (name[strlen(name) - 1] == '\n') {
        name[strlen(name) - 1] = '\0';
    }
    FILE* fd;
    strcat(name, tok);
    if (name[strlen(name) - 1] == '\n') {
        name[strlen(name) - 1] = '\0';
    }
    if(name[strlen(name) - 1] == '/')
    {
        int check = mkdir(name,0777);
        if(check<0)
        {
            identify = ERR_NODIR; // directory not exist
            printf("directory is not created");
            perror("mkdir");
        }

    }
    else
    {
        fd = fopen(name, "w");
        if (fd == NULL) {
            printf("file is not created");
            perror("fopen");
            identify = ERR_NOFILE; // file does not exist
        }
        fclose(fd);
    }

    return 0;
}

int delete_file(char* name, char* tok) {
    int indentify = 0;
    strcat(name, tok);

    if (name[strlen(name) - 1] == '\n') {
        name[strlen(name) - 1] = '\0';
    }
    if(name[strlen(name)]-1 == '/')
    {
        int check = rmdir(name);
        if(check == ENOTEMPTY)
        {
            printf("Directory is not empty\n");
            indentify = ERR_NOEMPTYDIR; // for nonempty directory
        }
        else if(check == ENOENT)
        {
            printf("Directory does not exist\n");
            indentify = ERR_NODIR; // for directory not exist
        }
    }
    else
    {
        int rmv = remove(name);
        if (rmv != 0) {
            perror("error deleting file");
            indentify = ERR_NOFILE; // file does not exist
        }
    }

    return indentify;
}

int sendFile(int clientsocket, char* filename) {
    int identify = 0;
    if(filename[strlen(filename)-1]=='/')
    {
        identify = ERR_READDIR;
        char stop[1024] = {0};
        strcpy(stop,"ERROR: THIS IS NOT A FILE\n");
        int sendstatus = send(clientsocket, stop, 1024, 0);
        if (sendstatus < 0) {
            perror("send");
        }
        return identify;
    }
    else
    {
        FILE* file = fopen(filename, "r");
        if (file == NULL) {
            identify = ERR_FILEOPEN; // Error for file fopen
            perror("File open error");
            // error here
            char* stop = "FILE DOES NOT EXIST";
            int sendstatus = send(clientsocket, &stop, 0, 0);
            if (sendstatus < 0) {
                perror("send");
                // error handle
            }
        }

        char buffer[1024] = {0};
        int bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            int sendstatus = send(clientsocket, buffer, bytesRead, 0);
            if (sendstatus < 0) {
                perror("send");
                fclose(file);
            }
        }
        // send a stop packetS
        char stop[1024] = {0};
        int sendstatus = send(clientsocket, stop, 1024, 0);
        if (sendstatus < 0) {
            perror("send");
            // error handle
            fclose(file);
        } else {
            fclose(file);
        }
    }
    return identify;
}

int sendInfo(int clientsocket, char* filename){
    int identify = 0;
    Info i;
    i.permission_id = -1;
    
    identify = additional_information_of_file(filename,&i);

    //send to client
    int errSend = send(clientsocket,&i,sizeof(Info),0);
    if(errSend<0){
        perror("send");
    }

    //recieve ack
    return identify;
}

int sendList(int clientsocket, char* path){
    DirectoryInfo info;
    info.count = -1;
    
    //send info
    int errSend = send(clientsocket,&info,sizeof(DirectoryInfo),0);
    if(errSend<0){
        perror("send");
    }


}

void* reader(void* arg) {
    int ack = 0;
    threadArg args = *(threadArg*)arg;
    int clientsocket = args.clientsocket;
    char* filename = args.filename;
    int op = args.op;

    pthread_mutex_lock(&mutex);
    readers++;
    pthread_mutex_unlock(&mutex);

    if(op==0){
        ack = sendFile(clientsocket, filename);
    }else if (op==2){
        ack = sendList(clientsocket, filename);
    }
    else{
        ack = sendInfo(clientsocket,filename);
    }
    

    int sendstatus = recv(clientsocket, &ack, sizeof(ack), 0);
    if (sendstatus < 0) {
        ack = ERR_RECV;
        perror("send");
    }

    pthread_mutex_lock(&mutex);
    readers--;
    if (readers == 0) {
        pthread_cond_signal(&cond);
    }
    pthread_mutex_unlock(&mutex);

    // close client
    close(clientsocket);

    return NULL;
}


void* writer(void* arg) {
    int ack = 0;
    threadArg t = *(threadArg*)arg;
    int clientsocket = t.clientsocket;
    char* prompt = t.filename;

    char cpy[256];
    strcpy(cpy,t.filename);

    char* filename = strtok(cpy, " \n"); // WRITE
    filename = strtok(NULL, " \n"); //filename


    pthread_mutex_lock(&mutex);
    while (readers != 0) {
        pthread_cond_wait(&cond, &mutex);
    }

    // Write the data
    char output[1024] = {0};
    extractSubstring(prompt, "\"", "\"", output, 1024);
    ack = write_file(filename, output);

    int sendstatus = send(clientsocket, &ack, sizeof(ack), 0);
    if (sendstatus < 0) {
        perror("send");
    }

    pthread_mutex_unlock(&mutex);

    return NULL;
}

void* clientThread(void* arg) {
    int socket = *(int*)arg;
    char* buffer = (char*)malloc(sizeof(char) * 1024);
    char* prompt = (char*)malloc(sizeof(char) * 1024);
    int recstatus = recv(socket, buffer, 1024, 0);
    if (recstatus < 0) {
        printf("recv error in client thread");
        perror("recv");
    }

    strcpy(prompt, buffer);

    char* temp = (char*)malloc(sizeof(char) * 100);
    strcpy(temp, strtok(buffer, " \n"));

    char* output = (char*)calloc(sizeof(char), 1024);

    if (strcmp(temp, "READ") == 0 || strcmp(temp, "WRITE") == 0 || strcmp(temp,"INFO") == 0) {
        if (strcmp(temp, "READ") == 0) {
            char* filename = strtok(NULL, " \n");
            threadArg socketT;
            socketT.clientsocket = socket;
            socketT.op = 0;
            strcpy(socketT.filename, filename);
            pthread_t reader_thread;
            if (pthread_create(&reader_thread, NULL, reader, &socketT) != 0) {
                perror("Thread creation error: ");
            }
            pthread_join(reader_thread,NULL);
        }

        if (strcmp(temp, "WRITE") == 0) {
            threadArg socketT;
            socketT.clientsocket = socket;
            strcpy(socketT.filename, prompt);
            pthread_t writer_thread;
            if (pthread_create(&writer_thread, NULL, writer, &socketT) != 0) {
                perror("Thread creation error: ");
            }
            pthread_join(writer_thread,NULL);
        }
        if (strcmp(temp, "INFO") == 0) {
            char* filename = strtok(NULL, " \n");
            threadArg socketT;
            socketT.op = 1;
            socketT.clientsocket = socket;
            strcpy(socketT.filename, filename);
            pthread_t reader_thread;
            if (pthread_create(&reader_thread, NULL, reader, &socketT) != 0) {
                perror("Thread creation error: ");
            }
            pthread_join(reader_thread,NULL);
        }

    } 
    return NULL;
}

void* NMThread(void* arg){
    int identity = 0;
    int socket = *(int*)arg;
    char* buffer = (char*)malloc(sizeof(char) * 1024);
    char* prompt = (char*)malloc(sizeof(char) * 1024);
    int recstatus = recv(socket, buffer, 1024, 0);
    if (recstatus < 0) {
        identity = ERR_RECV; // Error in recv
        printf("recv error in client thread");
        perror("recv");
    }

    strcpy(prompt, buffer);


    char* temp = (char*)malloc(sizeof(char) * 100);
    strcpy(temp, strtok(buffer, " \n"));

    char* output = (char*)calloc(sizeof(char), 1024);
    if (strcmp(temp, "CREATE") == 0 || strcmp(temp, "DELETE") == 0) {
        if (strcmp(temp, "CREATE") == 0) {
            temp = strtok(NULL, " ");
            char* tok = strtok(NULL, " ");
            if(tok == NULL)
            {
                // identity = file_create(temp);
            }
            else
            {
                identity = create_file(temp, tok);
            }
            strcpy(output, "New File is Created");
        }

        if (strcmp(temp, "DELETE") == 0) {
            temp = strtok(NULL, " ");
            char* tok = strtok(NULL, " ");
            identity = delete_file(temp, tok);
            strcpy(output, "File is Deleted");
        }
        int ack = identity;
        int sendstaus = send(socket, &ack, sizeof(ack), 0);
        if (sendstaus < 0) {
            perror("send");
        }
    } else if (strcmp(temp, "COPY") == 0) {
    } else {
        printf("Wrong prompt\n");
    }
    return NULL;
}

void* handleClients(void* arg) {
    int SSsocketID_client = *(int*)arg;
    while (1) {
        struct sockaddr_in client;
        int* clientsocket = (int*)malloc(sizeof(int));
        socklen_t clientsize = sizeof(client);
        if ((*clientsocket = accept(SSsocketID_client, (struct sockaddr*)&client, &clientsize)) != -1) {
            pthread_t client_thread;
            printf("thread created for client in SS\n");
            if (pthread_create(&client_thread, NULL, clientThread, clientsocket) != 0) {
                perror("Thread creation error: ");
                free(clientsocket);
            }
            pthread_detach(client_thread);
        }
    }
    return NULL;
}

void* handleNM(void* arg) {
    int SSsocketID_NM = *(int*)arg;
    while (1) {
        struct sockaddr_in NMserver;
        int* NMsocket = malloc(sizeof(int));
        socklen_t NMsize = sizeof(NMserver);
        if ((*NMsocket = accept(SSsocketID_NM, (struct sockaddr*)&NMserver, &NMsize)) != -1) {
            pthread_t NM_thread;
            printf("thread created for NM in SS\n");
            if (pthread_create(&NM_thread, NULL, NMThread, NMsocket) != 0) {
                perror("Thread creation error: ");
                free(NMsocket);
            }
            pthread_detach(NM_thread);
        }
    }
    return NULL;
}


int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    char* argv[] = {"", "127.0.0.1", "8080"};
    char IPaddress[20];
    strcpy(IPaddress, argv[1]);
    int PORT = atoi(argv[2]);
    NMsocketID = socket(AF_INET, SOCK_STREAM, 0);
    if (NMsocketID < 0) {
        perror("Error: opening socket\n");
        exit(1);
    }

    struct sockaddr_in NMserveraddress, clientAddress;
    memset(&NMserveraddress, 0, sizeof(NMserveraddress));

    NMserveraddress.sin_family = AF_INET;
    NMserveraddress.sin_port = htons(PORT);
    NMserveraddress.sin_addr.s_addr = inet_addr(IPaddress);

    storageServerResponse my_info;
    strcpy(my_info.ip, IP_addr);
    my_info.nmPort = NM_PORT;
    my_info.clientPort = Client_PORT;
    my_info.numPaths = 0;

    int index = 0;

    printf("Enter Paths: \n");
    while (1) {
        char temp[50];
        fgets(temp, 50, stdin);

        if (temp[0] == '\n' && temp[1] == '\0') {
            break;
        } else {
            int length = strlen(temp);
            while(length>0 && temp[length-1]==' ')
            {
                length--;
            }
            temp[length] = '\0';
            if(temp[strlen(temp)-1]=='\n')
            {
                temp[strlen(temp)-1]='\0';
            }
            strcpy(my_info.availablePaths[my_info.numPaths], temp);
            my_info.numPaths++;
            index++;
        }
    }

    int connectstatus = connect(NMsocketID, (struct sockaddr*)&NMserveraddress, sizeof(NMserveraddress));
    if (connectstatus == -1) {
        printf("SS init error\n");
        perror("connect");
    }
    int sendstaus = send(NMsocketID, (storageServerResponse*)&my_info, sizeof(storageServerResponse), 0);
    if (sendstaus < 0) {
        printf("SS init error\n");
        perror("send");
    }

    // Bind storage server to naming server port
    SSsocketID_NM = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serveraddr_NM;

    serveraddr_NM.sin_family = AF_INET;
    serveraddr_NM.sin_addr.s_addr = inet_addr(IP_addr);
    serveraddr_NM.sin_port = htons(NM_PORT);

    int bindstatus = bind(SSsocketID_NM, (struct sockaddr*)&serveraddr_NM, sizeof(serveraddr_NM));
    if (bindstatus == -1) {
        printf("Not able to bind in SS\n");
        perror("bind");
        close(SSsocketID_NM);
    }

    int listenstatus = listen(SSsocketID_NM, 5);
    if (listenstatus == -1) {
        printf("Server is unable to listen\n");
        perror("listen");
        close(SSsocketID_NM);
    }

    // Bind storage server to client port
    SSsocketID_client = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serveraddr_client;

    serveraddr_client.sin_family = AF_INET;
    serveraddr_client.sin_addr.s_addr = inet_addr(IP_addr);
    serveraddr_client.sin_port = htons(Client_PORT);

    bindstatus = bind(SSsocketID_client, (struct sockaddr*)&serveraddr_client, sizeof(serveraddr_client));
    if (bindstatus == -1) {
        printf("Not able to bind in SS\n");
        perror("bind");
        close(SSsocketID_client);
    }

    listenstatus = listen(SSsocketID_client, 5);
    if (listenstatus == -1) {
        printf("Server is unable to listen\n");
        perror("listen");
        close(SSsocketID_client);
    }

    pthread_t client_thread, NM_thread;
    pthread_create(&client_thread, NULL, handleClients, &SSsocketID_client);
    pthread_create(&NM_thread, NULL, handleNM, &SSsocketID_NM);

    pthread_join(client_thread, NULL);
    pthread_join(NM_thread, NULL);
}