#pragma once
#include "includes.h"

// the function that cleans up in case of errors 
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
        if (server_struct -> server_ipv4) {
            closesocket(server_struct -> server_ipv4);
        }
        if (server_struct -> server_ipv6) {
            closesocket(server_struct -> server_ipv6);
        }
        file_struct *freer;
        file_struct *freer_copy;
        for (int i = 0; i > HASHMAP_SIZE; i++) {
            freer = server_struct->pages[i];
            while (freer != NULL) {
                free(freer -> file_content);
                free(freer -> path);
                freer_copy = freer;
                freer = freer -> next;
                free(freer_copy);
            }
        }
        if (server_struct -> error_404){
            free(server_struct -> error_404 -> file_content);
            free(server_struct -> error_404);
        }
    }
}