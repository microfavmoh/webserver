// the file name is shorthand for structs and cosntants
#pragma once
#include "includes.h"

#define REQUEST_BUFFER_SIZE 457
#define DATE_SIZE 46
#define HASHMAP_SIZE 400

/*
    this structure contains the following:
    1- the size of the file in bytes
    2- the webpath to the file
    3- the file cotent
    4- a pointer to the next file_struct in the linked list
*/
typedef struct file_struct {
    int file_size; // the size of the file in bytes
    char *path; // the webpath to the file
    char *file_content; // the file cotent
    struct file_struct *next; // a pointer to the next file_struct in the linked list
}file_struct;


/*
    this structure is passed into the manage_request thread it contains the following:
    1- a hashmap of the files read 
    2- a pointer to the server socket for ipv4 
    3 - a pointer to the server socket for ipv6
    3- the 404 page for quick access
*/
typedef struct webserver {
    SOCKET server_ipv4; // the server socket for ipv4 
    SOCKET server_ipv6; // the server socket for ipv6
    file_struct* pages[HASHMAP_SIZE]; // hashmap of the files read 
    file_struct *error_404; // the 404 page for quick access
}webserver;