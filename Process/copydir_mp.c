// A program coping a directory and its subdirectories by multiprocess
// Compile: gcc -o copydir_mp copydir_mp.c
// Use: ./copydir_mp <source directory> <target directory>
// Author: Noah Lin
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>

void copy_file(const char* source_file, const char* target_file)
{
    int source_fd = open(source_file, O_RDONLY);
    if(source_fd == -1) { // ERROR
        printf("Can not open a source file!\n");
        close(source_fd);
        exit(-1);
    }
    int target_fd = open(target_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(target_fd == -1) { //ERROR
        printf("Can not open a target file!\n");
        close(target_fd);
        exit(-1);
    }
    char buffer[4096];// Buffer of copying(copy 4096 bytes everytime)
    ssize_t read_bytes;
    // Keep reading and copying the source file until reaching the end
    while((read_bytes = read(source_fd, buffer, sizeof(buffer))) > 0) { // read_bytes is the number of bytes actually read
        ssize_t write_bytes = write(target_fd, buffer, read_bytes);
        if(write_bytes == -1) { // ERROR
            printf("An error occurs when writing a target file!\n");
            printf("write");// ERROR information
            close(source_fd);
            close(target_fd);
            exit(-1);
        }
    }
    if(read_bytes == -1) { // ERROR
        printf("An error occurs when reading a source file!\n");
        perror("read");// ERROR information
        close(source_fd);
        close(target_fd);
        exit(-1);
    }
    close(source_fd);
    close(target_fd);
}

void copy_dir(const char* source_dir, const char* target_dir)
{
    DIR* dir;// Directory stream
    struct dirent* ptr;
    dir = opendir(source_dir);// Open directory
    if(!dir) { // ERROR
        printf("Cannot open a directory\n");
        perror("opendir");
        exit(-1);
    }
    struct stat stat_buf;
    if(stat(target_dir, &stat_buf) == -1) { // If target directory has not existed
        if(mkdir(target_dir, 0755) == -1) { // Create target directory
            printf("Can not make a directory!\n");
            perror("mkdir");// ERROR information
            exit(-1);
        }
    }
    pid_t pid;// Use for multiprocess
    // Read directory
    while((ptr = readdir(dir)) != NULL) {
        char* current_dir = ptr->d_name;// Current directory entry
        if(strcmp(current_dir, ".") == 0 || strcmp(current_dir, "..") == 0) {
            continue;// Skip . and ..
        }
        char target_path[1024];
        snprintf(target_path, sizeof(target_path), "%s/%s", target_dir, current_dir);// Target path
        char source_path[1024];
        snprintf(source_path, sizeof(source_path), "%s/%s", source_dir, current_dir);// Source path
        if(ptr->d_type == DT_DIR) { // If type of current directory entry is a directory
            pid = fork();// Fork a child process and use it to copy the subdirectory
            if(pid == 0) { // Child process
                copy_dir(source_path, target_path);// Recursively copy subdirectories
                exit(0);// Exit child processs after copying is end
            }
            else if(pid < 0) { // ERROR
                printf("Cannot fork a process\n");
                perror("fork");
                exit(-1);
            }
        }
        else if(ptr->d_type == DT_LNK) { // If type of current directory entry is a symbol link file
            if(stat(target_path, &stat_buf) == -1) {// If target link file has not existed
                if(link(source_path, target_path) == -1) { // Create a link file
                    printf("Can not create a link file!\n");
                    perror("link");
                    exit(-1);
                }
            }
        }
        else {
            if(stat(target_path, &stat_buf) == -1) { // If target file has not existed
                copy_file(source_path, target_path);// Copy file
            }
        }
    }
    // Wait for child processes
    while(1) {
        pid = wait(NULL);
        if(pid == -1) { // All child processes have been done
            break;
        }
    }
    closedir(dir);
}

int main(int argc, char* argv[])
{
    if(argc != 3) {
        printf("Use copydir_mp by: ./copydir_mp <source directory> <target directory>\n");
        return -1;// ERROR
    }
    char* source_dir = argv[1];
    char* target_dir = argv[2];
    copy_dir(source_dir, target_dir);
    return 0;
}