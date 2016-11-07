/*
 * Oculus Rift CV1 Radio
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#ifndef __RIFT_RADIO_H__
#define __RIFT_RADIO_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_RIFT_RADIO		(ouvrt_rift_radio_get_type())
#define OUVRT_RIFT_RADIO(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), \
					 OUVRT_TYPE_RIFT_RADIO, OuvrtRiftRadio))
#define OUVRT_IS_RIFT_RADIO(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
					 OUVRT_TYPE_RIFT_RADIO))
#define OUVRT_RIFT_RADIO_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), \
					 OUVRT_TYPE_RIFT_RADIO, \
					OuvrtRiftRadioClass))
#define OUVRT_IS_RIFT_RADIO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
					 OUVRT_TYPE_RIFT_RADIO))
#define OUVRT_RIFT_RADIO_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), \
					 OUVRT_TYPE_RIFT_RADIO, \
					 OuvrtRiftRadioClass))

typedef struct _OuvrtRiftRadio		OuvrtRiftRadio;
typedef struct _OuvrtRiftRadioClass	OuvrtRiftRadioClass;
typedef struct _OuvrtRiftRadioPrivate	OuvrtRiftRadioPrivate;

struct _OuvrtRiftRadioPrivate;

struct _OuvrtRiftRadio {
	OuvrtDevice dev;

	OuvrtRiftRadioPrivate *priv;
};

struct _OuvrtRiftRadioClass {
	OuvrtDeviceClass parent_class;
};

GType ouvrt_rift_radio_get_type(void);

OuvrtDevice *rift_cv1_radio_new(const char *devnode);

#endif /* __RIFT_RADIO_H__ */
