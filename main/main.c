#include <stdio.h>
#include <dirent.h>

int main(int argc, char *argv[])
{
    DIR *dir;
    struct dirent *ent;
    if (argc != 2)
    {
        printf("用法: %s <目录>\n", argv[0]);
        return 1;
    }
    if ((dir = opendir(argv[1])) == NULL)
    {
        printf("不能打开 %s\n", argv[1]);
        return 1;
    }
    while ((ent = readdir(dir)) != NULL)
    {
        printf("%s\n", ent->d_name);
    }
    closedir(dir);
    return 0;
}