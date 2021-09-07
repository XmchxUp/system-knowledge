#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <ftw.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_PATH 1024

static char dest_root_path[MAX_PATH];
static char source_root_path[MAX_PATH];

static void copy_file(const char *filepath);
static void copy_diretory(const char *filepath);
static void copy_symlink(const char *filepath);
static int doit(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf);
static void usage(char **argv);
static int is_directory(const char *path);

// ./a.out ./tmp ./tmp1
int main(int argc, char **argv)
{
    if (argc != 3)
    {
        usage(argv);
    }

    if (!is_directory(argv[1]) ||
        !is_directory(argv[2]))
    {
        usage(argv);
    }

    // 统一输入的路径名样式风格, 防止后面处理出错
    strcpy(source_root_path, argv[1]);
    strcpy(dest_root_path, argv[2]);
    int n1 = strlen(source_root_path);
    int n2 = strlen(dest_root_path);
    if (source_root_path[n1 - 1] == '/')
    {
        source_root_path[n1 - 1] = '\0';
    }
    if (dest_root_path[n2 - 1] == '/')
    {
        dest_root_path[n2 - 1] = '\0';
    }

    printf("复制目录 %s 到 %s\n", argv[1], dest_root_path);
    // 默认DFS,可以设置FTW_DEPTH变成BFS(即当前目录全部遍历完在进入一下个)
    if (nftw(argv[1], doit, 20, FTW_PHYS) == -1)
    {
        perror("nftw");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

static int doit(const char *fpath, const struct stat *sb,
                int tflag, struct FTW *ftwbuf)
{
    int retval = 0; /* default to tell nftw() to continue */
    const char *filename = fpath + ftwbuf->base;

    printf("%*s", ftwbuf->level * 2, ""); /* indent over */

    switch (tflag)
    {
    case FTW_F: /* regular file */
        copy_file(fpath);
        break;
    case FTW_D: /* directory */
        copy_diretory(fpath);
        break;
    case FTW_SL: /* symbol link */
        copy_symlink(fpath);
        break;
    default:
        printf("can't copy %s\n", filename);
        retval = 1;
        break;
    }

    printf("%s\n", filename);
    return retval;
}

#define BUFSIZE 4096

static void copy_file(const char *filepath)
{
    int ch;
    char tmp_full_path[MAX_PATH];
    const char *p = filepath + strlen(source_root_path);

    strcpy(tmp_full_path, dest_root_path);
    strcat(tmp_full_path, p);
    // printf("%s\n", tmp_full_path);

    // open创建文件时要设置权限，先获取在设置

    struct stat st;
    int source_fd = open(filepath, O_RDONLY);
    fstat(source_fd, &st);

    // 发现create设置不了可执行权限
    // open可以，下面全显示重新设置权限
    int dest_fd = open(tmp_full_path, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    int n;
    char buf[BUFSIZE];
    while ((n = read(source_fd, buf, BUFSIZE)) > 0)
    {
        if (write(dest_fd, buf, n) < 0)
        {
            fprintf(stderr, "write file(%s) error", tmp_full_path);
            exit(EXIT_FAILURE);
        }
    }

    if (n < 0)
    {
        fprintf(stderr, "copy file(%s) error", filepath);
        exit(EXIT_FAILURE);
    }

    chmod(tmp_full_path, st.st_mode);

    close(dest_fd);
    close(source_fd);
    // 流打开 内部好像默认获取了权限？
    // 发现：可执行权限处理不了
    /*    
    FILE *source = fopen(filepath, "r");
    FILE *dest = fopen(tmp_full_path, "w");

    while ((ch = fgetc(source)) != EOF)
    {
        if (fputc(ch, dest) == EOF)
        {
            fprintf(stderr, "copy file error");
            exit(EXIT_FAILURE);
        }
    }

    fclose(dest);
    fclose(source);
    */
}

static const int kDefaultDirMode = 0755;

static void copy_diretory(const char *filepath)
{
    // /name
    const char *p = filepath;
    p += strlen(source_root_path);

    char tmp_full_path[MAX_PATH];
    strcpy(tmp_full_path, dest_root_path);
    strcat(tmp_full_path, p);

    if (access(tmp_full_path, F_OK) < 0) // 目录不存在
    {
        struct stat st;
        stat(filepath, &st);
        if (mkdir(tmp_full_path, kDefaultDirMode) < 0)
        {
            fprintf(stderr, "创建%s目录失败\n", tmp_full_path);
            exit(EXIT_FAILURE);
        }
        chmod(tmp_full_path, st.st_mode);
    }
    else if (!is_directory(tmp_full_path))
    { // 有同名文件
        fprintf(stderr, "复制%s目录失败，有同名文件\n", tmp_full_path);
        exit(EXIT_FAILURE);
    }
}

static void copy_symlink(const char *filepath)
{
    char buf[BUFSIZE];
    int n;
    // 读取符号连接目标
    n = readlink(filepath, buf, BUFSIZE);
    if (n == -1)
    {
        fprintf(stderr, "read %s(symbol link) error\n", filepath);
        exit(EXIT_FAILURE);
    }
    buf[n] = '\0'; // 这里是个坑,养成好习惯

    char tmp_full_path[MAX_PATH];
    const char *p = filepath + strlen(source_root_path);

    strcpy(tmp_full_path, dest_root_path);
    strcat(tmp_full_path, p);

    symlink(buf, tmp_full_path);

    struct stat st;
    stat(filepath, &st);
    chmod(tmp_full_path, st.st_mode);
}

static void usage(char **argv)
{
    printf("usage: %s <source-pathname> <dest-pathname>\n", argv[0]);
    exit(EXIT_SUCCESS);
}

static int is_directory(const char *path)
{
    DIR *dp;
    if ((dp = opendir(path)) == NULL)
    {
        return 0;
    }
    else
    {
        closedir(dp);
        return 1;
    }
}