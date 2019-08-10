/*
 * Pose estimation using OpenCV
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier: (LGPL-2.1-or-later OR BSL-1.0)
 */
#include <opencv2/calib3d/calib3d.hpp>
#if CV_MAJOR_VERSION >= 4
#include <opencv2/calib3d/calib3d_c.h>
#endif

extern "C" {
#include <stdio.h>

#include "blobwatch.h"
#include "leds.h"
#include "maths.h"
}


extern "C" void estimate_initial_pose(struct blob *blobs, int num_blobs,
				      vec3 *leds, int num_pos,
				      dmat3 *camera_matrix, double *dist_coeffs,
				      dquat &rot, dvec3 &trans,
				      bool use_extrinsic_guess)
{
	int i, j;
	int num_leds = 0;
	uint64_t taken = 0;
	int flags = CV_ITERATIVE;
	cv::Mat inliers;
	int iterationsCount = 50;
	float reprojectionError = 1.0;
	float confidence = 0.95;
	cv::Mat A = cv::Mat(3, 3, CV_64FC1, camera_matrix->m);
	cv::Mat distCoeffs = cv::Mat(5, 1, CV_64FC1, dist_coeffs);
	cv::Mat rvec = cv::Mat::zeros(3, 1, CV_64FC1);
	cv::Mat tvec = cv::Mat(3, 1, CV_64FC1, (double *)&trans);
	cv::Mat R = cv::Mat(3, 3, CV_64FC1, (double *)&rot);
	cv::Rodrigues(R, rvec);

	/* count identified leds */
	for (i = 0; i < num_blobs; i++) {
		if (blobs[i].led_id < 0 || blobs[i].led_id >= num_pos)
			continue;
		if (taken & (1ULL << blobs[i].led_id))
			continue;
		taken |= (1ULL << blobs[i].led_id);
		num_leds++;
	}

	if (num_leds < 4)
		return;

	std::vector<cv::Point3f> list_points3d(num_leds);
	std::vector<cv::Point2f> list_points2d(num_leds);

	taken = 0;
	for (i = 0, j = 0; i < num_blobs && j < num_leds; i++) {
		if (blobs[i].led_id < 0)
			continue;
		if (taken & (1ULL << blobs[i].led_id))
			continue;
		taken |= (1ULL << blobs[i].led_id);
		list_points3d[j].x = leds[blobs[i].led_id].x;
		list_points3d[j].y = leds[blobs[i].led_id].y;
		list_points3d[j].z = leds[blobs[i].led_id].z;
		list_points2d[j].x = blobs[i].x;
		list_points2d[j].y = blobs[i].y;
		j++;
	}

	cv::solvePnPRansac(list_points3d, list_points2d, A, distCoeffs, rvec, tvec,
			   use_extrinsic_guess, iterationsCount, reprojectionError,
			   confidence, inliers, flags);

	dvec3 v;
	double angle = sqrt(rvec.dot(rvec));
	double inorm = 1.0f / angle;

	v.x = rvec.at<double>(0) * inorm;
	v.y = rvec.at<double>(1) * inorm;
	v.z = rvec.at<double>(2) * inorm;
	dquat_from_axis_angle(&rot, &v, angle);
}
