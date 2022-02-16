#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
    if(argc < 2)
    {
        printf(1, "Please enter the file path.\n");
        exit();
    }
    int fd = open(argv[1], 0);
    if(fd < 0)
    {
        printf(1, "File not found.\n");
        exit();
    }
    int sectors[13];
    if(get_file_sectors(fd, sectors) < 0)
    {
        printf(1, "Error while running get file sectors system call.\n");
        exit();
    }
    for(int i = 0; i < 13; i++)
        printf(1, "block %d in sector %d\n", i + 1, sectors[i]);
    exit();
}