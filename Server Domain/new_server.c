// The 'server.c' code goes here.

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include "Md5.c"  // Feel free to include any other .c files that you need in the 'Server Domain'.
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#define client_port 9999
#define MAXLINE 100
#define CHUNK 1000
#define MD5_DIGEST_LENGTH 16
#define MINI 10


struct sockaddr_in address;
typedef struct file_mutex{
    char filename[MAXLINE];
    pthread_mutex_t mutex;
}file_mutex;

struct file_mutex fileMutex[10];
int structsize = 0;


void *thread(void *vargp);
void parseline (char *cmdline, char **argv);
void MDFile (char* filename, unsigned char* md5);
int check_lock (char* filename, int *checker);
void cmd_append (char* filename, int connfd);
void cmd_upload (char* filename, int connfd);
void cmd_download (char* filename, int connfd);
void cmd_delete (char* filename, int connfd);
void cmd_syncheck (char* filename, int connfd);
void cmd_process (int connfd);


int check_lock (char* filename, int *checker) {
    int i;
    for (i = 0; i < MINI; i++) {
        if (strcmp(filename, fileMutex[i].filename) == 0) {
            printf("%s, %d\n",fileMutex[i].filename, i);
            if (pthread_mutex_trylock(&fileMutex[i].mutex) == 0) {
                pthread_mutex_unlock(&fileMutex[i].mutex);
                printf("Exists unlocked file\n");
                return i; //exists but unlocked
            } else {
                printf("Locked file\n");
                *checker = i;
                return -1; //locked
            }
        }
    }
    printf("Not exists\n");
    return -2; //not exists
}


void MDFile (char* filename, unsigned char* md5) {
    FILE *fptr = fopen (filename, "rb");
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];
    if (fptr == NULL) {
        printf ("%s can't be opened.\n", filename);
        return;
    }
    MD5Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, fptr)) != 0)
        MD5Update (&mdContext, data, bytes);
    MD5Final (&mdContext);
    int i;
    for (i = 0; i < 16; i++){
        md5[i] = mdContext.digest[i];
    }
    fclose (fptr);
}


void cmd_append (char* filename, int connfd) {
    FILE *fptr;
    char signs[MINI];
    if ((fptr = fopen(filename, "r"))) {
        int checker = -10;
        int check = check_lock(filename, &checker);
        if (check == -1){
            strcpy(signs,"locked");
            send(connfd, signs, MINI,0);
            return;
        } else if (check == -2){
            fclose(fptr);
            strcpy(signs,"found");
            send(connfd, signs, MINI,0);
            strcpy(fileMutex[structsize].filename, filename);
            int x = pthread_mutex_lock(&fileMutex[structsize].mutex);
            fptr = fopen(filename, "a+");
            char buf[MAXLINE];
            char *cmd[MAXLINE];
            char tmp_cmd[MAXLINE];
            size_t n;
            while (bzero(buf, MAXLINE), (n = recv(connfd, buf, MAXLINE, 0)) != 0) {
                strcpy(tmp_cmd, buf);
                parseline(tmp_cmd, cmd);
                if (strcmp(cmd[0], "close") == 0) {
                    break;
                }
                int len = strlen(buf);
                if (buf[len-1] == '\n')
                buf[len-1] = '\0';
                fprintf(fptr, "\n%s", buf);
            }
            fclose(fptr);
            int y = pthread_mutex_unlock(&fileMutex[structsize].mutex);
            structsize++;
        } else {
            fclose(fptr);
            strcpy(signs,"found");
            send(connfd, signs, MINI,0);
            char buf[MAXLINE];
            char *cmd[MAXLINE];
            char tmp_cmd[MAXLINE];
            size_t n;
            pthread_mutex_lock(&fileMutex[check].mutex);
            fptr = fopen(filename, "a+");
            while (bzero(buf, MAXLINE), (n = recv(connfd, buf, MAXLINE, 0)) != 0) {
                strcpy(tmp_cmd, buf);
                parseline(tmp_cmd, cmd);
                if (strcmp(cmd[0], "close") == 0) {
                    break;
                }
                int len = strlen(buf);
                if (buf[len-1] == '\n'){
                    buf[len-1] = '\0';
                }
                fprintf(fptr, "\n%s", buf);
            }
            fclose(fptr);
            pthread_mutex_unlock(&fileMutex[check].mutex);
        }
    } else {
        strcpy(signs,"not found");
        send(connfd, signs, MINI,0);
        return;
    }
}


void cmd_upload (char* filename, int connfd) {
    int checker = -10;
    int check = check_lock(filename, &checker);
    char signs[MINI];
    if (check == -1){
        strcpy(signs,"locked");
        send(connfd, signs, MINI,0);
        return;
    }else{
        strcpy(signs,"unlocked");
        send(connfd, signs, MINI,0);
        FILE *fptr;
        fptr = fopen(filename,"wb");
        char file_buffer[CHUNK];
        int filesize;
        int received_bytes;
        int total_bytes = 0;
        recv(connfd, &filesize, sizeof(filesize), 0);
        int file_size = ntohl(filesize);
        while (1){
            bzero(file_buffer, CHUNK);
            int remaining_bytes = file_size - total_bytes;
            if (remaining_bytes <= CHUNK){
                received_bytes = recv(connfd, file_buffer, remaining_bytes, 0);
                total_bytes = total_bytes + received_bytes;
                fwrite(&file_buffer, sizeof(char), received_bytes, fptr);
                break;
            }
            received_bytes = recv(connfd, file_buffer, CHUNK, 0);
            total_bytes = total_bytes + received_bytes;
            fwrite(&file_buffer, sizeof(char), received_bytes, fptr);
        }
        fclose(fptr);
        printf("%d bytes uploaded successfully.\n", (int)file_size);
    }
}


void cmd_download (char* filename, int connfd) {
    FILE *fptr = fopen(filename,"rb");
    char signs[MINI];
    if (fptr == NULL) {
        strcpy(signs,"not found");
        send(connfd, signs, MINI,0);
        return;
    } else {
        int checker = -10;
        int check = check_lock(filename, &checker);
        if (check == -1){
            strcpy(signs,"locked");
            send(connfd, signs, MINI,0);
            return;
        }
        strcpy(signs,"found");
        send(connfd, signs, MINI,0);
        char file_chunk[CHUNK];
        fseek(fptr, 0L, SEEK_END);
        int file_size = ftell(fptr);
        fseek(fptr, 0L, SEEK_SET);
        int filesize = htonl(file_size);
        send(connfd, &filesize, sizeof(filesize), 0);
        int total_bytes = 0;
        int current_chunk_size;
        ssize_t sent_bytes;
        while(1){
            bzero(file_chunk, CHUNK);
            int remaining_bytes = file_size - total_bytes;
            if (remaining_bytes <= CHUNK) {
                current_chunk_size = fread(&file_chunk, sizeof(char), remaining_bytes, fptr);
                sent_bytes = send(connfd, &file_chunk, current_chunk_size, 0);
                total_bytes = total_bytes + sent_bytes;
                break;
            }
            current_chunk_size = fread(&file_chunk, sizeof(char), CHUNK, fptr);
            sent_bytes = send(connfd, &file_chunk, current_chunk_size, 0);
            total_bytes = total_bytes + sent_bytes;
        }
        fclose(fptr);
        printf("%d bytes downloaded successfully.\n", file_size);
    }

}


void cmd_delete (char* filename, int connfd) {
    int checker = -10;
    int check = check_lock(filename, &checker);
    char signs[MINI];
    if (check == -1){
        strcpy(signs,"locked");
        send(connfd, signs, MINI,0);
        return;
    }
    if (remove(filename) == 0) {
        strcpy(signs,"delete");
        send(connfd, signs, MINI,0);
    } else {
        strcpy(signs,"not del");
        send(connfd, signs, MINI,0);
    }
}


void cmd_syncheck (char* filename, int connfd) {
    FILE *fptr = fopen(filename,"rb");
    char signs[MINI];
    if (fptr == NULL) {
        strcpy(signs,"not found");
        send(connfd, signs, MINI,0);
        return;
    } else {
        strcpy(signs,"found");
        send(connfd, signs, MINI,0);
        fclose(fptr);
        int checker = -10;
        int check = check_lock(filename, &checker);
        if (check == -1){
            int lock_status = pthread_mutex_trylock(&fileMutex[checker].mutex);
            while (lock_status !=0){
                lock_status = pthread_mutex_trylock(&fileMutex[checker].mutex);
            }
            pthread_mutex_unlock(&fileMutex[checker].mutex);
            fptr = fopen(filename,"rb");
            fseek(fptr, 0L, SEEK_END);
            int file_size = ftell(fptr);
            fseek(fptr, 0L, SEEK_SET);
            int filesize = htonl(file_size);
            send(connfd, &filesize, sizeof(filesize), 0);
            unsigned char md5[MD5_DIGEST_LENGTH];
            MDFile(filename, md5);
            send(connfd, md5, sizeof(md5),0);
            strcpy(signs,"locked");
            send(connfd, signs, MINI,0);
            fclose(fptr);
        }else{
            fptr = fopen(filename,"rb");
            fseek(fptr, 0L, SEEK_END);
            int file_size = ftell(fptr);
            fseek(fptr, 0L, SEEK_SET);
            int filesize = htonl(file_size);
            send(connfd, &filesize, sizeof(filesize), 0);
            unsigned char md5[MD5_DIGEST_LENGTH];
            MDFile(filename, md5);
            send(connfd, md5, sizeof(md5),0);
            strcpy(signs,"unlocked");
            send(connfd, signs, MINI,0);
            fclose(fptr);
        }
    }

}


void cmd_process (int connfd) {
    size_t n;
    char buf[MAXLINE];
    char *cmd[MAXLINE];
    while (bzero(buf, MAXLINE), (n = recv(connfd, buf, MAXLINE,0)) != 0) {
        parseline(buf,cmd);
        if (strcmp(cmd[0], "append") == 0) {
            printf("%s %s\n", cmd[0],cmd[1]);
            cmd_append(cmd[1], connfd);
        } else if (strcmp(cmd[0], "upload") == 0) {
            printf("%s %s\n", cmd[0],cmd[1]);
            cmd_upload(cmd[1], connfd);
        } else if (strcmp(cmd[0], "download") == 0) {
            printf("%s %s\n", cmd[0],cmd[1]);
            cmd_download(cmd[1], connfd);
        } else if (strcmp(cmd[0], "delete") == 0) {
            printf("%s %s\n", cmd[0],cmd[1]);
            cmd_delete(cmd[1], connfd);
        } else if (strcmp(cmd[0], "syncheck") == 0) {
            printf("%s %s\n", cmd[0],cmd[1]);
            cmd_syncheck(cmd[1], connfd);
        }

    }
}


void parseline (char *cmdline, char **argv) {
    char *token;
    char *temp = cmdline;
    if (temp[strlen(temp)-1] == '\n')
        temp[strlen(temp)-1] = ' ';
    else
        temp[strlen(temp)] = ' ';
    int argc = 0;
    while ((token = strchr(temp, ' ')) != NULL) {
        *token = '\0';
        argv[argc++] = temp;
        temp = token + 1;
    }
    argv[argc] = NULL;
}


void *thread(void *vargp) {
    int connfd = *((int *) vargp);
    pthread_detach(pthread_self());
    free(vargp);
    cmd_process(connfd);
    close(connfd);
    return NULL;
}


int main (int argc, char **argv) {     //From Lecture slide 12.7
    int listenfd, connfd, *connfdp;
    pthread_t tid;
    char * client_hostname = argv[1];
    int opt = 1;
    int addrlen = sizeof(address);
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    int socket_status = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR , &opt, sizeof(opt));
    if (socket_status) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(client_hostname);
    address.sin_port = htons(client_port);

    int bind_status = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    if (bind_status < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    int listen_status = listen(listenfd, 1);
    if (listen_status < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    chdir("./Remote Directory");
    int i;
    for (i = 0; i < MINI; i++) {
        pthread_mutex_init(&fileMutex[i].mutex,NULL);
    }
    while (1){
        connfdp = malloc(sizeof(int));
        *connfdp = accept(listenfd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (listenfd < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        printf("Connected to (%s, %d)\n", client_hostname, client_port);
        pthread_create(&tid, NULL, thread, connfdp);
    }
    int j;
    for (j = 0; j < MINI; j++) {
        pthread_mutex_destroy(&fileMutex[j].mutex);
    }
    exit(0);
}
