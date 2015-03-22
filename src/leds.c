#include <stdio.h>

#include "leds.h"
#include "math.h"

void leds_dump_obj(struct leds *leds)
{
	int i;

	printf("# ouvrt OBJ File: ''\n"
	       "o rift-dk2\n");

	for (i = 0; i < leds->num; i++) {
		vec3 *p = &leds->positions[i];
		vec3 *d = &leds->directions[i];

		printf("v %9.6f %9.6f %9.6f\n"
		       "v %9.6f %9.6f %9.6f\n",
		       p->x, p->y, p->z,
		       p->x + d->x, p->y + d->y, p->z + d->z);
	}
	for (i = 0; i < leds->num; i++)
		printf("l %d %d\n", i * 2 + 1, i * 2 + 2);
}

void leds_dump_struct(struct leds *leds)
{
	int i;

	printf("struct leds leds = {\n"
	       "\t.num = %d\n"
	       "\t.positions = {\n", leds->num);

	for (i = 0; i < leds->num; i++) {
		vec3 *p = &leds->positions[i];

		printf("\t\t{ %9.6f, %9.6f, %9.6f },\n", p->x, p->y, p->z);
	}

	printf("\t},\n"
	       "\t.directions = {\n");

	for (i = 0; i < leds->num; i++) {
		vec3 *d = &leds->directions[i];

		printf("\t\t{ %9.6f, %9.6f, %9.6f },\n", d->x, d->y, d->z);
	}

	printf("\t},\n"
	       ".patterns = {\n");

	for (i = 0; i < leds->num; i++)
		printf("\t\t0x%03x,\n", leds->patterns[i]);

	printf("\t},\n"
	       "};\n");
}
