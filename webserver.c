/*
TODO:
    add code to format dates in webpage
*/
#include <WinSock2.h>
#include <Windows.h>
#include <Shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#define REQUEST_BUFFER_SIZE 457
#define DATE_SIZE 46
#define HASHMAP_SIZE 400

typedef struct file_struct {
    int file_size;
    char *path;
    char *file_content;
    struct file_struct* next;
}file_struct;
// this structure is passed into the manage request thread

typedef struct webserver {
    SOCKET* server;
    file_struct* pages[HASHMAP_SIZE];
    file_struct *error_404;
}webserver;

DWORD WINAPI manage_request(LPVOID server_struct);
DWORD WINAPI server_command_line(LPVOID null);
int readfile(char *file_path, int http_header_size, webserver *server_struct, char *banned, char *http_header);
int get_file_size(FILE *file, int file_size);
void send_full(SOCKET client, char *buffer, int size);
void cleanup(webserver *server_struct, char *buffer[], FILE* file, HANDLE* handle, int wsacleanup);
unsigned int hash(void* key, int len);

int main() {
    #pragma comment(lib, "Ws2_32.lib")
    #pragma comment(lib, "Shlwapi.lib")
    WSADATA wsadata;
    //initiating the use of the Winsock DLL by a server
    if (WSAStartup(MAKEWORD(2, 2), &wsadata)) {
        printf("couldn't initiate the use of the Winsock DLL by a server error code: %i", WSAGetLastError());
        return 1;
    }
    //creating server socket
    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        printf("couldn't start server socket error code : %i", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    webserver server_struct = { NULL };
    server_struct.server = &server;
    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    addr.sin_addr.s_addr = INADDR_ANY;
    //binding server socket
    if (bind(server, (struct sockaddr*)&addr, sizeof(addr))) {
        printf("couldn't bind server socket error code : %i", WSAGetLastError());
        cleanup(&server_struct, NULL, NULL, NULL, 1);
        return 1;
    }
    //listening for requests
    if (listen(server, SOMAXCONN)) {
        printf("couldn't start listening for requests error code : %i", WSAGetLastError());
        cleanup(&server_struct, NULL, NULL, NULL, 1);
        return 1;
    }
    FILE* header = fopen("header.txt", "r");
    if (!header) {
        puts("couldn't open file that contains http header");
        cleanup(&server_struct, NULL, NULL, NULL, 1);
        return 1;
    }
    int http_header_size = 0;
    while (fgetc(header) != EOF)
        http_header_size++;
    rewind(header);
    char *http_header = malloc(http_header_size);
    if (!http_header) {
        puts("couldn't allocate memory for the http header");
        cleanup(&server_struct, NULL, header, NULL, 1);
        return 1;
    }
    // add support for custom http headers
    http_header[http_header_size - 1] = '\0';
    if (!fread(http_header, http_header_size, 1, header)) {
        puts("couldn't read from header");
        char *cleanup_array[] = { http_header , NULL };
        cleanup(&server_struct, cleanup_array, header, NULL, 1);
        return 1;
    }
    fclose(header);
    FILE* banned_file = fopen("banned.txt", "r");
    //the files that can't be sent to the client
    if (!banned_file) {
        puts("couldn't open the list of banned names");
        char *cleanup_array[] = { http_header, NULL };
        cleanup(&server_struct, cleanup_array, NULL, NULL, 1);
        return 1;
    }
    int banned_size = 0;
    while (fgetc(banned_file) != EOF)
        banned_size++;
    rewind(banned_file);
    char *banned = malloc(banned_size);
    if (!banned) {
        puts("couldn't allocate memory for banned list");
        char *cleanup_array[] = { http_header , NULL };
        cleanup(&server_struct, cleanup_array, banned_file, NULL, 1);
        return 1;
    }
    banned[banned_size - 1] = '\0';
    if (!fread(banned, banned_size, 1, banned_file)) {
        puts("couldn't read banned.txt");
        char *cleanup_array[] = { banned, http_header, NULL };
        cleanup(&server_struct, cleanup_array, banned_file, NULL, 1);
        return 1;
    }
    fclose(banned_file);
    WIN32_FIND_DATAA data;
    HANDLE hFind = FindFirstFile(".\\*", &data);
    if (hFind == INVALID_HANDLE_VALUE || hFind == ERROR_FILE_NOT_FOUND) {
        char *cleanup_array[] = { banned, http_header, NULL };
        cleanup(&server_struct, cleanup_array, NULL, NULL, 1);
        return 1;
    }
    while (FindNextFile(hFind, &data)) {
        if (*data.cFileName == '.') {
            continue;
        }
        if (readfile(data.cFileName, http_header_size, &server_struct, banned, http_header)){
            printf("file: %s couldn't be read", data.cFileName);
            char *cleanup_array[] = {banned, http_header, NULL};
            cleanup(&server_struct, cleanup_array, NULL, hFind, 1);
            return 1;
        }
    }
    int closed = 0;
    while (!closed)
        closed = FindClose(hFind);
    free(http_header);
    free(banned);
    HANDLE command_line = CreateThread(NULL, 0, server_command_line, NULL, 0, NULL);
    int timeout = 5;
    fd_set set;
    FD_SET(server, &set);
    TIMEVAL tv = {2, 0 };
    select(0, &set, NULL, NULL, &tv);
    setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    while (WaitForSingleObject(command_line ,0) != WAIT_OBJECT_0){
        HANDLE manage_request_thread = CreateThread(NULL, 0, manage_request, &server_struct, 0, NULL);
        Sleep(1);
    }
    cleanup(&server_struct, NULL, NULL, NULL, 1);
    return 0;
}
int readfile(char *file_path, int http_header_size, webserver *server_struct, char *banned, char *http_header){
    if (strstr(banned, file_path))
        return 0;
    if (PathIsDirectory(file_path)){
        int path_len = strlen(file_path);
        char *sub_directory_path = malloc(path_len + 3);
        if (!sub_directory_path)
            return 1;
        strcpy(sub_directory_path, file_path);
        strcpy(sub_directory_path + path_len, "\\*");
        WIN32_FIND_DATAA sub_directory_data;
        HANDLE sub_directory_handle = FindFirstFile(sub_directory_path, &sub_directory_data);
        if (sub_directory_handle == INVALID_HANDLE_VALUE || sub_directory_handle == ERROR_FILE_NOT_FOUND) {
            printf("directroy: %s may have been deleted", file_path);
            return 0;
        }
        while (FindNextFile(sub_directory_handle, &sub_directory_data)) {
            if (*sub_directory_data.cFileName == '.') {
                continue;
            }
            char *sub_directory_file_path = malloc(path_len + strlen(sub_directory_data.cFileName) + 1);
            if (!sub_directory_file_path){
                char *cleanup_array[] = {sub_directory_path , NULL};
                cleanup(NULL, cleanup_array, NULL, sub_directory_handle, 0);
                return 1;
            }
            strcpy(sub_directory_file_path, sub_directory_path);
            strcpy(sub_directory_file_path + path_len + 1, sub_directory_data.cFileName);
            if (readfile(sub_directory_file_path, http_header_size, server_struct, banned, http_header)){
                char *cleanup_array[] = {sub_directory_path, sub_directory_file_path, NULL};
                cleanup(NULL, cleanup_array, NULL, sub_directory_handle, 0);
                printf("%s couldn't be oppened\n", sub_directory_file_path);
                return 1;
            }
            free(sub_directory_file_path);
        }
        free(sub_directory_path);
        return 0;
    }
    char *file_extension = "";
    char *file_name = file_path;
    char *extension_name_test;
    if ((extension_name_test = strrchr(file_path, '\\'))){
        file_name = extension_name_test + 1;
    }
    if ((extension_name_test = strrchr(file_path, '.'))){
        file_extension = extension_name_test;
    }
    int path_len = strlen(file_path);
    FILE *file;
    if (!strcmp(file_extension, ".html") || !strcmp(file_extension, ".css") || !strcmp(file_extension, ".js")){
        FILE *file = fopen(file_path, "r");
        if (!file){
            return 1;
        }
        int file_size = get_file_size(file, 0);
        http_header_size += (int) floor(log10(file_size));
        if (!strcmp(file_extension,".js"))
            http_header_size += 13;
        else if(!strcmp(file_extension,".css"))
            http_header_size += 6;
        else
            http_header_size += 7;
        int total_size = file_size + http_header_size;
        char *file_loaded = malloc(total_size);
        if (!file_loaded){
            fclose(file);
            return 1;
        }
        if (!strcmp(file_extension, ".js")){
            if (sprintf(file_loaded, http_header, file_size, "text/javascript", "%s") == -1){
                char *cleanup_array[] = {file_loaded, NULL};
                cleanup(NULL, cleanup_array, file, NULL, 0);
                return 1;
            }
        }
        else if(!strcmp(file_extension, ".css")){
            if (sprintf(file_loaded, http_header, file_size, "text/css", "%s") == -1){
                char *cleanup_array[] = {file_loaded, NULL};
                cleanup(NULL, cleanup_array, file, NULL, 0);
                return 1;
            }
        }
        else {
            if (sprintf(file_loaded, http_header, file_size, "text/html", "%s") == -1){
                char *cleanup_array[] = {file_loaded, NULL};
                cleanup(NULL, cleanup_array, file, NULL, 0);
                return 1;
            }
        }
        if (!fread(file_loaded + http_header_size - 1, file_size, 1, file)){
            char *cleanup_array[] = {file_loaded, NULL};
            cleanup(NULL, cleanup_array, file, NULL, 0);
            return 1;
        }
        fclose(file);
        file_loaded[total_size - 1] = '\0';
        file_struct *file_node = malloc(sizeof(file_struct));
        if (!file_node){
            free(file_loaded);
            return 1;
        }
        char *web_path;
        if (!strcmp(file_path, "404.html")){
            char *code = strstr(file_loaded, "200 OK");
            file_loaded = realloc(file_loaded, total_size + 7);
            if (!file_loaded){
                free(file_loaded);
                free(file_node);
                return 1;
            }
            memmove(code + 7, code, file_size - 7);
            memcpy(code, "404 Not Found", 13);
            file_node -> file_content = file_loaded;
            file_node -> file_size = total_size + 7;
            file_node -> path = NULL;
            file_node -> next = NULL;
            server_struct -> error_404 = file_node;
            return 0;
        }
        if (!strcmp(file_name, "index.html")){
            int web_path_len = file_name - file_path;
            web_path = malloc(web_path_len + 2);
            memcpy(web_path + 1, file_path, web_path_len);
            web_path[web_path_len + 1] = '\0';
        }
        else {
            web_path = malloc(path_len);
            strcpy(web_path + 1, file_path);
        }
        web_path[0] = '/';
        char *web_path_converter;
        while ((web_path_converter = strchr(web_path, '\\'))){
            *web_path_converter = '/';
        }
        file_node -> file_content = file_loaded;
        file_node -> file_size = total_size;
        file_node -> path = web_path;
        file_node -> next = NULL;
        int hash_element = hash(web_path, strlen(web_path));
        file_struct *nav = server_struct -> pages[hash_element];
        if (nav) {
            while (nav){
                if (nav->next)
                    nav = nav->next;
            }
            nav->next = file_node;
        }
        else
            server_struct -> pages[hash_element] = file_node;
    }
    return 0;
}

int get_file_size(FILE *file, int file_size){
    while(fgetc(file) != EOF)
        file_size++;
    if (fgetc(file) != EOF){
        file_size = get_file_size(file, file_size);
    }
    rewind(file);
    return file_size;
}

DWORD WINAPI manage_request(LPVOID server_struct) {
    webserver* webserver_struct = server_struct;
    SOCKET client = accept(*(webserver_struct->server), NULL, NULL);
    if (client == INVALID_SOCKET) {
        ExitThread(0);
    }
    char request_buffer[REQUEST_BUFFER_SIZE];
    //null terminating request buffer and preventing it's content isn't already GET / by adding an extra NULL character
    if (recv(client, request_buffer, REQUEST_BUFFER_SIZE - 1, 0) == SOCKET_ERROR) {
        printf("couldn't receive request or no request was received error code: %i\n", WSAGetLastError());
        goto close_socket_client;
    }
    char *file_requested_path = request_buffer + 4;
    char *test_request = strchr(file_requested_path, ' ');
    if (test_request)
        *test_request = '\0';
    else
        ExitThread(0);
    file_struct *page_requested = webserver_struct -> pages[hash(file_requested_path, strlen(file_requested_path))];
    while (page_requested){
        if (strcmp(file_requested_path, page_requested -> path)){
            page_requested = page_requested -> next;
            continue;
        }
        send_full(client, page_requested -> file_content, page_requested -> file_size);
        goto close_socket_client;
    }
    send_full(client, webserver_struct -> error_404 -> file_content, webserver_struct -> error_404 -> file_size);
    goto close_socket_client;
    close_socket_client:
        Sleep(1000);
        closesocket(client);
        ExitThread(0);
}

void send_full(SOCKET client, char *buffer, int size) {
    while (size){
        size -= send(client, buffer, size, 0);
    }
}

DWORD WINAPI server_command_line(LPVOID null) {
    while (1) {
        if (getchar() == 'q')
            ExitThread(0);
    }
}

void cleanup(webserver* server_struct, char *buffer[], FILE* file, HANDLE* handle, int wsacleanup) {
    if (buffer) {
        for (int i = 0; buffer[i] != NULL; i++) {
            free(buffer[i]);
        }
    }
    if (wsacleanup)
        WSACleanup();
    if (file)
        fclose(file);
    if (handle) {
        int closed = 0;
        while (!closed)
            closed = FindClose(*handle);
    }
    if (server_struct) {
        if (server_struct -> server) {
            closesocket(*server_struct -> server);
        }
        file_struct* freer;
        file_struct* freer_copy;
        for (int i = 0; i > HASHMAP_SIZE; i++) {
            freer = server_struct->pages[i];
            while (freer != NULL) {
                free(freer -> file_content);
                free(freer -> path);
                freer_copy = freer;
                freer = freer->next;
                free(freer_copy);
            }
        }
        if (server_struct -> error_404){
            free(server_struct -> error_404 -> file_content);
            free(server_struct -> error_404);
        }
    }
}

unsigned int hash(void* key, int len)
{
    unsigned char *p = key;
    unsigned h = 0;
    int i;
    for (i = 0; i < len; i++) {
        h += p[i];
        h += (h << 10);
        h ^= (h >> 6);
    }
    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);
    h %= HASHMAP_SIZE;
    return h;
}