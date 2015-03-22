#include <stdlib.h>
#include <stdio.h>

#include "debug.h"
#include "leds.h"
#include "math.h"
#include "opencv.h"

static struct leds *global_leds = NULL;

void tracker_register_leds(struct leds *leds)
{
	if (global_leds == NULL)
		global_leds = leds;
}

void tracker_unregister_leds(struct leds *leds)
{
	if (global_leds == leds)
		global_leds = NULL;
}

void tracker_process(struct blob *blobs, int num_blobs,
		     double camera_matrix[9], double dist_coeffs[5],
		     dquat *rot, dvec3 *trans)
{
	struct leds *leds = global_leds;

	if (leds == NULL)
		return;

	estimate_initial_pose(blobs, num_blobs, leds->positions, leds->num,
			      camera_matrix, dist_coeffs, rot, trans,
			      true);
}
