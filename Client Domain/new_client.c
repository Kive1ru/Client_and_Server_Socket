// The 'client.c' code goes here.

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include "Md5.c"  // Feel free to include any other .c files that you need in the 'Client Domain'.
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#define client_port 9999
#define MAXLINE 100
#define CHUNK 1000
#define MD5_DIGEST_LENGTH 16
#define MINI 10


void parseline (char *cmdline, char **argv);
void MDFile (char* filename, unsigned char* md5);
void cmd_pause (char* line, int pTime, int clientfd, FILE* fp);
void cmd_append (char* line, char* filename, int clientfd, FILE* fp);
void cmd_upload (char* line, char* filename, int clientfd, FILE* fp);
void cmd_download (char* line, char* filename, int clientfd, FILE* fp);
void cmd_delete (char* line, char* filename, int clientfd, FILE* fp);
void cmd_syncheck (char* line, char* filename, int clientfd, FILE* fp);
void cmd_process (char *cmdline, int clientfd, FILE* fp);


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
    for (i = 0; i < MD5_DIGEST_LENGTH; i++){
        md5[i] = mdContext.digest[i];
    }
    fclose (fptr);
}


void cmd_pause (char* line,int pTime, int clientfd, FILE* fp){
    sleep(pTime);
}


void cmd_append (char* line, char* filename, int clientfd, FILE* fp){
    send(clientfd, line, strlen(line), 0);
    char results[MINI];
    if (recv(clientfd, results, MINI, 0) < 0){
        printf("File [%s] could not be found in remote directory.\n", filename);
        return;
    }
    if (strcmp(results, "not found") == 0){
        printf("File [%s] could not be found in remote directory.\n", filename);
        return;
    }
    if (strcmp(results, "locked") == 0){
        printf("File [%s] is currently locked by another user.\n", filename);
        return;
    }
    if (strcmp(results, "found") == 0){
        char cmdline[MAXLINE];
        char* cmd[MAXLINE];
        char tmp_cmd[MAXLINE];
        while (printf("Appending> "), fgets(cmdline, MAXLINE, fp) != NULL) {
            strcpy(tmp_cmd, cmdline);
            parseline(tmp_cmd, cmd);
            if (strcmp(cmd[0], "pause") == 0) {
                int len = strlen(cmdline);
                if (cmdline[len-1] == '\n')
                    printf("%s", cmdline);
                else
                    printf("%s\n", cmdline);
                int pTime = atoi(cmd[1]);
                cmd_pause(line, pTime, clientfd, fp);
                continue;
            }
            int len = strlen(cmdline);
            if (cmdline[len-1] == '\n')
                printf("%s", cmdline);
            else
                printf("%s\n", cmdline);
            send(clientfd, cmdline, strlen(cmdline),0);
            if (strcmp(cmd[0], "close") == 0) {
                break;
            }
        }
    }
}


void cmd_upload (char* line, char* filename, int clientfd, FILE* fp) {
    FILE *fptr;
    fptr = fopen(filename,"rb");
    if (fptr == NULL) {
        printf("File [%s] could not be found in local directory.\n", filename);
        return;
    }
    fclose(fptr);
    char results[MINI];
    send(clientfd, line, strlen(line), 0);
    if (recv(clientfd, results, MINI, 0) < 0){
        printf("Nothing received.\n");
        return;
    }
    if (strcmp(results, "locked") == 0){
        printf("File [%s] is currently locked by another user.\n", filename);
        return;
    }
    if (strcmp(results, "unlocked") == 0) {
        char file_chunk[CHUNK];
        fptr = fopen(filename,"rb");
        fseek(fptr, 0L, SEEK_END);
        int file_size = ftell(fptr);
        fseek(fptr, 0L, SEEK_SET);
        int filesize = htonl(file_size);
        send(clientfd, &filesize, sizeof(filesize), 0);
        int total_bytes = 0;
        int current_chunk_size;
        ssize_t sent_bytes;
        while(1){
            bzero(file_chunk, CHUNK);
            int remaining_bytes = file_size - total_bytes;
            if (remaining_bytes <= CHUNK) {
                current_chunk_size = fread(&file_chunk, sizeof(char), remaining_bytes, fptr);
                sent_bytes = send(clientfd, &file_chunk, current_chunk_size, 0);
                total_bytes = total_bytes + sent_bytes;
                break;
            }
            current_chunk_size = fread(&file_chunk, sizeof(char), CHUNK, fptr);
            sent_bytes = send(clientfd, &file_chunk, current_chunk_size, 0);
            total_bytes = total_bytes + sent_bytes;
        }
        fclose(fptr);
        printf("%d bytes uploaded successfully.\n", file_size);
    }
}


void cmd_download (char* line, char* filename, int clientfd, FILE* fp) {
    char results[MINI];
    send(clientfd, line, strlen(line), 0);
    if (recv(clientfd, results, MINI, 0) < 0){
        printf("File [%s] could not be found in remote directory.\n", filename);
        return;
    }
    if (strcmp(results, "not found") == 0){
        printf("File [%s] could not be found in remote directory.\n", filename);
        return;
    }
    if (strcmp(results, "locked") == 0){
        printf("File [%s] is currently locked by another user.\n", filename);
        return;
    }
    if (strcmp(results, "found") == 0) {
        FILE *fptr;
        fptr = fopen(filename,"wb");
        char file_buffer[CHUNK];
        int filesize;
        int received_bytes;
        int total_bytes = 0;
        recv(clientfd, &filesize, sizeof(filesize), 0);
        int file_size = ntohl(filesize);
        while (1){
            bzero(file_buffer, CHUNK);
            int remaining_bytes = file_size - total_bytes;
            if (remaining_bytes <= CHUNK){
                received_bytes = recv(clientfd, file_buffer, remaining_bytes, 0);
                total_bytes = total_bytes + received_bytes;
                fwrite(&file_buffer, sizeof(char), received_bytes, fptr);
                break;
            }
            received_bytes = recv(clientfd, file_buffer, CHUNK, 0);
            total_bytes = total_bytes + received_bytes;
            fwrite(&file_buffer, sizeof(char), received_bytes, fptr);
        }
        fclose(fptr);
        printf("%d bytes downloaded successfully.\n", (int)file_size);
    }
}


void cmd_delete (char* line, char* filename, int clientfd, FILE* fp) {
    char results[MINI];
    send(clientfd, line, strlen(line), 0);
    if (recv(clientfd, results, MINI, 0) < 0){
        printf("File [%s] could not be found in remote directory.\n", filename);
        return;
    }
    if (strcmp(results, "not del") == 0){
        printf("File [%s] could not be found in remote directory.\n", filename);
        return;
    }
    if (strcmp(results, "locked") == 0){
        printf("File [%s] is currently locked by another user.\n", filename);
        return;
    }
    if (strcmp(results, "deleted") == 0) {
        printf("The file is deleted successfully.\n");
    }
}


void cmd_syncheck (char* line, char* filename, int clientfd, FILE* fp) {
    printf("Sync Check Report:\n");
    FILE *fptr;
    fptr = fopen(filename,"rb");
    int exists = 0;
    if (fptr != NULL) {
        exists = 1;
        printf("- Local Directory:\n");
        fseek(fptr, 0L, SEEK_END);
        int file_size = ftell(fptr);
        fseek(fptr, 0L, SEEK_SET);
        printf("-- File Size: %d bytes.\n", file_size);
        fclose(fptr);
    }
    char results[MINI];
    send(clientfd, line, strlen(line), 0);
    if (recv(clientfd, results, MINI, 0) < 0){
        return;
    }
    if (strcmp(results, "not found") == 0){
        return;
    }
    if (strcmp(results, "found") == 0) {
        printf("- Remote Directory:\n");
        int filesize;
        recv(clientfd, &filesize, sizeof(filesize), 0);
        int file_size = ntohl(filesize);
        printf("-- File Size: %d bytes.\n", (int)file_size);
        if (exists == 1) {
            unsigned char md5[MD5_DIGEST_LENGTH];
            MDFile(filename, md5);
            unsigned char servermd5[MD5_DIGEST_LENGTH];
            recv(clientfd, servermd5, sizeof(servermd5), 0);
            int x;
            int sync = 1;
            for (x = 0; x < MD5_DIGEST_LENGTH; x++) {
                if (md5[x] != servermd5[x]) {
                    sync = 0;
                }
            }
            printf("-- Sync Status: %s.\n", sync ? "synced" : "unsynced");
        } else {
            unsigned char servermd5[MD5_DIGEST_LENGTH];
            recv(clientfd, servermd5, sizeof(servermd5), 0);
            printf("-- Sync Status: unsynced.\n");
        }
        char checklock[MINI];
        recv(clientfd, checklock, MINI, 0);
        if (strcmp(checklock, "locked") == 0){
            printf("-- Lock Status: locked.\n");
        } else if (strcmp(checklock, "unlocked") == 0) {
            printf("-- Lock Status: unlocked.\n");
        }
    }
}


void cmd_process (char *cmdline, int clientfd, FILE* fp){
    char *cmd[MAXLINE];
    char tmp_cmd[MAXLINE];
    strcpy(tmp_cmd, cmdline);
    parseline(tmp_cmd, cmd);
    if (strcmp(cmd[0], "quit") == 0) {
        exit(0);
    } else if (strcmp(cmd[0], "pause") == 0) {
        int pTime = atoi(cmd[1]);
        cmd_pause(cmdline, pTime, clientfd, fp);
    } else if (strcmp(cmd[0], "append") == 0) {
        cmd_append(cmdline, cmd[1], clientfd, fp);
    } else if (strcmp(cmd[0], "upload") == 0) {
        cmd_upload(cmdline,cmd[1], clientfd, fp);
    } else if (strcmp(cmd[0], "download") == 0){
        cmd_download(cmdline,cmd[1], clientfd, fp);
    } else if (strcmp(cmd[0], "delete") == 0) {
        cmd_delete(cmdline,cmd[1], clientfd, fp);
    } else if (strcmp(cmd[0], "syncheck") == 0) {
        cmd_syncheck(cmdline,cmd[1], clientfd, fp);
    } else {
        printf("Command [%s] is not recognized.\n", cmd[0]);
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


int main (int argc, char **argv) {
    int clientfd;
    const char* filename = argv[1];
    char* host = argv[2];
    struct sockaddr_in serv_addr;
    clientfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (clientfd < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(client_port);
    int addr_status = inet_pton(AF_INET, host, &serv_addr.sin_addr);
    if (addr_status <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
    int connect_status = connect(clientfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (connect_status < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    FILE *fp = fopen(filename,"r");
    if (fp == NULL) {
        fprintf(stderr, "Fail to open the command file.");
        exit(1);
    }
    printf("Welcome to ICS53 Online Cloud Storage.\n");
    chdir("./Local Directory");
    char cmdline[MAXLINE];
    while (printf("> "), bzero(cmdline, MAXLINE), fgets(cmdline, MAXLINE, fp)) {
        int len = strlen(cmdline);
        if (cmdline[len-1] == '\n')
            printf("%s", cmdline);
        else
            printf("%s\n", cmdline);
        cmd_process(cmdline, clientfd, fp);
    }
    close(clientfd);
    exit(0);
}

