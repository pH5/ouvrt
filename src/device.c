#include <stdlib.h>
#include <string.h>

#include "device.h"

/*
 * Decrements the reference count. If refcount reaches zero, stops the device
 * and calls the device specific function to free the device structure.
 *
 * Returns NULL.
 */
struct device *device_unref(struct device *dev)
{
	dev->refcount--;

	if (dev->refcount <= 0) {
		device_stop(dev);
		dev->ops->free(dev);
	}

	return NULL;
}

/*
 * Initializes common fields of the device structure.
 */
void device_init(struct device *dev, const char *devnode,
		 const struct device_ops *ops)
{
	dev->devnode = strdup(devnode);
	dev->serial = NULL;
	dev->refcount = 1;
	dev->ops = ops;
}

/*
 * Frees common fields of the device structure. To be called from the device
 * specific free operation.
 */
void device_fini(struct device *dev)
{
	free(dev->devnode);
	free(dev->serial);
}

/*
 * Checks whether the device matches a given device node.
 */
bool device_match_devnode(struct device *dev, const char *devnode)
{
	return strcmp(dev->devnode, devnode) == 0;
}

/*
 * Wraps the device specific thread worker function for pthreads.
 */
static void *device_start_routine(void *ptr)
{
	struct device *dev = ptr;

	dev->ops->thread(dev);

	return NULL;
}

/*
 * Starts the device and its worker thread.
 */
int device_start(struct device *dev)
{
	int ret;

	if (dev->active)
		return 0;

	ret = dev->ops->start(dev);
	if (ret < 0)
		return ret;

	dev->active = true;

	return pthread_create(&dev->thread, NULL, device_start_routine, dev);
}

/*
 * Stops the device and its worker thread.
 */
void device_stop(struct device *dev)
{
	if (!dev->active)
		return;

	dev->active = false;

	pthread_join(dev->thread, NULL);

	dev->ops->stop(dev);
}
