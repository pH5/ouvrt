/*
 * Position estimation and tracking
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __TRACKER_H__
#define __TRACKER_H__

#include <glib-object.h>
#include <stdint.h>

#include "math.h"

#define OUVRT_TYPE_TRACKER		(ouvrt_tracker_get_type())
#define OUVRT_TRACKER(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), \
					 OUVRT_TYPE_TRACKER, OuvrtTracker))
#define OUVRT_IS_TRACKER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
					 OUVRT_TYPE_TRACKER))
#define OUVRT_TRACKER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), \
					 OUVRT_TYPE_TRACKER, \
					 OuvrtTrackerClass))
#define OUVRT_IS_TRACKER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), \
					 OUVRT_TYPE_TRACKER))
#define OUVRT_TRACKER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), \
					 OUVRT_TYPE_TRACKER, \
					 OuvrtTrackerClass))

typedef struct _OuvrtTracker		OuvrtTracker;
typedef struct _OuvrtTrackerClass	OuvrtTrackerClass;
typedef struct _OuvrtTrackerPrivate	OuvrtTrackerPrivate;

struct _OuvrtTracker {
	GObject parent_instance;

	OuvrtTrackerPrivate *priv;
};

struct _OuvrtTrackerClass {
	GObjectClass parent_class;
};

GType ouvrt_tracker_get_type(void);

struct leds;
struct blob;
struct blobservation;

void ouvrt_tracker_register_leds(OuvrtTracker *tracker, struct leds *leds);
void ouvrt_tracker_unregister_leds(OuvrtTracker *tracker, struct leds *leds);

void ouvrt_tracker_process_frame(OuvrtTracker *tracker,
				 uint8_t *frame, int width, int height,
				 int skipped, struct blobservation **ob);
void ouvrt_tracker_process_blobs(OuvrtTracker *tracker,
				 struct blob *blobs, int num_blobs,
				 dmat3 *camera_matrix, double dist_coeffs[5],
				 dquat *rot, dvec3 *trans);

OuvrtTracker *ouvrt_tracker_new();

#endif /* __TRACKER_H__ */
