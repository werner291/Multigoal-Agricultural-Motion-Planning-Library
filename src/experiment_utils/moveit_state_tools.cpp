// Copyright (c) 2022 University College Roosevelt
//
// All rights reserved.

//
// Created by werner on 11/1/23.
//

#include "moveit_state_tools.h"
#include "../math/Vec3.h"

#include <moveit/robot_state/robot_state.h>

#include <random>
#include <Eigen/Core>

namespace mgodpl::experiments {

	moveit::core::RobotState randomStateOutsideTree(const moveit::core::RobotModelConstPtr &drone, const int seed) {

		moveit::core::RobotState start_state(drone);

		randomizeUprightWithBase(start_state, 0.0);

		// Get the Rng
		random_numbers::RandomNumberGenerator rng(seed);

		double random_t = rng.uniformReal(-M_PI, M_PI);
		double height = rng.uniformReal(0.5, 2.0);

		double radius = 4.0;

		start_state.setVariablePosition(0, cos(random_t) * radius);
		start_state.setVariablePosition(1, sin(random_t) * radius);
		start_state.setVariablePosition(2, height);

		start_state.update(true);

		return start_state;
	}

	void moveEndEffectorToGoal(moveit::core::RobotState &state, double tolerance, const math::Vec3d &target) {

		// Get the Rng
		random_numbers::RandomNumberGenerator rng;

		// Sample a distance from the target to the end-effector uniformly between 0 (included) and tolerance (excluded)
		double sample_radius = tolerance * rng.uniformReal(0.0, 1.0 - std::numeric_limits<double>::epsilon());

		// Get the end-effector position (through forward kinematics)
		Eigen::Vector3d ee_pos = state.getGlobalLinkTransform("end_effector").translation();

		// Get the vector from the end-effector to the target
		Eigen::Vector3d delta = Eigen::Vector3d(target.components.data()) - ee_pos;

		// Compute the norm
		double norm = delta.norm();

		// If the end-effector is not within the tolerance of the target...
		if (norm > sample_radius) {
			// ...resize the delta vector
			delta *= ((norm - sample_radius) / norm);

			// ...and add it to the translational componnet of the flying base.
			double *positions = state.getVariablePositions();
			positions[0] += delta.x();
			positions[1] += delta.y();
			positions[2] += delta.z();

			// Finally, force-update the state. (Since we wrote to its memory it cannot know that it has changed)
			state.update(true);
		}
	}

	void randomizeUprightWithBase(moveit::core::RobotState &state, double translation_bound) {

		// Set the state to uniformly random values.
		// Unfortunately, this puts the base at the origin and the rotation will not be upright. We need to fix that.
		state.setToRandomPositions();

		// Get an Rng
		random_numbers::RandomNumberGenerator rng;

		// Randomize the floating base within a box defined by the translation_bound.
		double *pos = state.getVariablePositions();
		pos[0] = rng.uniformReal(-translation_bound, translation_bound);
		pos[1] = rng.uniformReal(-translation_bound, translation_bound);
		pos[2] = rng.uniformReal(0 /* Do not put it underground */, translation_bound);

		// Compute a random yaw-rotation, and assign it to the quaternion of the floating base.
		Eigen::Quaterniond q(Eigen::AngleAxisd(rng.uniformReal(-M_PI, M_PI), Eigen::Vector3d::UnitZ()));
		pos[3] = q.x();
		pos[4] = q.y();
		pos[5] = q.z();
		pos[6] = q.w();

		// Force-update the state. (Since we wrote to its memory it cannot know that it has changed)
		state.update(true);
	}
}