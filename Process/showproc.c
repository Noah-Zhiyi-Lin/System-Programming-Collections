// A program showing information of processes like ps -ef
// Complie: gcc -o showproc showproc.c
// Use: ./showproc
// Author: Noah Lin 
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <pwd.h> // getpwuid()
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>

// Check whether the name of a directory is constructed by numbers, if true, return 1, else return 0
int is_process(const char* dir_name)
{
    int i;
    for(i = 0; i < 256 && dir_name[i]; i++) {
        if(isdigit(dir_name[i]) == 0) { // If dir_name[i] is not a number
            return 0;
        }
    }
    return 1;
}

// Open a file under /proc/pid, return 1 if succeed, otherwise return 0
int open_proc_file(const char* proc_dir, const char* file_name, FILE** file_pp)
{
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "/proc/%s/%s", proc_dir, file_name);
    *file_pp = fopen(file_path, "r");// Open file(pp means pointer of pointer)
    // Check file pointer
    if(*file_pp) {
        return 1;
    }
    else {
        return 0;
    }
}

// Return position of the end of a process name in order to tackle with possible space in the process name
int get_end_of_proc_name(const char* proc_dir)
{
    FILE* stat;
    if(open_proc_file(proc_dir, "stat", &stat) == 0) {
        printf("get_end_of_proc_name(): Cannot open stat file of process %s\n", proc_dir);
        exit(-1);
    }
    char line[1024];
    if(fgets(line, sizeof(line), stat) != NULL) {
        char* name_end = strchr(line, ')');
        fclose(stat);
        if(name_end) {
            return (int)(name_end - line);
        }
        else {
            return -1;// ERROR
        }
    }
    else {
        fclose(stat);
        printf("get_end_of_proc_name(): Cannot read stat file of process %s\n", proc_dir);
        exit(-1);
    }
}

// Get EUID of a process
int get_euid(const char* proc_dir)
{
    FILE* status;// status file
    if(open_proc_file(proc_dir, "status", &status) == 0) { // ERROR
        printf("get_euid(): Cannot open status file of process %s\n", proc_dir);
        exit(-1);
    }
    char line[512];
    // Read lines of the status file in order to find Effective UID
    int EUID;// Effective UID
    while(fgets(line, sizeof(line), status)) {
        if(strncmp(line, "Uid:", 4) == 0) {
            sscanf(line, "Uid:\t%*d\t%d\t%*d\t%*d", &EUID);
            break;
        }
    }
    fclose(status);
    return EUID;
}

// Get username(maximum 7 chars) of a process by the EUID of the process
void get_uname(const char* proc_dir, char* uname)
{
    int EUID = get_euid(proc_dir);
    struct passwd* pw = getpwuid(EUID);
    if(!pw) { //ERROR
        printf("get_uname(): Cannot get username of process %s\n", proc_dir);
        exit(-1);
    }
    int i;
    int j = 0;
    for(i = 0; i < strlen(pw->pw_name); i++) {
        if(j < 7) {
            uname[j] = pw->pw_name[i];
            j++;
        }
        else if(j == 7) { // Truncate the username
            uname[j] = '+';
            j++;
            break;
        }
    }
    uname[j] = '\0';
}

// Get pid and ppid of a process
void get_pid_and_ppid(const char* proc_dir, int* pid, int* ppid)
{
    FILE* stat;// stat file
    if(open_proc_file(proc_dir, "stat", &stat) == 0) { // ERROR
        printf("get_pid_and_ppid(): Cannot open stat file of process %s\n", proc_dir);
        exit(-1);
    }
    // Read stat file to get pid and ppid
    char line[1024];
    if(fgets(line, sizeof(line), stat) != NULL) {
        sscanf(line, "%d", pid);
        int name_end_ind = get_end_of_proc_name(proc_dir);
        if(name_end_ind == -1) {
            printf("get_pid_and_ppid(): Name error of process %s\n", proc_dir);
            fclose(stat);
            exit(-1);
        }
        sscanf(line + name_end_ind + 4, "%d", ppid);
    }
    else { //ERROR
        printf("get_pid_and_ppid(): Cannot read stat file of process %s\n", proc_dir);
        fclose(stat);
        exit(-1);
    }
    fclose(stat);
}

// Get process start time(measured in jiffies)
unsigned long get_start_jiffies(const char* proc_dir)
{
    FILE* stat;// stat file
    if(open_proc_file(proc_dir, "stat", &stat) == 0) { // ERROR
        printf("Cannot open stat file of process: %s\n", proc_dir);
        exit(-1);
    }
    // Read the stat file to get start time(jiffies) of the process
    unsigned long start_jiffies;
    // Move file pointer to ppid
    int name_end_ind = get_end_of_proc_name(proc_dir);
    if(name_end_ind == -1) { // ERROR
        printf("get_start_jiffies(): Name error of process %s\n", proc_dir);
        fclose(stat);
        exit(-1);
    }
    if(fseek(stat, name_end_ind + 4, SEEK_SET) == -1) { // ERROR
        perror("get_start_jiffies(): Cannot seek stat file");
        fclose(stat);
        exit(-1);
    }
    int i;
    for(i = 0; i < 19; i++) {
        fscanf(stat, (i == 18) ? "%lu" : "%*s", &start_jiffies);// Only save the 22nd filed(process start time)
    }
    fclose(stat);
    return start_jiffies;
}

// Get system boot time(measured in seconds)
time_t get_boot_time()
{
    struct sysinfo info;
    if(sysinfo(&info) == -1) {
        perror("get_boot_time(): Cannot get system information");
        exit(-1);
    }
    // System boot time(the time from the UNIX epoch to the present which is measured in seconds)
    time_t boot_time = time(NULL) - info.uptime;
    return boot_time;
}

// Get process CPU utilization time(measured in jiffies)
unsigned long get_proc_cpu_time(const char* proc_dir)
{
    FILE* stat;// stat file
    if(open_proc_file(proc_dir, "stat", &stat) == 0) { // ERROR
        printf("get_proc_cpu_time(): Cannot open stat file of process %s\n", proc_dir);
        exit(-1);
    }
    // Read stat file to get utime and stime
    unsigned long utime, stime;// User mode time，System mode time
    char line[1024];
    if(fgets(line, sizeof(line), stat) != NULL) {
        int name_end_ind = get_end_of_proc_name(proc_dir);
        if(name_end_ind == -1) {
            printf("get_proc_cpu_time(): Name error of process %s\n", proc_dir);
            fclose(stat);
            exit(-1);
        }
        sscanf(line + name_end_ind + 4, "%*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %lu %lu", &utime, &stime);
    }
    else { //ERROR
        printf("get_proc_cpu_time(): Cannot read stat file of process %s\n", proc_dir);
        fclose(stat);
        exit(-1);
    }
    fclose(stat);
    unsigned long proc_cpu_time = utime + stime;// Process CPU time
    return proc_cpu_time;
}

// Get elapsed time of a process(measured in jiffies)
unsigned long get_elapsed_time(const char* proc_dir, time_t boot_time, time_t current_time, long jiffies_per_sec)
{
    unsigned long proc_cpu_time = get_proc_cpu_time(proc_dir);
    unsigned long start_jiffies = get_start_jiffies(proc_dir);
    // Caculate the process start time(the time from the UNIX epoch to the present which is measured in seconds)
    time_t start_time = boot_time + (start_jiffies / jiffies_per_sec);
    time_t elapsed_sec = current_time - start_time;// Elapsed time(measured in seconds)
    unsigned long elapsed_time = elapsed_sec * jiffies_per_sec;// Elapsed time(measured in jiffies)
    return elapsed_time;
}

// Get CPU unilization(C) of a process
int get_cpu_util(const char* proc_dir, time_t boot_time, time_t current_time, long jiffies_per_sec)
{
    unsigned long proc_cpu_time = get_proc_cpu_time(proc_dir);
    unsigned long elapsed_time = get_elapsed_time(proc_dir, boot_time, current_time, jiffies_per_sec);
    if(proc_cpu_time >= 0 && elapsed_time >=0) {
        int cpu_util = ((double)proc_cpu_time / (double)elapsed_time) * 100.0;
        return cpu_util;
    }
    else { //ERROR
        printf("get_cpu_util(): CPU time error\n");
        exit(-1);
    }
}

// Get the start time of a process(STIME)
void get_stime(const char* proc_dir, char* time_str, size_t buffer_size, time_t boot_time, time_t current_time, long jiffies_per_sec)
{
    unsigned long start_jiffies = get_start_jiffies(proc_dir);
    // Caculate the process start time(the time from the UNIX epoch to the present which is measured in seconds)
    time_t start_time = boot_time + (start_jiffies / jiffies_per_sec);
    struct tm start_tm;
    if(localtime_r(&start_time, &start_tm) != NULL) { // Get formatted time struct
        // If the procees was started in current day, return hour and minute, otherwise return month and date
        struct tm current_tm;
        localtime_r(&current_time, &current_tm);
        // If the process was started in current day
        if(start_tm.tm_year == current_tm.tm_year
            && start_tm.tm_mon == current_tm.tm_mon
            && start_tm.tm_mday == current_tm.tm_mday) {
                strftime(time_str, buffer_size, "%H:%M", &start_tm);// Return hour and minute
            }
        else {
            strftime(time_str, buffer_size, "%b %d", &start_tm);// Return month and date
        }
    }
    else {
        printf("get_stime(): Cannot parse process start time\n");
        exit(-1);
    }
}

// Get the tty number of a process
int get_tty_nr(const char* proc_dir)
{
    FILE* stat;// stat file
    if(open_proc_file(proc_dir, "stat", &stat) == 0) { // ERROR
        printf("get_tty_nr(): Cannot open stat file of process %s\n", proc_dir);
        exit(-1);
    }
    // Read stat file to get tty number
    int tty_nr;
    char line[1024];
    if(fgets(line, sizeof(line), stat) != NULL) {
        int name_end_ind = get_end_of_proc_name(proc_dir);
        if(name_end_ind == -1) { // ERROR
            printf("get_tty_nr(): Name error of process %s\n", proc_dir);
            fclose(stat);
            exit(-1);
        }
        sscanf(line + name_end_ind + 4, "%*d %*d %*d %d", &tty_nr);
        fclose(stat);
        return tty_nr;
    }
    else { // ERROR
        printf("get_tty_nr(): Cannot read stat file");
        fclose(stat);
        exit(-1);
    }
}

// Match terminal, return 1 if succeed, otherwise return 0
// Note that stdin, stdout and stderr actually refer to pts/0
int match_term(const char* dev_path, int major_num, int minor_num, char* tty_name, size_t buffer_size)
{
    DIR* dev_dir = opendir(dev_path);
    struct dirent* dir_entry;
    struct stat dev_stat;
    if(!dev_dir) {
        perror("match_term(): Cannot open directory");
        return 0;
    }
    while((dir_entry = readdir(dev_dir)) != NULL) {
        char path[512];// Path of the device file
        snprintf(path, sizeof(path), "%s/%s", dev_path, dir_entry->d_name);
        // Check if current device is matched
        if(stat(path, &dev_stat) == 0 && S_ISCHR(dev_stat.st_mode)) {
            // Compare device number
            if(major(dev_stat.st_rdev) == major_num && minor(dev_stat.st_rdev) == minor_num) { // Matched
                if(strcmp(path, "/dev/stdin") == 0 
                    || strcmp(path, "/dev/stdout") == 0 
                    || strcmp(path, "/dev/stderr") == 0) {
                        strncpy(tty_name, "pts/0", buffer_size - 1);
                        closedir(dev_dir);
                        return 1;
                    }
                else {
                    int i;
                    int j = 0;
                    for(i = 5; i < strlen(path); i++) { // Omit "/dev/"
                        if(j < buffer_size) {
                            tty_name[j] = path[i];
                            j++;
                        }
                    }
                closedir(dev_dir);
                return 1;
                }
            }
        }
    }
    closedir(dev_dir);
    return 0;
}

// Get tty name of a process
void get_tty(const char* proc_dir, char* tty_name, size_t buffer_size)
{
    int tty_nr = get_tty_nr(proc_dir);
    if(tty_nr == 0) { // The process does not use a tty device
        strncpy(tty_name, "?", buffer_size);// TTY name is set as ? 
        return;
    }
    int major_num = major(tty_nr);// Major device numebr
    int minor_num = minor(tty_nr);// Minor device number
    // Match physical terminal
    if(match_term("/dev", major_num, minor_num, tty_name, buffer_size) == 1) {
        return;
    }
    else if(match_term("/dev/pts", major_num, minor_num, tty_name, buffer_size) == 1) { // Match pseudo terminal
        return;
    }
    else { // Unknown tty
        strncpy(tty_name, "?", buffer_size);// TTY name is set as ?
        return;
    }
}

// Get process CPU utilization time(TIME, measured in seconds)
void get_time(const char* proc_dir, char* time_str, size_t buffer_size, long jiffies_per_sec)
{
    unsigned long proc_cpu_time = get_proc_cpu_time(proc_dir);
    unsigned long proc_cpu_time_sec = proc_cpu_time / jiffies_per_sec;
    unsigned long hours = proc_cpu_time_sec / 3600;
    unsigned long minutes = (proc_cpu_time_sec % 3600) / 60;
    unsigned long seconds = proc_cpu_time_sec % 60;
    snprintf(time_str, buffer_size, "%02ld:%02ld:%02ld", hours, minutes, seconds);
}

// Get startup command of a process
void get_cmd(const char* proc_dir, char* cmd, size_t buffer_size)
{
    FILE* cmdline;// cmdline file
    if(open_proc_file(proc_dir, "cmdline", &cmdline) == 0) { // ERROR
        printf("get_cmd(): Cannot open cmdline file of process %s\n", proc_dir);
        exit(-1);
    }
    // Read cmdline to get startup command(maximum 54 bytes)
    size_t read_bytes = fread(cmd, 1, 54, cmdline);// Use fread to tackle with \0 in cmdline file
    if(read_bytes > 0) {
        fclose(cmdline);
        int i;
        for(i = 0; i < read_bytes; i++) {
            if(cmd[i] == '\0') {
                cmd[i] = ' ';
            }
        }
        cmd[read_bytes] = '\0';// Add a \0 to mark the end
    }
    else { // The process does not have a startup command, then get name of the process
        fclose(cmdline);
        FILE* comm;// comm file
        if(open_proc_file(proc_dir, "comm", &comm) == 0) { // ERROR
            printf("get_cmd(): Cannot open comm file of process %s\n", proc_dir);
            exit(-1);
        }
        char proc_name[100];
        if(fscanf(comm, "%s", proc_name) != EOF) {

        }
        else { // ERROR
            printf("get_cmd(): Cannot read comm file of process %s\n", proc_dir);
            fclose(comm);
            exit(-1);
        }
        fclose(comm);
        snprintf(cmd, buffer_size, "[%s]", proc_name);
    }
}

int main()
{
    // Read /proc to get all processes
    DIR* dir;
    struct dirent* ptr;
    dir = opendir("/proc");// Open /proc
    time_t boot_time = get_boot_time();// System boot time
    time_t current_time = time(NULL);// Current_time
    long jiffies_per_sec = sysconf(_SC_CLK_TCK);// Get number of jiffies in per second
    // Read process directories in /proc
    printf("%-10s %-8s %-8s %-2s %-6s %-8s %-10s %-s\n", "UID", "PID", "PPID", "C" ,"STIME", "TTY" , "TIME" , "CMD");
    while((ptr = readdir(dir)) != NULL) {
        char* proc_dir = ptr->d_name;
        if(ptr->d_type == DT_DIR && is_process(proc_dir) == 1) {
            char uname[16];
            get_uname(proc_dir, uname);
            int pid, ppid;
            get_pid_and_ppid(proc_dir, &pid, &ppid);
            int C = get_cpu_util(proc_dir, boot_time, current_time, jiffies_per_sec);
            char stime[16];
            get_stime(proc_dir, stime, sizeof(stime), boot_time, current_time, jiffies_per_sec);
            char tty[64];
            get_tty(proc_dir, tty, sizeof(tty));
            char time[64];
            get_time(proc_dir, time, sizeof(time), jiffies_per_sec);
            char cmd[256];
            get_cmd(proc_dir, cmd, sizeof(cmd));
            printf("%-10s %-8d %-8d %-2d %-6s %-8s %-10s %-s\n",uname, pid, ppid, C ,stime, tty, time, cmd);
        }
    }
    closedir(dir);
    exit(0);
}