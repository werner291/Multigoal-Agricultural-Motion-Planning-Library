
#include "mesh_utils.h"
#include "moveit_conversions.h"
#include "msgs_utilities.h"
#include "math_utils.h"

Eigen::Vector3d closestPointOnMesh(const shape_msgs::msg::Mesh &mesh, const Eigen::Vector3d &query_point) {
	double closest_distance = std::numeric_limits<double>::max();
	Eigen::Vector3d closest_point;

	for (const auto &tr: mesh.triangles) {
		Eigen::Vector3d a = toEigen(mesh.vertices[tr.vertex_indices[0]]);
		Eigen::Vector3d b = toEigen(mesh.vertices[tr.vertex_indices[1]]);
		Eigen::Vector3d c = toEigen(mesh.vertices[tr.vertex_indices[2]]);

		Eigen::Vector3d candidate = closest_point_on_triangle(query_point, a, b, c);

		double distance = (query_point - candidate).norm();
		if (distance < closest_distance) {
			closest_distance = distance;
			closest_point = candidate;
		}
	}
	return closest_point;
}

std::vector<shape_msgs::msg::Mesh> break_down_to_connected_components(const shape_msgs::msg::Mesh &combined_mesh) {

	// Extract the connected components.
	auto cc = connected_vertex_components(combined_mesh);

	// Allocate n meshes for the n connected components.
	std::vector<shape_msgs::msg::Mesh> mesh_components(cc.size());

	// Split up the vertices and build up an index translation map.
	std::vector<std::pair<size_t, size_t>> index_map(combined_mesh.vertices.size());

	// For every connected component...
	for (size_t i = 0; i < cc.size(); i++) {
		// ...and for every vertex in the connected component...
		for (size_t j = 0; j < cc[i].size(); j++) {
			// ...record the connected component ID and new vertex ID within the component in the index map,
			index_map[cc[i][j]] = std::make_pair(i, j);
			// ...and add the vertex to the mesh for the connected component.
			mesh_components[i].vertices.push_back(combined_mesh.vertices[cc[i][j]]);
		}
	}

	// For every triangle in the combined mesh...
	for (auto triangle : combined_mesh.triangles) {

		// Look up the connected component ID and new vertex ID of the triangle's vertices in the index map.
		auto &v0 = index_map[triangle.vertex_indices[0]];
		auto &v1 = index_map[triangle.vertex_indices[1]];
		auto &v2 = index_map[triangle.vertex_indices[2]];

		// Sanity check: the three vertices should all be in the same connected component.
		assert(v0.first == v1.first && v1.first == v2.first);

		// Add the triangle to the mesh for the connected component.
		mesh_components[v0.first].triangles.push_back(triangle);
		mesh_components[v0.first].triangles.back().vertex_indices[0] = v0.second;
		mesh_components[v0.first].triangles.back().vertex_indices[1] = v1.second;
		mesh_components[v0.first].triangles.back().vertex_indices[2] = v2.second;
	}
	return mesh_components;
}
