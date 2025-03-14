/*
TODO:
    add code to make the server send text/javascript
    add code to send text/css to the browser
    add code to correctly shutdown the webserver
    add a condition to give one extra byte for html files
*/
#include <WinSock2.h>
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#define REQUEST_BUFFER_SIZE 457
#define DATE_SIZE 46
#define HASHMAP_SIZE 100

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

DWORD WINAPI manage_request(LPVOID manage_request_helper_received);
DWORD WINAPI server_command_line(LPVOID null);
void cleanup(webserver *server_struct, char *buffer[], FILE* file, HANDLE* handle, int wsacleanup);
unsigned int hash(void* key, int len);

int main() {
    #pragma comment(lib, "Ws2_32.lib")
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
    FILE* webpage_element = NULL;
    int webpage_element_size = 0;
    char *webpage_element_loaded = NULL;
    char *path = NULL;
    int total_size;
    int path_size;
    file_struct* file = NULL;
    int hash_element;
    int size_file_struct = sizeof(file_struct);
    // this pointer navigates through the hash map to put the file in the correct position
    file_struct* nav = NULL;
    WIN32_FIND_DATAA sub_directory_data;
    HANDLE sub_directory_handle;
    //a path for a file inside the directory;
    char *sub_directory_file_path = NULL;
    char *sub_directory_path = NULL;
    int len_sub_directory_name;
    //coverts the windows system file path to webpagepath instead of allocating more memory
    char *web_path_converter = NULL;
    //the file extension (.exe for example) of the file
    char *file_extension = NULL;
    while (FindNextFile(hFind, &data)) {
        if (*data.cFileName == '.') {
            continue;
        }
        if (data.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) {
            len_sub_directory_name = strlen(data.cFileName);
            sub_directory_path = malloc(5 + len_sub_directory_name);
            if (!sub_directory_path) {
                char *cleanup_array[] = { banned, http_header, NULL };
                cleanup(&server_struct, cleanup_array, NULL, hFind, 1);
                return 1;
            }
            strcpy(sub_directory_path, ".\\");
            strcpy(sub_directory_path + 2, data.cFileName);
            strcpy(sub_directory_path + len_sub_directory_name + 2, "\\*");
            sub_directory_path[len_sub_directory_name + 4] = '\0';
            sub_directory_handle = FindFirstFile(sub_directory_path, &sub_directory_data);
            if (sub_directory_handle == INVALID_HANDLE_VALUE || sub_directory_handle == ERROR_FILE_NOT_FOUND) {
                char *cleanup_array[] = { banned, http_header,sub_directory_path, NULL };
                cleanup(&server_struct, cleanup_array, hFind, NULL, 1);
                return 1;
            }
            free(sub_directory_path);
            while (FindNextFile(sub_directory_handle, &sub_directory_data)) {
                if (*sub_directory_data.cFileName == '.'){
                    continue;
                }
                if (strstr(banned, sub_directory_data.cFileName)) {
                    continue;
                }
                sub_directory_file_path = malloc(strlen(sub_directory_data.cFileName) + len_sub_directory_name + 1);
                sprintf(sub_directory_file_path, "%s\\%s", data.cFileName ,sub_directory_data.cFileName);
                webpage_element = fopen(sub_directory_file_path, "r");
                if (!webpage_element) {
                    char *cleanup_array[] = { banned, http_header,sub_directory_file_path, NULL };
                    cleanup(&server_struct, cleanup_array, hFind, NULL, 1);
                    FindClose(sub_directory_handle);
                    return 1;
                }
                if (!webpage_element) {
                    char *cleanup_array[] = { banned, http_header, NULL };
                    cleanup(&server_struct, cleanup_array, NULL, hFind, 1);
                    return 1;
                } 
                while (fgetc(webpage_element) != EOF)
                    webpage_element_size++;
                if (!webpage_element_size) {
                    fclose(webpage_element);
                    continue;
                }
                rewind(webpage_element);
                http_header_size += (int) floor(log10(webpage_element_size));
                total_size = webpage_element_size + http_header_size;
                webpage_element_loaded = malloc(total_size);
                if (!webpage_element_loaded) {
                    char *cleanup_array[] = { banned, http_header, NULL };
                    cleanup(&server_struct, cleanup_array, webpage_element, hFind, 1);
                    return 1;
                }
                webpage_element_loaded[total_size - 1] = '\0';
                if (sprintf(webpage_element_loaded, http_header, webpage_element_size, "%s") < 0) {
                    puts("couldn't format page size");
                    char *cleanup_array[] = { banned, http_header, webpage_element_loaded, NULL };
                    cleanup(&server_struct, cleanup_array, webpage_element, hFind, 1);
                    return 1;
                }
                if (!fread(&webpage_element_loaded[http_header_size - 1], webpage_element_size, 1, webpage_element)) {
                    char *cleanup_array[] = { banned, http_header, webpage_element_loaded, NULL };
                    cleanup(&server_struct, cleanup_array, webpage_element, hFind, 1);
                    return 1;
                }
                fclose(webpage_element);
                http_header_size -= (int)floor(log10(webpage_element_size));
                while ((web_path_converter = strchr(sub_directory_file_path,'\\')))
                    *web_path_converter = '/';
                memmove(sub_directory_file_path + 1, sub_directory_file_path, strlen(sub_directory_file_path));
                sub_directory_file_path[0] = '/';
                if (!strcmp(sub_directory_data.cFileName, "index.html")){
                    for (int i = 0; sub_directory_file_path[i] != '\0'; i++){
                        if (sub_directory_file_path[i] == '/'){
                            web_path_converter = &sub_directory_file_path[i];
                        }
                    }
                    *web_path_converter = '\0';
                }
                else{
                    for (int i = 0; sub_directory_file_path[i] != '\0'; i++){
                        if (sub_directory_file_path[i] == '.'){
                            file_extension = &(sub_directory_file_path[i]);
                        }
                    }
                    if (!strcmp(file_extension, ".html"))
                        *file_extension = '\0'; 
                }
                hash_element = hash(sub_directory_file_path, strlen(sub_directory_file_path));
                file = malloc(size_file_struct);
                if (!file) {
                    char *cleanup_array[] = { banned, http_header, webpage_element_loaded, path, NULL };
                    cleanup(&server_struct, cleanup_array, webpage_element, hFind, 1);
                    return 1;
                }
                file->file_content = webpage_element_loaded;
                file->file_size = webpage_element_size + http_header_size + DATE_SIZE;
                file->path = sub_directory_file_path;
                file->next = NULL;
                nav = server_struct.pages[hash_element];
                if (nav) {
                    while (nav){
                        if (nav->next)
                            nav = nav->next;
                        else
                            break;
                    }
                    nav->next = file;
                }
                else
                    server_struct.pages[hash_element] = file;
                webpage_element_size = 0;
            }
            int closed = 0;
            while(!closed)
                closed = FindClose(sub_directory_handle);
        }
        else {
            if (strstr(banned, data.cFileName)) {
                continue;
            }
            webpage_element = fopen(data.cFileName, "r");
            if (!webpage_element) {
                char *cleanup_array[] = { banned, http_header, NULL };
                cleanup(&server_struct, cleanup_array, NULL, hFind, 1);
                return 1;
            } 
            while (fgetc(webpage_element) != EOF)
                webpage_element_size++;
            if (!webpage_element_size) {
                fclose(webpage_element);
                continue;
            }
            rewind(webpage_element);
            http_header_size += (int) floor(log10(webpage_element_size));
            total_size = webpage_element_size + http_header_size;
            webpage_element_loaded = malloc(total_size);
            if (!webpage_element_loaded) {
                char *cleanup_array[] = { banned, http_header, NULL };
                cleanup(&server_struct, cleanup_array, webpage_element, hFind, 1);
                return 1;
            }
            webpage_element_loaded[total_size - 1] = '\0';
            if (sprintf(webpage_element_loaded, http_header, webpage_element_size, "%s") < 0) {
                puts("couldn't format page size");
                char *cleanup_array[] = { banned, http_header, webpage_element_loaded, NULL };
                cleanup(&server_struct, cleanup_array, webpage_element, hFind, 1);
                return 1;
            }
            if (!fread(&webpage_element_loaded[http_header_size - 1], webpage_element_size, 1, webpage_element)) {
                char *cleanup_array[] = { banned, http_header, webpage_element_loaded, NULL };
                cleanup(&server_struct, cleanup_array, webpage_element, hFind, 1);
                return 1;
            }
            http_header_size -= (int)floor(log10(webpage_element_size));
            if (!strcmp(data.cFileName, "index.html")) {
                path = malloc(2);
                if (!path) {
                    char *cleanup_array[] = { banned, http_header, webpage_element_loaded, NULL };
                    cleanup(&server_struct, cleanup_array, webpage_element, hFind, 1);
                    return 1;
                }
                path[0] = '/';
                path[1] = '\0';
            }
            else if (!strcmp(data.cFileName, "404.html")) {
                file = malloc(size_file_struct);
                if (!file){
                    char *cleanup_array[] = { banned, http_header, webpage_element_loaded, NULL };
                    cleanup(&server_struct, cleanup_array, webpage_element, hFind, 1);
                    return 1;
                }
                webpage_element_loaded = realloc(webpage_element_loaded, webpage_element_size + http_header_size + 7);
                if (!webpage_element_loaded){
                    free(file);
                    char *cleanup_array[] = {banned, http_header, webpage_element_loaded, NULL };
                    cleanup(&server_struct, cleanup_array, webpage_element, hFind, 1);
                    return 1;
                }
                char *code  = strstr(webpage_element_loaded, "200");
                memmove(code + 7, code, webpage_element_size + http_header_size);
                strcpy(code, "404 Not Found");
                code[13] = '\n';
                file -> file_content = webpage_element_loaded;
                file -> file_size = webpage_element_size + http_header_size + DATE_SIZE + 7;
                file -> next = NULL;
                file -> path = NULL;
                server_struct.error_404 = file;
                fclose(webpage_element);
                webpage_element_size = 0;
                continue;
            }
            else {
                path = malloc(strlen(data.cFileName) + 4);
                if (!path) {
                    char *cleanup_array[] = { banned, http_header, webpage_element_loaded, NULL };
                    cleanup(&server_struct, cleanup_array, webpage_element, hFind, 1);
                    return 1;
                }
                path[0] = '/';
                strcpy(path + 1, data.cFileName);
            }
            hash_element = hash(path, strlen(path));
            file = malloc(size_file_struct);
            if (!file) {
                char *cleanup_array[] = { banned, http_header, webpage_element_loaded, path, NULL };
                cleanup(&server_struct, cleanup_array, webpage_element, hFind, 1);
                return 1;
            }
            file->file_content = webpage_element_loaded;
            file->file_size = webpage_element_size + http_header_size + DATE_SIZE;
            file->path = path;
            file->next = NULL;
            nav = server_struct.pages[hash_element];
            if (nav) {
                while (nav){
                    if (nav->next)
                        nav = nav->next;
                    else
                        break;
                }
                nav->next = file;
            }
            else
                server_struct.pages[hash_element] = file;
            fclose(webpage_element);
            webpage_element_size = 0;
        }
    }
    int closed = 0;
    while (!closed)
        closed = FindClose(hFind);
    free(http_header);
    free(banned);
    HANDLE command_line = CreateThread(NULL, 0, server_command_line, NULL, 0, NULL);
    int timeout = 2;
    fd_set set;
    FD_SET(server, &set);
    TIMEVAL tv = {1, 0 };
    select(0, &set, NULL, NULL, &tv);
    setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    while (WaitForSingleObject(command_line ,0) != WAIT_OBJECT_0){
        HANDLE manage_request_thread = CreateThread(NULL, 0, manage_request, &server_struct, 0, NULL);
        Sleep(100);
    }
    cleanup(&server_struct, NULL, NULL, NULL, 1);
    return 0;
}

DWORD WINAPI manage_request(LPVOID manage_request_helper_received) {
    webserver* manage_request_helper_pointer = manage_request_helper_received;
    SOCKET client = accept(*(manage_request_helper_pointer->server), NULL, NULL);
    if (client == INVALID_SOCKET) {
        ExitThread(0);
    }
    char request_buffer[REQUEST_BUFFER_SIZE];
    request_buffer[0] = '\0';
    //null terminating request buffer and preventing it's content isn't already GET / by adding an extra NULL character
    if (recv(client, request_buffer, REQUEST_BUFFER_SIZE - 1, 0) <= 0) {
        puts("couldn't receive request or no request was received");
        goto close_socket_client;
    }
    request_buffer[REQUEST_BUFFER_SIZE - 1] = '\0';
    char current_time[DATE_SIZE];
    current_time[DATE_SIZE - 1] = '\0';
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    strftime(current_time, sizeof(current_time), "%a, %d %b %Y %H:%M:%S %Z", &tm);
    char *page_requested_path;
    if (!(page_requested_path = strchr(request_buffer, '\n'))) {
        goto close_socket_client;
    }
    *(page_requested_path - 10) = '\0';
    page_requested_path = request_buffer + 4;
    int hash_page_path = hash(page_requested_path, strlen(page_requested_path));
    file_struct *page_requested = manage_request_helper_pointer->pages[hash_page_path];
    if (page_requested) {
        while (strcmp(page_requested_path, page_requested -> path)) {
            if (page_requested->next) {
                page_requested = page_requested->next;
            }
            else {
                send(client, manage_request_helper_pointer -> error_404 -> file_content, manage_request_helper_pointer -> error_404 -> file_size, 0);
                goto close_socket_client;
            }
        }
    }
    else {
        send(client, manage_request_helper_pointer -> error_404 -> file_content, manage_request_helper_pointer -> error_404 -> file_size, 0);
        goto close_socket_client;
    }
    int page_size = page_requested->file_size;
    char *page_date = malloc(page_size);
    if (!page_date) {
        puts("couldn't allocate memory for page");
        goto close_socket_client;
    }
    page_date[page_size - 1] = '\0';
    if (sprintf(page_date, page_requested->file_content, current_time) < 0) {
        puts("couldn't format date");
        goto cleanup_label;
    }
    if (send(client, page_date, page_size, 0) == SOCKET_ERROR) {
        printf("couldn't send data error code: %i", WSAGetLastError());
        goto cleanup_label;
    }
    cleanup_label:
        free(page_date);
    close_socket_client:
        Sleep(100);
        closesocket(client);
        ExitThread(0);
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