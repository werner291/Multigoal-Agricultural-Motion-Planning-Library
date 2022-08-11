#ifndef NEW_PLANNERS_CONVEXHULLSHELL_H
#define NEW_PLANNERS_CONVEXHULLSHELL_H

#include "SphereShell.h"
#include "../planners/ShellPathPlanner.h"
#include "../utilities/math_utils.h"
#include <ompl/datastructures/NearestNeighborsGNAT.h>
#include <range/v3/algorithm/min_element.hpp>
#include <range/v3/view/iota.hpp>

/**
 * A reference to some location on a ConvexHullShell.
 */
struct ConvexHullPoint {
	/// The facet identifier of the facet containing this point.
	size_t face_id;
	/// The position of this point in 3D Euclidean space.
	Eigen::Vector3d position;
};

/**
 * A ShellBuilder that builds a ConvexHullShell around the leaf vertices in a tree.
 */
class ConvexHullShellBuilder : public ShellPathPlanner<ConvexHullPoint>::ShellBuilder {

public:
	ConvexHullShellBuilder() = default;

	[[nodiscard]] std::shared_ptr<OMPLSphereShellWrapper<ConvexHullPoint>>
	buildShell(const AppleTreePlanningScene &scene_info, const ompl::base::SpaceInformationPtr &si) const override;

	[[nodiscard]] Json::Value parameters() const override;
};

class ConvexHullShell : public CollisionFreeShell<ConvexHullPoint> {



	/// A set of all the vertices in the convex hull.
	std::vector<Eigen::Vector3d> vertices;

	/// A padding distance to add to shell states.
	double padding = 0.1;

	/// The Facet structure that contains references to the vertices and neighbouring facets.
	struct Facet {
		/// The vertices that make up this facet.
		size_t a, b, c;
		/// The neighbouring facets, by the edge that connects to them.
		size_t neighbour_ab, neighbour_bc, neighbour_ca;

		[[nodiscard]] std::array<size_t, 3> neighbours() const;

		[[nodiscard]] size_t neighbour(TriangleEdgeId edge_id) const;

		[[nodiscard]] size_t vertex(TriangleVertexId vertex_id) const;

		[[nodiscard]] std::array<size_t, 2> edge_vertices(TriangleEdgeId edge_id) const;
	};

	/// The facets that make up the convex hull.
	std::vector<Facet> facets;

	/// The entry type for the nearest-neighbour structure.
	struct NNGNATEntry {
		bool operator==(const NNGNATEntry &rhs) const;

		bool operator!=(const NNGNATEntry &rhs) const;

		size_t face_index;
		Eigen::Vector3d at;

	};

	/// A GNAT that gives a indexes all facets by their center of gravity.
	ompl::NearestNeighborsGNAT<NNGNATEntry> facet_index;

	/**
	 * Find the facet whose center of gravity is closest to the given point,
	 * used as an initial guess for locating the facet closest to the point.
	 *
	 * This operation takes O(log n) time in the number of facets.
	 *
	 * @param a 	The point to find the facet closest to.
	 * @return 		The index of the facet whose COG is closest to the point.
	 */
	size_t guess_closest_face(const Eigen::Vector3d &a) const;

public:

	/**
	 * Construct a ConvexHullShell from a convex hull mesh.
	 *
	 * Warning: The mesh given is assumed to already be convex. If it is not,
	 * you may build one using the convexHull function.
	 *
	 * @param mesh 		The mesh to use. (Note: assumed to be convex!)
	 */
	ConvexHullShell(const shape_msgs::msg::Mesh &mesh);

	/**
	 * Construct a RobotState whose end effector is at the given point, plus a padding distance.
	 *
	 * @param drone 	The robot model to use.
	 * @param a 		The point to use.
	 * @return 			The constructed RobotState.
	 */
	[[nodiscard]] moveit::core::RobotState
	state_on_shell(const moveit::core::RobotModelConstPtr &drone, const ConvexHullPoint &a) const override;

	/**
	 * Plan a path of RobotStates from the given start to the given goal.
	 *
	 * Linear interpolation between the states in the returned vector are guaranteed to have the robot's end-effector
	 * on the convex hull at all times (+ padding), and the path is guaranteed to be collision-free.
	 *
	 * @param drone 	The robot model to use.
	 * @param start 	The start point.
	 * @param goal 		The goal point.
	 *
	 * @return 			A vector of RobotStates, which serve as anchor points for linear interpolation.
	 */
	[[nodiscard]] std::vector<moveit::core::RobotState> path_on_shell(const moveit::core::RobotModelConstPtr &drone,
																	  const ConvexHullPoint &a,
																	  const ConvexHullPoint &b) const override;

	/**
	 * Predict the length of a path from the given start to the given goal by generating and measuring the length
	 * of a segmented line across the surface without constructing all the robot states.
	 *
	 * @param a 	The start point.
	 * @param b 	The goal point.
	 * @return 		The predicted length of the path.
	 */
	[[nodiscard]] double predict_path_length(const ConvexHullPoint &a, const ConvexHullPoint &b) const override;

	/**
	 * Generate a ConvexHullPoint in a (roughly) gaussian distribution around another point.
	 *
	 * @param near 	The point to generate around.
	 * @return 		The generated point.
	 */
	ConvexHullPoint gaussian_sample_near_point(const ConvexHullPoint &near) const override;

	/**
	 * Find the ConvexHullPoint of the projection of the end-effector of the robot onto the convex hull.
	 *
	 * @param st 		The RobotState to use.
	 * @return 			The projected point.
	 */
	ConvexHullPoint project(const moveit::core::RobotState &st) const override;

	/**
	 * Find the ConvexHullPoint of the projection of an apple onto the convex hull.
	 *
	 * @param st 		The RobotState to use.
	 * @return 			The projected point.
	 */
	ConvexHullPoint project(const Apple &st) const override;

	/**
	 * The number of facets in the convex hull.
	 *
	 * Valid facet indices will be in the range [0, num_facets).
	 *
	 * @return 		The number of facets.
	 */
	size_t num_facets() const;

	/**
	 * Get the facet structure for a given index.
	 *
	 * @param i 	The index of the facet (0 <= i < num_facets()).
	 * @return 		The facet structure.`
	 */
	const Facet& facet(size_t i) const;

	/**
	 * Get the vertices (in Euclidean space) for the given edge of the given face ID.
	 *
	 * @param face_i	The face ID.
	 * @param edge_id 	The edge ID.
	 * @return 			The vertices.
	 */
	std::array<Eigen::Vector3d, 2> facet_edge_vertices(size_t face_i, TriangleEdgeId edge_id) const;

	/**
	 * Compute the outward-pointing normal of a facet. Normalization is applied.
	 *
	 * @param i 	The index of the facet (0 <= i < num_facets()).
	 * @return 		The normal (unit length).
	 */
	Eigen::Vector3d facet_normal(size_t i) const;

	/**
	 * Look up a vertex of the convex hull.
	 *
	 * @param i 	The index of the vertex (0 <= i < num_vertices()).
	 * @return 		The vertex.
	 */
	const Eigen::Vector3d& vertex(size_t i) const;

	/**
	 * Look up the three vertices of a facet (in Euclidean space).
	 *
	 * @param facet_i 	The index of the facet (0 <= facet_i < num_facets()).
	 * @return 			The three vertices.
	 */
	std::array<Eigen::Vector3d, 3> facet_vertices(size_t facet_i) const;

	/**
	 * Look up the signed distance of a point to the convex hull.
	 *
	 * A distance >0 indicates the point is outside the convex hull, <0 indicates it is inside.
	 *
	 * Note: this operation is O(n) in the number of facets.
	 *
	 * @param pt 	The point to check.
	 * @return 		The signed distance.
	 */
	double signed_distance(const Eigen::Vector3d &pt) const;

	/**
	 * Generate a path of ConversHullPoints from the given start to the given goal across the surface of the convex hull.
	 *
	 * At every edge traversal, a point will be generated for the facet being exited, and a point for the facet being entered,
	 * so note that the returned path will contain duplicate points in Euclidean terms.
	 *
	 * @param a	The start point.
	 * @param b	The goal point.
	 * @return	A vector of ConvexHullPoints.
	 */
	std::vector<ConvexHullPoint> convex_hull_walk(const ConvexHullPoint& a, const ConvexHullPoint& b) const;

protected:

	/**
	 * Project a point onto the convex hull, returning a ConvexHullPoint (that includes the facet index).
	 *
	 * @param a 	The point to project.
	 * @return 		The projected point.
	 */
	[[nodiscard]] ConvexHullPoint project(const Eigen::Vector3d &a) const;

	/**
	 * Internal: populate the neighbour_ab, neighbour_bc, neighbour_ca fields of each facet.
	 */
	void match_faces();

	/**
	 * Internal: Build the facet lookup structure.
	 */
	void init_gnat();

	/**
	 *
	 * Find the signed distance of a point to the plane extending a facet of the convex hull.
	 *
	 * @param ptr 			The point to check.
	 * @param facet_index 	The index of the facet.
	 * @return 				The signed distance.
	 */
	double facet_signed_distance(const Eigen::Vector3d& ptr, size_t facet_index) const;

	/**
	 * Compute the "support point" for a convex_hull_walk. That is: the third point to define the cutting plane,
	 * in this case computed by taking the halfway point between a and b and projecting it onto the convex hull.
	 *
	 * @param a		The first point.
	 * @param b		The second point.
	 * @return		The support point.
	 */
	Eigen::Vector3d computeSupportPoint(const ConvexHullPoint &a, const ConvexHullPoint &b) const;

	/**
	 *
	 * Internal: Given a ConvexHullPoint, a cutting plane, and a previous face index,
	 * find the next point on the convex hull walk.
	 *
	 * @param current_point		The current point.
	 * @param cutting_plane		The cutting plane.
	 * @param last_face_idx		The previous face index. (Assumed != current_point.facet_index)
	 *
	 * @return					The next point, on the next face.
	 */
	[[nodiscard]] ConvexHullPoint nextStep(const ConvexHullPoint &current_point,
										   const Plane3d &cutting_plane,
										   size_t last_face_id) const;

	/**
	 * Internal: Given an initial point on the convex hull, a cutting plane, and a point,
	 * find the next point on the path traced by cutting the facet of the convex hull by the plane,
	 * in the direction of the given point.
	 *
	 * @param towards_point		The point to walk towards (to pick between the two intersections).
	 * @param cutting_plane		The cutting plane.
	 * @param start_point		The initial point.
	 * @return					The next point, on the next face.
	 */
	ConvexHullPoint firstStep(const Eigen::Vector3d &towards_point,
							  const Plane3d &cutting_plane,
							  const ConvexHullPoint & start_point) const ;

	/**
	 * Same as the ConvexHullPoint, but explicitly try to complete the step only
	 * by exiting through the interior of an edge (not a vertex).
	 *
	 * @param towards_point 	The point to walk towards (to pick between the two intersections).
	 * @param cutting_plane 	The cutting plane.
	 * @param start_point 		The initial point.
	 * @return 					The next point, on the next face (or nullopt if the step fails)
	 */
	std::optional<ConvexHullPoint> firstStepThroughEdges(
			const Eigen::Vector3d &towards_point,
			const Plane3d &cutting_plane,
			const ConvexHullPoint & start_point) const;

	/**
	 * Generate the next step on the convex hull walk; such a point is assumed to exist and that
	 * the path has not yet been completed.
	 *
	 * @param b 				The goal point.
	 * @param support_point 	The support point.
	 * @param cutting_plane 	The cutting plane.
	 * @param walk 				The current walk.
	 * @return 					The next step.
	 */
	ConvexHullPoint walk_step(const ConvexHullPoint &b,
							  const Eigen::Vector3d &support_point,
							  const Plane3d &cutting_plane,
							  const std::vector<ConvexHullPoint> &walk) const;

	/**
	 * Compute the intersection with the given edge of the given facet, if any.
	 *
	 * The method is "strict" in the sense that nullopt will be returned if the intersection is on a vertex.
	 *
	 * @param face_id			The index of the facet.
	 * @param edge				The edge to intersect with.
	 * @param cutting_plane		The cutting plane.
	 * @return					The intersection point, if any.
	 */
	std::optional<ConvexHullPoint> strictEdgeTraversal(size_t face_id,
													   TriangleEdgeId edge,
													   const Plane3d &cutting_plane) const;

	/**
	 * Given a vertex of a facet in the convex hull, generate a ConvexHullPoint at that vertex but on the facet
	 * that makes the most progress towards the goal point.
	 *
	 * @param towards_point 	The goal/direction indication point.
	 * @param current_face 		The facet containing the vertex.
	 * @param v 				The vertex.
	 * @return 					The ConvexHullPoint.
	 */
	ConvexHullPoint stepAroundVertexTowards(const Eigen::Vector3d &towards_point,
											size_t current_face,
											TriangleVertexId &v) const;

	/**
	 * Given a point on the convex hull a cutting plane, find the next point of the walk if such a point appears
	 * on an edge of the facet that lies in the cutting plane.
	 *
	 * @param b 				The goal point.
	 * @param support_point 	The support point.
	 * @param cutting_plane 	The cutting plane.
	 * @param walk 				The current walk.
	 * @return 					The next step if an edge lies in the plane, otherwise nullopt.
	 */
	std::optional<ConvexHullPoint> step_through_edge_in_cutting_plane(const ConvexHullPoint &b,
																	  const Eigen::Vector3d &support_point,
																	  const Plane3d &cutting_plane,
																	  const std::vector<ConvexHullPoint> &walk) const;
};

#endif //NEW_PLANNERS_CONVEXHULLSHELL_H
