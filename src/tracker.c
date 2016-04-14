/*
 * Position estimation and tracking
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#include <stdlib.h>
#include <stdio.h>

#include "blobwatch.h"
#include "debug.h"
#include "leds.h"
#include "math.h"
#include "opencv.h"
#include "tracker.h"

struct _OuvrtTrackerPrivate {
	struct blobwatch *bw;
	struct leds *leds;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtTracker, ouvrt_tracker, G_TYPE_OBJECT)

void ouvrt_tracker_register_leds(OuvrtTracker *tracker, struct leds *leds)
{
	if (!tracker || tracker->priv->leds)
		return;

	tracker->priv->leds = leds;
}

void ouvrt_tracker_unregister_leds(OuvrtTracker *tracker, struct leds *leds)
{
	if (!tracker || tracker->priv->leds != leds)
		return;

	tracker->priv->leds = NULL;
}

void ouvrt_tracker_process_frame(OuvrtTracker *tracker, uint8_t *frame,
				 int width, int height, int skipped,
				 struct blobservation **ob)
{
	OuvrtTrackerPrivate *priv = tracker->priv;

	if (priv->bw == NULL)
		priv->bw = blobwatch_new(width, height);

	blobwatch_process(priv->bw, frame, width, height, skipped,
			  priv->leds, ob);
}

void ouvrt_tracker_process_blobs(OuvrtTracker *tracker,
				 struct blob *blobs, int num_blobs,
				 dmat3 *camera_matrix, double dist_coeffs[5],
				 dquat *rot, dvec3 *trans)
{
	struct leds *leds = tracker->priv->leds;

	if (leds == NULL)
		return;

	estimate_initial_pose(blobs, num_blobs, leds->positions, leds->num,
			      camera_matrix, dist_coeffs, rot, trans,
			      true);
}

static void ouvrt_tracker_class_init(OuvrtTrackerClass *klass G_GNUC_UNUSED)
{
}

static void ouvrt_tracker_init(OuvrtTracker *self)
{
	self->priv = ouvrt_tracker_get_instance_private(self);
	self->priv->leds = NULL;
}

OuvrtTracker *ouvrt_tracker_new(void)
{
	return g_object_new(OUVRT_TYPE_TRACKER, NULL);
}
