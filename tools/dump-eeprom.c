#include <stdint.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "esp570.h"

/*
 * Dumps the Oculus Positional Tracker DK2 EEPROM 
 */
int main(int argc, char *argv[])
{
	char buf[0x20];
	int fd = open("/dev/video0", O_RDWR);
	int outfd;
	int addr;
	int ret;

	if (argc != 2) {
		fprintf(stderr, "usage: dump-eeprom <file.bin>\n");
		return -1;
	}

	if (fd < 0) {
		fprintf(stderr, "failed to open /dev/video0\n");
		return -1;
	}

	if (argv[1][0] == '-' && argv[1][1] == '\0') {
		outfd = 1;
	} else {
		outfd = open(argv[1], O_CREAT | O_WRONLY, 0664);
		if (outfd < 0) {
			fprintf(stderr, "failed to open '%s'\n", argv[1]);
			return -1;
		}
	}

	for (addr = 0; addr < 0x4000; addr += 0x20) {
		ret = esp570_eeprom_read(fd, addr, 0x20, buf);
		if (ret < 0) {
			fprintf(stderr, "failed to read at address 0x%04x\n",
				addr);
			return -1;
		}
		write(outfd, buf, 0x20);
	}

	close(fd);
	if (outfd != 1)
		close(outfd);

	return 0;
}
