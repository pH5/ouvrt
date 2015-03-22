#include <stdint.h>
#include <stdlib.h>

#include "debug.h"
#include "blobwatch.h"
#include "imu.h"
#include "leds.h"

int debug_mode = 0;

#define IMU_FIFO_LEN 32
static struct imu_state imu_fifo[IMU_FIFO_LEN];
static unsigned int fifo_in = 0;
static unsigned int fifo_out = 0;

unsigned int debug_imu_fifo_in(struct imu_state *samples, unsigned int n)
{
	int i = 0;

	while (n--) {
		if (((fifo_out + 1) % IMU_FIFO_LEN) == (fifo_in % IMU_FIFO_LEN))
			return i;
		imu_fifo[fifo_out % IMU_FIFO_LEN] = *samples;
		__sync_synchronize();
		fifo_out++;
		samples++;
		i++;
	}

	return i;
}

unsigned int debug_imu_fifo_out(struct imu_state *samples, unsigned int n)
{
	int i = 0;

	while (n--) {
		if ((fifo_in % IMU_FIFO_LEN) == (fifo_out % IMU_FIFO_LEN))
			return i;
		*samples = imu_fifo[fifo_in % IMU_FIFO_LEN];
		__sync_synchronize();
		fifo_in++;
		samples++;
		i++;
	}

	return i;
}
