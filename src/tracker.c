/*
 * Position estimation and tracking
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#include <stdlib.h>

#include "blobwatch.h"
#include "debug.h"
#include "leds.h"
#include "maths.h"
#include "opencv.h"
#include "tracker.h"

struct _OuvrtTracker {
	GObject parent_instance;
	struct blobwatch *bw;
	struct leds leds;
	uint32_t radio_address;
};

G_DEFINE_TYPE(OuvrtTracker, ouvrt_tracker, G_TYPE_OBJECT)

void ouvrt_tracker_register_leds(OuvrtTracker *tracker, struct leds *leds)
{
	if (!tracker || tracker->leds.model.num_points)
		return;

	leds_copy(&tracker->leds, leds);
}

void ouvrt_tracker_unregister_leds(G_GNUC_UNUSED OuvrtTracker *tracker,
				   G_GNUC_UNUSED struct leds *leds)
{
//	tracker->leds.gone = true;
}

void ouvrt_tracker_set_radio_address(OuvrtTracker *tracker, uint32_t address)
{
	tracker->radio_address = address;
}

uint32_t ouvrt_tracker_get_radio_address(OuvrtTracker *tracker)
{
	return tracker->radio_address;
}

void ouvrt_tracker_process_frame(OuvrtTracker *tracker, uint8_t *frame,
				 int width, int height, int skipped,
				 struct blobservation **ob)
{
	if (tracker->bw == NULL)
		tracker->bw = blobwatch_new(width, height);

	blobwatch_process(tracker->bw, frame, width, height, skipped,
			  &tracker->leds, ob);
}

void ouvrt_tracker_process_blobs(OuvrtTracker *tracker,
				 struct blob *blobs, int num_blobs,
				 dmat3 *camera_matrix, double dist_coeffs[5],
				 dquat *rot, dvec3 *trans)
{
	struct leds *leds = &tracker->leds;

	if (leds == NULL)
		return;

	/*
	 * Estimate initial pose without previously known [rot|trans].
	 */
	estimate_initial_pose(blobs, num_blobs, leds->model.points,
			      leds->model.num_points,
			      camera_matrix, dist_coeffs, rot, trans,
			      true);
}

static void ouvrt_tracker_class_init(OuvrtTrackerClass *klass G_GNUC_UNUSED)
{
}

static void ouvrt_tracker_init(OuvrtTracker *self)
{
	leds_fini(&self->leds);
}

OuvrtTracker *ouvrt_tracker_new(void)
{
	return g_object_new(OUVRT_TYPE_TRACKER, NULL);
}
