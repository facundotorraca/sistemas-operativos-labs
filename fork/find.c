#define _GNU_SOURCE //strcasestr

#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

//Case sensitive
#define ARGS_CS 2
#define CS_MODE 1

//Case insensitive
#define ARGS_CI 3
#define CI_MODE 2
#define CI_FLAG "-i"

#define ERROR -1
#define SUCCESS 0

static bool is_subdir(const char* dir_name) {
    bool is_current_dir = (strcmp(dir_name,".") == 0);
    bool is_parent_dir = (strcmp(dir_name, "..") == 0);

    return !is_current_dir && !is_parent_dir;
}

static bool is_substr_cs(const char* str, const char* substr) {
    return strstr(str, substr) != NULL;
}

static bool is_substr_ci(const char* str, const char* substr) {
    return strcasestr(str, substr) != NULL;
}

static DIR* get_next_dir(DIR* curr_dir, const char* next_dir_name) {
    int curr_dir_fd = dirfd(curr_dir);
    //with O_DIRECTORY flag, if name is not a directory, fail
    int next_dir_fd = openat(curr_dir_fd, next_dir_name, O_DIRECTORY);
    DIR* new_dir = fdopendir(next_dir_fd);
    return new_dir;
}

static size_t update_path(char* path, const char* next_dir_name, size_t path_len) {
    size_t rem_path = PATH_MAX - path_len;

    //Clean previous iterarions paths
    memset(&path[path_len], 0, rem_path);

    //+1 for "\0" +1 for "/"
    if (strlen(next_dir_name) + 2 > rem_path) {
        printf("no se entra a %s%s: longitud supera %u bytes\n",
               path, next_dir_name, PATH_MAX);
        return 0;
    }

    //Append new directory name
    strcat(path, next_dir_name);
    strcat(path, "/");

    return path_len + strlen(next_dir_name) + 1;
}

static void find(const char* exp, DIR* curr_dir, char* path, size_t path_len,
                 bool close_dir, bool (*is_substr)(const char*, const char*)) {

    struct dirent* entry = readdir(curr_dir);

    if (!entry) return; //No more files or folders

    if ((*is_substr)(entry->d_name, exp))
        printf("%.*s%s\n", (int)path_len, path, entry->d_name);

    //Continues dipping. This make files/folders be printed in order
    find(exp, curr_dir, path, path_len, false, is_substr);

    if (entry->d_type == DT_DIR && is_subdir(entry->d_name)) {

        DIR* next_dir = get_next_dir(curr_dir, entry->d_name);
        size_t new_path_len = update_path(path, entry->d_name, path_len);

        if (new_path_len != 0) //buffer suports path length
            find(exp, next_dir, path, new_path_len, true, is_substr);
        else //buffer dont support path length
            closedir(next_dir);
    }

    //if close_dir is true, this is the last call that uses the directory
    if (close_dir) closedir(curr_dir);
}

static void find_all(const char* exp, DIR* root, bool insensitive) {
    char path_buff[PATH_MAX];
    memset(path_buff, 0, PATH_MAX);

    bool (*is_substr)(const char*, const char*);
    is_substr = (insensitive) ? is_substr_ci : is_substr_cs;

    //Search in all directories recursively
    find(exp, root, path_buff, 0/*initial path len*/, true, is_substr);
}

static int verify_args(int argc, char* const argv[]) {
    if (argc == ARGS_CI && strcmp(argv[1], CI_FLAG) == 0)
        return CI_MODE;

    if (argc == ARGS_CS)
        return CS_MODE;

    return ERROR;
}

static char* get_exp(int mode, char* const argv[]) {
    return (mode == CI_MODE) ? argv[2] : argv[1];
}

int main(int argc, char* const argv[]) {
    int mode = verify_args(argc, argv);

    if (mode == ERROR) {
        fprintf(stderr, "args error\n");
        return ERROR;
    }
    
    //expression to be searched
    const char* exp = get_exp(mode, argv);
    bool insensitive = (mode == CI_MODE);

    //opens root directory
    DIR* root = opendir(".");

    find_all(exp, root, insensitive);
    return SUCCESS;
}
