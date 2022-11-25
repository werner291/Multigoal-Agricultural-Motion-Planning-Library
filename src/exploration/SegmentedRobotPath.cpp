// Copyright (c) 2022 University College Roosevelt
//
// All rights reserved.

//
// Created by werner on 25-11-22.
//

#include "SegmentedRobotPath.h"


SegmentedRobotPath::Index SegmentedRobotPath::next_waypoint_index(SegmentedRobotPath::Index idx) {
	if (idx.waypoint_index < segments[idx.segment_index].waypoints.size() - 1) {
		idx.waypoint_index++;
	} else {
		idx.segment_index++;
		idx.waypoint_index = 0;
	}
	return idx;
}

SegmentedRobotPath::Index SegmentedRobotPath::prev_waypoint_index(SegmentedRobotPath::Index idx) {
	if (idx.waypoint_index > 0) {
		idx.waypoint_index--;
	} else {
		idx.segment_index--;
		idx.waypoint_index = segments[idx.segment_index].waypoints.size() - 1;
	}
	return idx;
}

SegmentedRobotPath::Index SegmentedRobotPath::first_waypoint_index() {
	return Index{0, 0};
}

SegmentedRobotPath::Index SegmentedRobotPath::last_waypoint_index() {
	return Index{segments.size() - 1, segments.back().waypoints.size() - 1};
}

moveit::core::RobotState &SegmentedRobotPath::waypoint(SegmentedRobotPath::Index idx) {
	return segments[idx.segment_index].waypoints[idx.waypoint_index];
}

bool SegmentedRobotPath::is_at_target(SegmentedRobotPath::Index idx) {
	return segments[idx.segment_index].waypoints.size() == idx.waypoint_index + 1;
}

void SegmentedRobotPath::pop_first() {

	assert(!segments.empty() && !segments.front().waypoints.empty());

	segments.front().waypoints.erase(segments.front().waypoints.begin());
	if (segments.front().waypoints.empty()) {
		segments.erase(segments.begin());
	}
}

bool SegmentedRobotPath::empty() const {
	return segments.empty();
}

bool SegmentedRobotPath::Index::operator==(const SegmentedRobotPath::Index &other) const {
	return segment_index == other.segment_index && waypoint_index == other.waypoint_index;
}

bool SegmentedRobotPath::Index::operator!=(const SegmentedRobotPath::Index &other) const {
	return !(*this == other);
}

bool SegmentedRobotPath::Index::operator<(const SegmentedRobotPath::Index &other) const {
	return segment_index < other.segment_index ||
		   (segment_index == other.segment_index && waypoint_index < other.waypoint_index);
}

bool SegmentedRobotPath::Index::operator>(const SegmentedRobotPath::Index &other) const {
	return segment_index > other.segment_index ||
		   (segment_index == other.segment_index && waypoint_index > other.waypoint_index);
}

bool SegmentedRobotPath::Index::operator<=(const SegmentedRobotPath::Index &other) const {
	return *this < other || *this == other;
}

bool SegmentedRobotPath::Index::operator>=(const SegmentedRobotPath::Index &other) const {
	return *this > other || *this == other;
}
