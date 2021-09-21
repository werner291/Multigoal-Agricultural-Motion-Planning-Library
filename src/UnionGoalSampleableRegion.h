//
// Created by werner on 9/21/21.
//

#ifndef NEW_PLANNERS_UNIONGOALSAMPLEABLEREGION_H
#define NEW_PLANNERS_UNIONGOALSAMPLEABLEREGION_H

#include <ompl/base/goals/GoalSampleableRegion.h>

/**
 * Represents the union of all goal sampling regions.
 * Samples are drawn sequentially from each sub-region.
 */
class UnionGoalSampleableRegion : public ompl::base::GoalSampleableRegion {

    std::vector<std::shared_ptr<const GoalSampleableRegion>> goals;

    // sampleGoal really shouldn't be const... Oh well.
    mutable size_t next_goal = 0;

public:
    UnionGoalSampleableRegion(const ompl::base::SpaceInformationPtr &si,
                              const std::vector<std::shared_ptr<const GoalSampleableRegion>> &goals);

    double distanceGoal(const ompl::base::State *st) const override;

    void sampleGoal(ompl::base::State *st) const override;

    [[nodiscard]] unsigned int maxSampleCount() const override;

};

#endif //NEW_PLANNERS_UNIONGOALSAMPLEABLEREGION_H
