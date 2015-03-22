#include <stdlib.h>
#include <stdio.h>

#include "debug.h"
#include "leds.h"

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
