#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdint.h>

#define DEBUG_MODE_SHM	1
#define DEBUG_MODE_X	2
#define DEBUG_MODE_PNG	3

#include "blobwatch.h"

extern int debug_mode;

struct ouvrt_debug_attachment {
	struct blobservation blobservation;
};

int debug_parse_arg(const char *arg);

#endif /* __DEBUG_H__ */
