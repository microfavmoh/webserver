#pragma once
#include "includes.h"

//gets the size of the file
int get_file_size(FILE *file, int file_size){
    while(fgetc(file) != EOF)
        file_size++;
    if (fgetc(file) != EOF){
        file_size = get_file_size(file, file_size);
    }
    rewind(file);
    return file_size;
}

//reads a file into the hashmap if the file is a directory it lists it and reads the files in it
int readfile(char *file_path, int http_header_size, webserver *server_struct, char *http_header){
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
            free(sub_directory_path);
            return 0;
        }
        while (FindNextFile(sub_directory_handle, &sub_directory_data)) {
            if (*sub_directory_data.cFileName == '.') {
                continue;
            }
            char *sub_directory_file_path = malloc(path_len + strlen(sub_directory_data.cFileName) + 2);
            if (!sub_directory_file_path){
                char *cleanup_array[] = {sub_directory_path , NULL};
                cleanup(NULL, cleanup_array, NULL, sub_directory_handle, 0);
                return 1;
            }
            strcpy(sub_directory_file_path, sub_directory_path);
            strcpy(sub_directory_file_path + path_len + 1, sub_directory_data.cFileName);
            if (readfile(sub_directory_file_path, http_header_size, server_struct, http_header)){
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
    int is_js = !strcmp(file_extension, ".js");
    int is_css = !strcmp(file_extension, ".css");
    if (!strcmp(file_extension, ".html") || is_js || is_css){
        FILE *file = fopen(file_path, "r");
        if (!file){
            return 1;
        }
        int file_size = get_file_size(file, 0);
        http_header_size += (int) floor(log10(file_size));
        int is_404 = 0;
        if (is_js)
            http_header_size += 13;
        else if (is_css)
            http_header_size += 6;
        else
            http_header_size += 7;
            if ((is_404 = !strcmp(file_path, "assets\\404.html")))
                http_header_size += 7;
        int total_size = file_size + http_header_size;
        char *file_loaded = malloc(total_size);
        if (!file_loaded){
            fclose(file);
            return 1;
        }
        if (is_js){
            if (sprintf(file_loaded, http_header, file_size, "text/javascript", "%s") == -1){
                char *cleanup_array[] = {file_loaded, NULL};
                cleanup(NULL, cleanup_array, file, NULL, 0);
                return 1;
            }
        }
        else if(is_css){
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
        if (is_404){
            char *code = strstr(file_loaded, "200 OK");
            if (!code) {
                puts("invalid http header\n");
                free(file_node);
                free(file_loaded);
                return 1;
            }
            memmove(code + 7, code, http_header_size - 17);
            memcpy(code, "404 Not Found", 13);
            file_node -> file_content = file_loaded;
            file_node -> file_size = total_size + 7;
            server_struct -> error_404 = file_node;
            return 0;
        }
        if (!strcmp(file_name, "index.html")){
            int web_path_len = file_name - file_path;
            web_path = malloc(web_path_len - 5);
            if (!web_path){
                free(file_loaded);
                free(file_node);
                return 1;
            }
            memcpy(web_path + 1, file_path + 7, web_path_len - 7);
            web_path[web_path_len - 6] = '\0';
        }
        else {
            web_path = malloc(path_len - 6);
            if (!web_path){
                free(file_loaded);
                free(file_node);
                return 1;
            }
            strcpy(web_path + 1, file_path + 7);
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
            while (nav->next){
                nav = nav->next;
            }
            nav->next = file_node;
        }
        else
            server_struct -> pages[hash_element] = file_node;
    }
    return 0;
}