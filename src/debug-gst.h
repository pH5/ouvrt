#include <stdint.h>

struct debug_gst;

void debug_gst_init(int argc, char *argv[]);
struct debug_gst *debug_gst_new(int width, int height, int framerate);
struct debug_gst *debug_gst_unref(struct debug_gst *gst);
uint32_t *debug_gst_frame_new(struct debug_gst *gst, uint8_t *frame,
			      int width, int height);
void debug_gst_frame_push(struct debug_gst *gst);
