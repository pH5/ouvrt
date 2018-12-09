/*
 * Position estimation and tracking
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#ifndef __TRACKER_H__
#define __TRACKER_H__

#include <glib-object.h>
#include <stdint.h>

#include "maths.h"

#define OUVRT_TYPE_TRACKER (ouvrt_tracker_get_type())
G_DECLARE_FINAL_TYPE(OuvrtTracker, ouvrt_tracker, OUVRT, TRACKER, GObject)

struct leds;
struct blob;
struct blobservation;

void ouvrt_tracker_register_leds(OuvrtTracker *tracker, struct leds *leds);
void ouvrt_tracker_unregister_leds(OuvrtTracker *tracker, struct leds *leds);

void ouvrt_tracker_set_radio_address(OuvrtTracker *tracker,
				     const uint8_t address[5]);
void ouvrt_tracker_get_radio_address(OuvrtTracker *tracker, uint8_t address[5]);

void ouvrt_tracker_add_exposure(OuvrtTracker *tracker,
				uint64_t device_timestamp, uint64_t time,
				uint8_t led_pattern_phase);

void ouvrt_tracker_process_frame(OuvrtTracker *tracker, uint8_t *frame,
				 int width, int height, uint64_t sof_time,
				 struct blobservation **ob);
void ouvrt_tracker_process_blobs(OuvrtTracker *tracker,
				 struct blob *blobs, int num_blobs,
				 dmat3 *camera_matrix, double dist_coeffs[5],
				 dquat *rot, dvec3 *trans);

OuvrtTracker *ouvrt_tracker_new();

#endif /* __TRACKER_H__ */
