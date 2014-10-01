#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <stdbool.h>
#include <pthread.h>

struct device_ops;

struct device {
	char *devnode;
	char *serial;
	int refcount;
	bool active;
	pthread_t thread;
	const struct device_ops *ops;
};

struct device_ops {
	int (*start)(struct device *dev);
	void (*thread)(struct device *dev);
	void (*stop)(struct device *dev);
	void (*free)(struct device *dev);
};

struct device *device_unref(struct device *dev);
bool device_match_devnode(struct device *dev, const char *devnode);
void device_init(struct device *dev, const char *devnode,
		 const struct device_ops *ops);
void device_fini(struct device *dev);

int device_start(struct device *dev);
void device_stop(struct device *dev);

#endif /* __DEVICE_H__ */
