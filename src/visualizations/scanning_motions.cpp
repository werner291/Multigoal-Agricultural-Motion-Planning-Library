// Copyright (c) 2024 University College Roosevelt
//
// All rights reserved.

//
// Created by werner on 8/28/24.
//

#include <vtkTextActor.h>
#include <fcl/narrowphase/collision_object.h>
#include <fcl/geometry/shape/box.h>
#include "../experiment_utils/default_colors.h"
#include "../experiment_utils/procedural_fruit_placement.h"
#include "../experiment_utils/procedural_robot_models.h"
#include "../planning/ParametricInfiniteCone.h"
#include "../planning/RobotPath.h"
#include "../planning/RobotPathFn.h"
#include "../planning/state_tools.h"
#include "../visualization/VtkLineSegmentVizualization.h"
#include "../visualization/VtkPolyLineVisualization.h"
#include "../visualization/robot_state.h"
#include "../visualization/visualization_function_macros.h"
#include "../visualization/vtk.h"
#include "../experiment_utils/surface_points.h"
#include "../visualization/scannable_points.h"
#include "../visualization/TraceVisualization.h"
#include "../visualization/ui.h"
#include "../planning/collision_detection.h"
#include "../planning/shell_path_planning.h"
#include "../planning/RobotModel.h"
#include "../planning/fcl_utils.h"
#include "../planning/cgal_chull_shortest_paths.h"

using namespace mgodpl;
using namespace mgodpl::vizualisation;

/**
 * Creates a path that orbits around the fruit, keeping the end-effector at a fixed distance from the fruit,
 * and the arm pointing radially inwards towards the fruit. This causes the robot's main body to swing out a lot.
 *
 * @param robot_model		The robot model that the path is for.
 * @param distance			The distance from the center of the fruit to the end-effector.
 * @param fruit_position	The position of the fruit.
 *
 * @return A function that takes a time parameter and returns a RobotState. (Note: captures robot_model, distance,
 * fruit_position by reference)
 */
RobotPathFn whole_body_orbit_path(const robot_model::RobotModel &robot_model,
								  double distance = 0.5,
								  const math::Vec3d &fruit_position = {0.0, 0.0, 0.0}) {
	return [robot_model, distance, fruit_position](double t) {
		const double angle = t * 2.0 * M_PI;
		const math::Vec3d arm_vector = math::Vec3d{cos(angle), sin(angle), 0.0};
		const math::Vec3d offset_vector = fruit_position + arm_vector * distance;
		return fromEndEffectorAndVector(robot_model, offset_vector, arm_vector);
	};
}

RobotPathFn end_effector_orbit_path(const robot_model::RobotModel &robot_model, double distance = 0.5) {
	return [robot_model, distance](double t) {
		const double angle = t * 2.0 * M_PI;
		const math::Vec3d arm_vector = math::Vec3d{1.0, 0.0, 0.0};
		const math::Vec3d offset_vector = math::Vec3d{cos(angle), sin(angle), 0.0} * distance;
		return fromEndEffectorAndVector(robot_model, offset_vector, arm_vector);
	};
}

RobotPathFn end_effector_vertical_path(const robot_model::RobotModel &robot_model, double distance = 0.5) {
	return [robot_model, distance](double t) {
		const double angle = t * 2.0 * M_PI;
		const math::Vec3d arm_vector = math::Vec3d{1.0, 0.0, 0.0};
		const math::Vec3d offset_vector = math::Vec3d{0.0, cos(angle), sin(angle)} * distance;
		return fromEndEffectorAndVector(robot_model, offset_vector, arm_vector);
	};
}

/**
 * A composition of slices of the above two:
 *
 * We follow a
 */
RobotPathFn segmented_orbit_path(const robot_model::RobotModel &robot_model, double distance = 0.5) {
	return concat({whole_body_orbit_path(robot_model, distance), end_effector_orbit_path(robot_model, distance)});
}

REGISTER_VISUALIZATION(parametric_scan_path) {

	// The radius of the sphere representing the fruit.
	const double FRUIT_RADIUS = 0.1;

	// The distance from the surface at which we'd ideally like to scan the fruit.
	const double SCAN_DISTANCE = 0.2;

	double scan_radius = FRUIT_RADIUS + SCAN_DISTANCE;

	const math::Vec3d fruit_position = {0.0, 0.0, 0.0};
	viewer.addSphere(FRUIT_RADIUS, fruit_position, FRUIT_COLOR);

	const robot_model::RobotModel robot_model = experiments::createProceduralRobotModel(
			experiments::RobotArmParameters{.total_arm_length = 1.0, .joint_types = {experiments::HORIZONTAL}});

	const math::Vec3d initial_end_effector_position = {scan_radius, 0.0, 0.0};
	const math::Vec3d initial_arm_vec(1.0, 0.0, 0.0);

	const RobotState initial_state = fromEndEffectorAndVector(robot_model, {scan_radius, 0.0, 0.0}, initial_arm_vec);

	viewer.setCameraTransform(fruit_position + math::Vec3d{5.0, 0.0, 5}, fruit_position);

	auto rb = vizualize_robot_state(viewer, robot_model, forwardKinematics(robot_model, initial_state));

	std::vector<math::Vec3d> cone_points;

	ParametricInfiniteCone cone(-initial_arm_vec, M_PI / 4.0, initial_end_effector_position);

	// Generate points on the cone by iterating through t and theta.
	for (int t_i = 0; t_i < 10; t_i++) {
		const double t = t_i / 10.0;
		for (int theta_i = 0; theta_i < 10; theta_i++) {
			const double theta = theta_i / 10.0 * 2.0 * M_PI;
			cone_points.push_back(cone.position(t, theta));
		}
	}

	VtkPointCloudVisualization points(1, 0, 1);
	points.updatePoints(cone_points);
	viewer.addActor(points.getActor());

	// Something fun: let's make a circle, but through t and theta space:
	std::vector<math::Vec3d> circle_points;
	VtkPolyLineVisualization circle(1, 1, 1);
	viewer.addActor(circle.getActor());

	int path_i = 0;

	viewer.addTimerCallback([&]() {
		int i = circle_points.size();
		double t = static_cast<double>(i) / 100.0;

		switch (path_i) {
			case 0: {
				// Weird curve path
				const double theta = 2.0 * M_PI * t;
				circle_points.push_back(cone.position(sin(theta / 2.0) * scan_radius * 1.5, theta));
			}
				break;
			case 1: {
				// Spider path
				double theta = std::floor(t * 8.0) * 2.0 * M_PI / 8.0;
				double linear = t * 8.0 - std::floor(t * 8.0);
				if (linear > 0.5)
					linear = 1.0 - linear;
				circle_points.push_back(cone.position(linear, theta));
			}
				break;
			default:
				throw std::runtime_error("Invalid path_i");
		}

		circle.updateLine(circle_points);

		if (i == 100) {
			circle_points.clear();
			path_i = (path_i + 1) % 2;
		}
	});


	viewer.start();
}

math::Vec3d arm_vector_from_eepoint(const math::Vec3d &ee_point) {
	const math::Vec3d REFERENCE_SHALLOW_PULL_POINT = {5, 0, 0};
	const math::Vec3d arm_vector = (REFERENCE_SHALLOW_PULL_POINT - ee_point).normalized();
	return arm_vector;
}

RobotPathFn weird_cone_curve_path(const robot_model::RobotModel &robot_model, double distance = 0.5) {
	ParametricInfiniteCone cone({-1.0, 0.0, 0.0}, M_PI / 4.0, {distance, 0.0, 0.0});

	return [robot_model, cone, distance](double t) {
		const double theta = t * 2.0 * M_PI;
		const auto ee_point = cone.position(sin(theta / 2.0) * distance * 1.2, theta);
		return fromEndEffectorAndVector(robot_model, ee_point, arm_vector_from_eepoint(ee_point));
	};
}

RobotPathFn spider_path(const robot_model::RobotModel &robot_model, double distance = 0.5) {
	ParametricInfiniteCone cone({-1.0, 0.0, 0.0}, M_PI / 4.0, {distance, 0.0, 0.0});

	return [robot_model, cone, distance](double t) {
		const double theta = std::floor(t * 8.0) * 2.0 * M_PI / 8.0;
		double linear = t * 8.0 - std::floor(t * 8.0);
		if (linear > 0.5)
			linear = 1.0 - linear;
		const auto ee_point = cone.position(linear, theta);
		return fromEndEffectorAndVector(robot_model, ee_point, arm_vector_from_eepoint(ee_point));
	};
}

math::Vec3d weird_paraboloid_point(double distance, const double theta, double linear) {
	math::Vec3d ee_point = {
			distance - linear * linear, sin(theta) * linear * distance * 4.0, cos(theta) * linear * distance * 4.0};
	return ee_point;
}

RobotPathFn
curved_spider_path(const robot_model::RobotModel &robot_model, double distance = 0.5, unsigned int n_spokes = 8) {
	return [robot_model, distance, n_spokes](double t) {

		double spokes_d = static_cast<double>(n_spokes);

		const double theta = std::floor(t * spokes_d) * 2.0 * M_PI / spokes_d;
		double linear = t * spokes_d - std::floor(t * spokes_d);
		if (linear > 0.5)
			linear = 1.0 - linear;

		math::Vec3d ee_point = weird_paraboloid_point(distance, theta, linear);

		return fromEndEffectorAndVector(robot_model, ee_point, arm_vector_from_eepoint(ee_point));
	};
}


ScannablePoints generate_sphere_scannable_points(const int n_points, random_numbers::RandomNumberGenerator &rng,
												 const math::Vec3d &fruit_position, const double FRUIT_RADIUS) {
	std::vector<SurfacePoint> points;
	// Generate points on the sphere by sampling from a normal distribution 3 times and normalizing:
	for (int i = 0; i < n_points; i++) {
		math::Vec3d point(rng.gaussian01(), rng.gaussian01(), rng.gaussian01());
		point.normalize();
		points.push_back({.position = fruit_position + point * FRUIT_RADIUS, .normal = point});
	}

	// Create the scannable points
	ScannablePoints scannable_points{
			0.5, 0.1, M_PI / 2.0, std::move(points)
	};

	return scannable_points;
}

/**
 * Visualizes a set of different motions whereby the robot, starting from a configuration near a fruit,
 * will make motions to scan the fruit from different angles using its end-effector.
 */
REGISTER_VISUALIZATION(scanning_motions_straight_arm) {

	auto stats_output = visualization::add_text_label(viewer, "Straight arm", 10, 10);

	// The radius of the sphere representing the fruit.
	const double FRUIT_RADIUS = 0.1;

	// The distance from the surface at which we'd ideally like to scan the fruit.
	const double SCAN_DISTANCE = 0.1;

	double scan_radius = FRUIT_RADIUS + SCAN_DISTANCE;

	const math::Vec3d fruit_position = {0.0, 0.0, 0.0};
	viewer.addSphere(FRUIT_RADIUS, fruit_position, FRUIT_COLOR);

	// Generate points on the sphere surface:
	random_numbers::RandomNumberGenerator rng;

	const auto &scannable_points = generate_sphere_scannable_points(500, rng, fruit_position, FRUIT_RADIUS);

	// Initialize all points as unseen
	SeenPoints ever_seen = SeenPoints::create_all_unseen(scannable_points);

	// Create the fruit points visualization
	auto fruit_points_visualization = visualize(viewer, scannable_points, ever_seen);

	const robot_model::RobotModel robot_model = experiments::createProceduralRobotModel(
			experiments::RobotArmParameters{.total_arm_length = 1.0, .joint_types = {
					experiments::HORIZONTAL}, .add_spherical_wrist=false});

	const RobotState initial_state = fromEndEffectorAndVector(robot_model, {scan_radius, 0.0, 0.0}, {1.0, 0.0, 0.0});

	viewer.setCameraTransform(fruit_position + math::Vec3d{1.0, 4.0, 3}, fruit_position);

	auto rb = vizualize_robot_state(viewer, robot_model, forwardKinematics(robot_model, initial_state));

	const std::vector<std::tuple<RobotPathFn, double, std::string>> paths = {
			{whole_body_orbit_path(robot_model, scan_radius),      0.05,  "whole_body_orbit_path"},
			{end_effector_orbit_path(robot_model, scan_radius),    0.05,  "end_effector_orbit_path"},
			{end_effector_vertical_path(robot_model, scan_radius), 0.025, "end_effector_vertical_path"},
			{weird_cone_curve_path(robot_model, scan_radius),      0.025, "weird_cone_curve_path"},
			{spider_path(robot_model, scan_radius),                0.01,  "spider_path"},
			{curved_spider_path(robot_model, scan_radius),         0.01,  "curved_spider_path"}
	};

	double t = 0.0;

	VtkLineSegmentsVisualization to_surface(1, 0, 1);
	viewer.addActor(to_surface.getActor());

	mgodpl::visualization::TraceVisualisation trace_visualisation(viewer, {1, 0, 1});

	std::vector<std::pair<math::Vec3d, math::Vec3d>> lines;

	double robot_distance = 0.0;
	size_t current_path = 0;
	RobotState last_state = get<0>(paths[current_path])(0.0);
	stats_output->SetInput(get<2>(paths[current_path]).c_str());

	RobotPath path = RobotPath::singleton(initial_state);

	viewer.addTimerCallback([&]() {

		t += get<1>(paths[current_path]);

		RobotState new_state = get<0>(paths[current_path])(t);

		// Add the distance:
		robot_distance += equal_weights_distance(last_state, new_state);
		last_state = new_state;

		if (t > 1.0) {

			std::cout << "Path " << get<2>(paths[current_path]) << " scan stats: " << ever_seen.count_seen() << "/"
					  << scannable_points.surface_points.size() << ", distance: " << robot_distance << std::endl;

			t = 0.0;
			current_path += 1;
			if (current_path >= paths.size()) {
				current_path = 0;
				if (viewer.isRecording()) {
					viewer.stop();
				}
			}

			lines.clear();
			trace_visualisation.clear();
			last_state = get<0>(paths[current_path])(t);
			ever_seen = SeenPoints::create_all_unseen(scannable_points);
			robot_distance = 0.0;
			stats_output->SetInput(get<2>(paths[current_path]).c_str());
		}

		auto fk = forwardKinematics(robot_model, new_state);
		update_robot_state(robot_model, fk, rb);

		const math::Vec3d ee_pos = fk.forLink(robot_model.findLinkByName("end_effector")).translation;

		// Add the ee-to-fruit line to the visualization.
		lines.emplace_back(ee_pos, fruit_position);
		to_surface.updateLine(lines);

		// Add the ee position to the trace visualization.
		trace_visualisation.add_point(ee_pos);

		// Update seen/unseen points
		update_visibility(scannable_points, ee_pos, ever_seen);

		// Set the colors of the fruit points visualization
		update_visualization(ever_seen, fruit_points_visualization);
	});

	viewer.start();
}

/**
 * \brief Creates a cube collision object.
 *
 * This function creates a cube collision object with the specified position and size.
 *
 * \param position The position of the cube's center.
 * \param size The size of the cube (length of each side).
 * \return A collision object representing the cube.
 */
fcl::CollisionObjectd createCubeCollisionObject(const math::Vec3d &position, const math::Vec3d &size) {
	auto box = std::make_shared<fcl::Boxd>(size.x(), size.y(), size.z());
	fcl::Transform3d transform = fcl::Transform3d::Identity();
	transform.translation() = fcl::Vector3d(position.x(), position.y(), position.z());
	return {box, transform};
}


/**
 * Maximum-y-cycle.
 *
 * @param x_range The maximum x value.
 * @param y_range The maximum y value.
 * @param can_move  A function that takes two Vec2i and tells us if we can move from the first to the second.
 *
 * (0,0) is assumed to be a valid point.
 */
std::vector<std::array<int, 2>>
maximum_y_cycle(int x_range, int y_range, std::function<bool(std::array<int, 2>, std::array<int, 2>)> can_move) {

	// The furthest one can move on each spoke in the y direction.
	std::vector<int> max_y(x_range, 0);

	// Find the maximum y for each x:
	for (int x = 0; x < x_range; x++) {
		int y = 0;
		while (y < y_range && can_move({x, y}, {x, y + 1})) {
			y++;
		}
		max_y[x] = y;
	}

	// The furthest y for each x where we can move to the right; at most the maximum y.
	std::vector<int> shift_y(x_range, 0);

	// find the maximum y for each x where we can move to the right.
	for (int x = 0; x < x_range; x++) {
		int y = std::min(max_y[x], max_y[(x + 1) % x_range]);
		while (y > 0 && !can_move({x, y}, {(x + 1) % x_range, y})) {
			y--;
		}
		// Record the shift y:
		shift_y[x] = y;
	}

	// Now, build the path:
	std::vector<std::array<int, 2>> path;

	// Starts at 0,0:
	path.push_back({0, 0});

	// Run through each x:
	for (int x = 0; x < x_range; x++) {

		// Push points from current y to max y:
		for (int y = path.back()[1]; y < max_y[x]; y++) {
			path.push_back({x, y});
		}

		// Then down to the shift y:
		for (int y = max_y[x]; y > shift_y[x]; y--) {
			path.push_back({x, y});
		}

		// Push with increased x at the shift y:
		path.push_back({(x + 1) % x_range, shift_y[x]});
	}

	// Then push points back down to 0,0:
	for (int y = shift_y[0]; y >= 0; y--) {
		path.push_back({0, y});
	}

	return path;
}

#include <vtkProperty.h>

/**
 * A function that defines a parametric paraboloid.
 *
 * It is a paraboloid produced by the revolution of a parabola around the y-axis.
 *
 * The apex is at (0,scan_radius,0), and it opens in the negative y-direction,
 * containing the ring around the z-axis with radius scan_radius at y = 0.
 *
 * @param scan_radius	The radius of the paraboloid at y = 0.
 * @param u				The u parameter, between 0 and 1, defining the angle around the y-axis.
 * @param v				The v parameter, between 0 and +infinity, defining the height of the paraboloid.
 *
 * @return The point on the paraboloid.
 */
math::Vec3d paraboloid_point(double scan_radius, double u, double v) {
	return { sin(2.0 * M_PI * u) * v * scan_radius,
			 scan_radius * (1.0 - v * v),
			 cos(2.0 * M_PI * u) * v * scan_radius};
}

/**
 * @fn RobotPath createObstacleAvoidingPath(const robot_model::RobotModel &robot_model, const fcl::CollisionObjectd &obstacle, double scan_radius, int MAX_X, int MAX_Y)
 * @brief Creates a path for the robot that avoids a given obstacle.
 * @param robot_model The robot model.
 * @param obstacle The obstacle to avoid.
 * @param scan_radius The radius of the scan.
 * @param MAX_X The maximum x value for the path.
 * @param MAX_Y The maximum y value for the path.
 * @return A RobotPath that avoids the obstacle.
 */
RobotPath createObstacleAvoidingPath(const robot_model::RobotModel &robot_model,
									 const fcl::CollisionObjectd &obstacle,
									 double scan_radius,
									 int MAX_X,
									 int MAX_Y,
									 math::Vec3d fruit_position,
									 math::Vec3d initial_arm_vector) {

	auto calculate_position = [](const std::array<int, 2> &a, double scan_radius, int MAX_X, int MAX_Y) {
		return paraboloid_point(scan_radius, a[0] / static_cast<double>(MAX_X), a[1] / static_cast<double>(MAX_Y));
    };


	// Compute a basis that'll transform the paraboloid such that the y-axis aligns with the arm vector:
	const math::Vec3d y = initial_arm_vector; // Inverted such that the paraboloid faces away from the robot.
	const math::Vec3d x = initial_arm_vector.cross({0.0, 0.0, 1.0}).normalized();
	const math::Vec3d z = x.cross(y).normalized();

	// Create a path that avoids the obstacle:
    auto path = maximum_y_cycle(MAX_X, MAX_Y, [&](std::array<int, 2> a, std::array<int, 2> b) {
        math::Vec3d a_pos = calculate_position(a, scan_radius, MAX_X, MAX_Y);
		math::Vec3d a_arm = initial_arm_vector;// arm_vector_from_eepoint(a_pos);
        math::Vec3d b_pos = calculate_position(b, scan_radius, MAX_X, MAX_Y);
		math::Vec3d b_arm = initial_arm_vector;// arm_vector_from_eepoint(b_pos);

		// transform them:
		a_pos = x * a_pos.x() + y * a_pos.y() + z * a_pos.z() + fruit_position;
		b_pos = x * b_pos.x() + y * b_pos.y() + z * b_pos.z() + fruit_position;
		a_arm = x * a_arm.x() + y * a_arm.y() + z * a_arm.z();
		b_arm = x * b_arm.x() + y * b_arm.y() + z * b_arm.z();

        return !check_motion_collides(robot_model, obstacle,
                                      fromEndEffectorAndVector(robot_model, a_pos, a_arm),
                                      fromEndEffectorAndVector(robot_model, b_pos, b_arm));
    });


    // Turn it into a RobotPath:
    RobotPath path_robot;
    for (const auto &point: path) {
        math::Vec3d ee_pos = calculate_position(point, scan_radius, MAX_X, MAX_Y);
		math::Vec3d arm_vector = initial_arm_vector;//arm_vector_from_eepoint(ee_pos);

		ee_pos = x * ee_pos.x() + y * ee_pos.y() + z * ee_pos.z() + fruit_position;
		arm_vector = x * arm_vector.x() + y * arm_vector.y() + z * arm_vector.z();

		path_robot.append(fromEndEffectorAndVector(robot_model, ee_pos, arm_vector));
	}

	return path_robot;
}

REGISTER_VISUALIZATION(scanning_motions_obstacle_avoidance) {

	// The radius of the sphere representing the fruit.
	const double FRUIT_RADIUS = 0.1;

	// The distance from the surface at which we'd ideally like to scan the fruit.
	const double SCAN_DISTANCE = 0.1;

	double scan_radius = FRUIT_RADIUS + SCAN_DISTANCE;

	const math::Vec3d fruit_position = {0.0, 0.0, 0.0};
	viewer.addSphere(FRUIT_RADIUS, fruit_position, FRUIT_COLOR);

	// Generate points on the sphere surface:
	random_numbers::RandomNumberGenerator rng;

	const auto &scannable_points = generate_sphere_scannable_points(500, rng, fruit_position, FRUIT_RADIUS);

	// Initialize all points as unseen
	SeenPoints ever_seen = SeenPoints::create_all_unseen(scannable_points);

	// Create the fruit points visualization
	auto fruit_points_visualization = visualize(viewer, scannable_points, ever_seen);

	const robot_model::RobotModel robot_model = experiments::createProceduralRobotModel(
			experiments::RobotArmParameters{.total_arm_length = 1.0, .joint_types = {
					experiments::HORIZONTAL}, .add_spherical_wrist=false});

	const RobotState initial_state = fromEndEffectorAndVector(robot_model, {scan_radius, 0.0, 0.0}, {1.0, 0.0, 0.0});

	viewer.setCameraTransform(fruit_position + math::Vec3d{1.0, 4.0, 3}, fruit_position);

	auto rb = vizualize_robot_state(viewer,
									robot_model,
									forwardKinematics(robot_model, initial_state),
									{0.8, 0.8, 0.8},
									true);

	// Create an fcl collision object: a cube above the fruit to simulate an obstacle.
	math::Vec3d obstacle_position(0.0, 0.0, 0.3);
	math::Vec3d obstacle_size(0.3, 0.3, 0.3);
	fcl::CollisionObjectd obstacle = createCubeCollisionObject(obstacle_position, obstacle_size);

	// Visualize a cube at that position:
	viewer.addBox(obstacle_size, obstacle_position, WOOD_COLOR);

	RobotPath path_robot = createObstacleAvoidingPath(robot_model, obstacle, scan_radius, 100, 100, fruit_position, {1.0, 0.0, 0.0});

	mgodpl::visualization::TraceVisualisation trace_visualisation(viewer, {1, 0, 1});

	PathPoint path_point{0, 0.0};

	viewer.addTimerCallback([&]() {

		// Advance the robot state by a small amount of time.
		if (advancePathPointWrap(path_robot, path_point, 0.05, equal_weights_distance)) {

			std::cout << "Scanned " << ever_seen.count_seen() << " out of " << scannable_points.surface_points.size()
					  << " points." << std::endl;

			if (viewer.isRecording()) {
				viewer.stop();
			} else {
				ever_seen = SeenPoints::create_all_unseen(scannable_points);
				trace_visualisation.clear();
			}
		}

		// Get the new state:
		RobotState new_state = interpolate(path_point, path_robot);

		// FK:
		auto fk = forwardKinematics(robot_model, new_state);

		// Check if collides:
		bool collides = check_robot_collision(robot_model, obstacle, new_state);

		if (collides) {
			rb.actors[0]->GetProperty()->SetColor(1.0, 0.0, 0.0);
		} else {
			rb.actors[0]->GetProperty()->SetColor(0.8, 0.8, 0.8);
		}

		update_robot_state(robot_model, fk, rb);

		const math::Vec3d ee_pos = fk.forLink(robot_model.findLinkByName("end_effector")).translation;
		trace_visualisation.add_point(ee_pos);

		update_visibility(scannable_points, ee_pos, ever_seen);

		// Set the colors of the fruit points visualization
		update_visualization(ever_seen, fruit_points_visualization);

	});

	viewer.lockCameraUp();
	viewer.setCameraTransform({2.0, 2.0, -2.0}, fruit_position);

	viewer.start();
}

REGISTER_VISUALIZATION(scanning_motions_for_each_fruit) {

	const double SCAN_RADIUS = 0.2;

	// Load the tree meshes
	auto tree_model = tree_meshes::loadTreeMeshes("appletree");

	// Initialize a random number generator
	random_numbers::RandomNumberGenerator rng(42);

	viewer.addTree(tree_model, false, true);

	robot_model::RobotModel robot_model = experiments::createProceduralRobotModel(
			experiments::RobotArmParameters{.total_arm_length = 0.75, .joint_types = {experiments::HORIZONTAL}, .add_spherical_wrist=false});

	RobotState initial_state = fromEndEffectorAndVector(robot_model, {5.0, 0.0, 5.0}, {1.0, 0.0, 0.0});

	// CGAl mesh data for the tree model:
	cgal::CgalMeshData chull_shell(tree_model.leaves_mesh);

	// Allocate a BVH convex_hull for the tree trunk.
	auto tree_collision = mgodpl::fcl_utils::treeMeshesToFclCollisionObject(tree_model);

	const auto& target_points = computeFruitPositions(tree_model);

	RobotPath path = mgodpl::shell_path_planning::plan_multigoal_path(
		initial_state,
		tree_collision,
		chull_shell,
		target_points,
		SCAN_RADIUS,
		robot_model,
		rng,
		{}, //Default planning methods
		[](const math::Vec3d target_point, const RobotState &state){
			return RobotPath::singleton(state);
		}
		);

	// Create an end-effector visualization:
	mgodpl::visualization::TraceVisualisation trace_visualisation(viewer, {1, 0, 1}, 1000);

	// Visualize the state:
	auto robot_visual = vizualize_robot_state(viewer, robot_model, forwardKinematics(robot_model, initial_state));

	static PathPoint path_point{0, 0.0};
	viewer.addTimerCallback([&]() {
		if (advancePathPointWrap(path, path_point, 0.05, equal_weights_distance)) {
			if (viewer.isRecording()) {
				viewer.stop();
			}
		}
		auto fk = forwardKinematics(robot_model, interpolate(path_point, path));
		trace_visualisation.add_point(fk.forLink(robot_model.findLinkByName("end_effector")).translation);
		update_robot_state(robot_model, fk, robot_visual);
	});

	viewer.start();

}