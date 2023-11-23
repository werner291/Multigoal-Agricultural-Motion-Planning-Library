// Copyright (c) 2022 University College Roosevelt
//
// All rights reserved.

//
// Created by werner on 11/23/23.
//

#include "RobotModel.h"

namespace mgodpl::robot_model {

	ForwardKinematicsResult forwardKinematics(const RobotModel &model,
											  const std::vector<double> &joint_values,
											  const RobotModel::LinkId &root_link,
											  const math::Transformd &root_link_transform) {

		// Allocate a result with all identity transforms.
		ForwardKinematicsResult result{
				.link_transforms = std::vector<math::Transformd>(model.links.size(), root_link_transform)
		};

		// Keep a vector to track which links have been visited, since a robot may contain cycles.
		std::vector<bool> visited(model.links.size(), false);

		// Keep a stack of links to visit, and push the root link.
		// Every entry in the stack is a pair of a link and its transform relative to the root_link.
		std::vector<std::pair<RobotModel::LinkId, math::Transformd>> stack{
				{root_link, math::Transformd::identity()}
		};

		// Keep track of the index in the joint_values vector; this induces the DFS order.
		size_t variable_index = 0;

		while (!stack.empty()) {

			// Pop the top of the stack.
			auto [link, transform] = stack.back();
			stack.pop_back();

			// If we have already visited this link, skip it.
			if (visited[link])
				continue;

			// Mark the link as visited.
			visited[link] = true;

			// Update the transform for this link.
			result.link_transforms[link] = transform;

			// For all children, if not already visited, compute their transform and push them on the stack.
			for (const auto &joint: model.links[link].joints) {

				// If the other link is already visited, skip it.
				if (visited[model.joints[joint].linkB])
					continue;

				// Compute the transform from the link to the joint.
				math::Transformd local_to_joint = model.joints[joint].linkA == link ? model.joints[joint].attachmentA
																					: model.joints[joint].attachmentB;

				// Compute the transform induced by the variable part of the joint.
				math::Transformd joint_transform = RobotModel::variableTransform(model.joints[joint].type_specific,
																				 joint_values.data() + variable_index);
				if (model.joints[joint].linkB == link) joint_transform = joint_transform.inverse();
				variable_index += RobotModel::n_variables(model.joints[joint].type_specific);

				// Compute the transform from the joint to the link.
				math::Transformd after_joint_frame = (model.joints[joint].linkA == link
													  ? model.joints[joint].attachmentB
													  : model.joints[joint].attachmentA).inverse();

				// Compute the transform from the link to the next link.
				math::Transformd next_link_transform =
						transform
								.then(local_to_joint)
								.then(joint_transform)
								.then(after_joint_frame);

				// Push the next link on the stack.
				stack.emplace_back(
						model.joints[joint].linkA == link ? model.joints[joint].linkB : model.joints[joint].linkA,
						next_link_transform);
			}
		}

		// Return the result.
		return result;

	}

	RobotModel::LinkId RobotModel::insertLink(const RobotModel::Link &link) {
		// Just push the link onto the vector, and return the index.
		links.push_back(link);
		return links.size() - 1;
	}

	RobotModel::JointId RobotModel::insertJoint(const RobotModel::Joint &joint) {
		// Just push the joint onto the vector, and return the index.
		joints.push_back(joint);
		return joints.size() - 1;
	}

	RobotModel::LinkId RobotModel::findLinkByName(const std::string &name) const {
		// Linear search; throw an exception if not found.
		for (size_t i = 0; i < links.size(); i++)
			if (links[i].name == name)
				return i;
		throw std::runtime_error("Link not found: " + name);
	}

	RobotModel::JointId RobotModel::findJointByName(const std::string &name) const {
		// Linear search; throw an exception if not found.
		for (size_t i = 0; i < joints.size(); i++)
			if (joints[i].name == name)
				return i;
		throw std::runtime_error("Joint not found: " + name);
	}


	mgodpl::math::Transformd
	mgodpl::robot_model::RobotModel::variableTransform(const RobotModel::JointTypeSpecific &variablePart,
													   const double *joint_values) {
		// Switch on the type of joint.
		switch (variablePart.index()) {
			case 0: {
				// Construct a rotation transform from the axis and the joint value.
				const auto &revolute_joint = std::get<RevoluteJoint>(variablePart);
				return math::Transformd::fromRotation(math::Quaterniond::fromAxisAngle(revolute_joint.axis,
																					   joint_values[0]));
			}
			case 1: {
				// Fixed joint, return identity.
				return math::Transformd::identity();
			}
			default:
				throw std::runtime_error("Unknown joint type");
		}
	}

}