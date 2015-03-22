struct leds;
struct blob;

void tracker_register_leds(struct leds *leds);
void tracker_unregister_leds(struct leds *leds);

void tracker_process(struct blob *blobs, int num_blobs,
		     double camera_matrix[9], double dist_coeffs[5],
		     dquat *rot, dquat *trans);
