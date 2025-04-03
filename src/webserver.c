/*
TODO:
    add code to format dates in webpage
    add support for image and binary files
    add support for ipv6
    add https
    add support for custom http headers
    use windows api functions to get the file size
    add support for the assets folder and remove banned.txt file
*/
// Note: if upon running the program crashes please change the HASHMAP_SIZE constant in the lib/sc.h to a lower value
// Note: it isn't recommended to use the files in lib directory seperately instead include lib/inlcudes.h
#include "../lib/includes.h"
// function/thread that manages requests
DWORD WINAPI manage_request(LPVOID server_struct);
// the thread that is the command line of the server
DWORD WINAPI server_command_line(LPVOID null);

int main() {
    webserver server_struct = {0};
    WSADATA wsadata;
    //initiating the use of the Winsock DLL by a server
    if (WSAStartup(MAKEWORD(2, 2), &wsadata)) {
        printf("couldn't initiate the use of the Winsock DLL by a server error code: %i", WSAGetLastError());
        return 1;
    }
    //the ipv4 server socket server socket
    SOCKET server_ipv4 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_ipv4 == INVALID_SOCKET) {
        printf("couldn't start server socket error code : %i", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    server_struct.server_ipv4 = server_ipv4;
    struct sockaddr_in addr_ipv4 = {0};
    addr_ipv4.sin_family = AF_INET;
    addr_ipv4.sin_port = htons(80);
    addr_ipv4.sin_addr.s_addr = INADDR_ANY;
    //binding server socket
    if (bind(server_ipv4, (SOCKADDR *)&addr_ipv4, sizeof(addr_ipv4))) {
        printf("couldn't bind server socket error code : %i", WSAGetLastError());
        cleanup(&server_struct, NULL, NULL, NULL, 1);
        return 1;
    }
    //listening for requests
    if (listen(server_ipv4, SOMAXCONN)) {
        printf("couldn't start listening for requests error code : %i", WSAGetLastError());
        cleanup(&server_struct, NULL, NULL, NULL, 1);
        return 1;
    }
    FILE* header = fopen("required/header.txt", "r");
    //reading the http header
    if (!header) {
        puts("couldn't open file that contains http header");
        cleanup(&server_struct, NULL, NULL, NULL, 1);
        return 1;
    }
    int http_header_size = get_file_size(header, 0);
    char *http_header = malloc(http_header_size);
    if (!http_header) {
        puts("couldn't allocate memory for the http header");
        cleanup(&server_struct, NULL, header, NULL, 1);
        return 1;
    }
    http_header[http_header_size - 1] = '\0';
    if (!fread(http_header, http_header_size, 1, header)) {
        puts("couldn't read from header");
        char* cleanup_array[] = { http_header, NULL };
        cleanup(&server_struct, cleanup_array, header, NULL, 1);
        return 1;
    }
    fclose(header);
    if (readfile("assets", http_header_size, &server_struct, http_header)) {
        char* cleanup_array[] = { http_header, NULL };
        cleanup(&server_struct, cleanup_array, NULL, NULL, 1);
        return 1;
    }
    // reading the files in the current directory and sub-directories
    free(http_header);
    /*
        freeing both the http header and the list of banned files
        as they aren't required anymore
    */
    // the command line thread
    int timeout = 2;
    HANDLE request_event = WSACreateEvent();
    if (!request_event) {
        printf("couldn't create event error code:%i\n", WSAGetLastError());
        cleanup(&server_struct, NULL, NULL, NULL, 1);
        return 1;
    }
    if (WSAEventSelect(server_ipv4, request_event, FD_ACCEPT) == SOCKET_ERROR) {
        printf("couldn't listen  for request event error code:%i\n", WSAGetLastError());
        WSACloseEvent(request_event);
        cleanup(&server_struct, NULL, NULL, NULL, 1);
        return 1;
    }
    setsockopt(server_struct.server_ipv4, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    int thread_initial_stack_size = 5 * sizeof(int*) + 457 + 100;
    HANDLE command_line = CreateThread(NULL, 0, server_command_line, NULL, 0, NULL);
    // timeout on the recv function
    WSANETWORKEVENTS network_events;
    while (WaitForSingleObject(command_line ,0)){
        if (!WaitForSingleObject(request_event, 0)) {
            HANDLE manage_request_thread = CreateThread(NULL, thread_initial_stack_size, manage_request, &server_struct, 0, NULL);
            WSAEnumNetworkEvents(server_ipv4, request_event, &network_events);
        }
    }
    WSACloseEvent(request_event);
    cleanup(&server_struct, NULL, NULL, NULL, 1);
    return 0;
}

DWORD WINAPI manage_request(LPVOID server_struct) {
    webserver* webserver_struct = server_struct;
    SOCKET client = accept(webserver_struct->server_ipv4, NULL, NULL);
    if (client == INVALID_SOCKET) {
        ExitThread(0);
    }
    char request_buffer[REQUEST_BUFFER_SIZE];
    request_buffer[REQUEST_BUFFER_SIZE - 1] = 0;
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
    file_struct *page_requested = webserver_struct -> pages[hash(file_requested_path, test_request - file_requested_path)];
    while (page_requested){
        if (strcmp(file_requested_path, page_requested -> path)){
            page_requested = page_requested -> next;
            continue;
        }
        send(client, page_requested -> file_content, page_requested -> file_size, 0);
        goto close_socket_client;
    }
    send(client, webserver_struct -> error_404 -> file_content, webserver_struct -> error_404 -> file_size, 0);
    goto close_socket_client;
    close_socket_client:
        Sleep(1);
        closesocket(client);
        ExitThread(0);
}

DWORD WINAPI server_command_line(LPVOID null) {
    while (1) {
        if (getchar() == 'q')
            ExitThread(0);
    }
}