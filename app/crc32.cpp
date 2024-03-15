#include "crc32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdexcept>

#define BUFSIZE   16 * 1024

static unsigned int crc_table[256];

void init_crc_table(void)
{
    unsigned int i, j;
 
    for (i = 0; i < 256; i++) {
        unsigned int c = i;
        for (j = 0; j < 8; j++) {
            if (c & 1)
                c = 0xedb88320 ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc_table[i] = c;
    }
}
 
unsigned int crc32(unsigned int crc,unsigned char *buffer, unsigned int size)
{
    unsigned int i = crc ^ 0xffffffff;
    while (size--)
    {
        i = (i >> 8) ^ crc_table[(i & 0xff) ^ *buffer++];
    }
    return i ^ 0xffffffff;
}
 
void calc_crc(const char *in_file, unsigned int *file_crc)
{
    int fd;
    int nread;
    unsigned char buf[BUFSIZE];
    unsigned int crc = 0;
 
    fd = open(in_file, O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error( std::string("open failed: ") + strerror(errno) );
    }
 
    while ((nread = read(fd, buf, BUFSIZE)) > 0) {
        crc = crc32(crc, buf, nread);
    }
    *file_crc = crc;
 
    close(fd);
 
    if (nread < 0) {
        throw std::runtime_error( std::string("read failed: ") + strerror(errno) );
    }

}
