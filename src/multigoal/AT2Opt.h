//
// Created by werner on 12-10-21.
//

#ifndef NEW_PLANNERS_AT2OPT_H
#define NEW_PLANNERS_AT2OPT_H

#include "multi_goal_planners.h"
#include "approach_table.h"

class AT2Opt : public MultiGoalPlanner {
public:
    MultiGoalPlanResult plan(const std::vector<GoalSamplerPtr> &goals,
                             const ompl::base::State *start_state,
                             PointToPointPlanner &point_to_point_planner) override;

    std::string getName() override;

    multigoal::ATSolution random_initial_solution(const PointToPointPlanner &point_to_point_planner,
                                                  const multigoal::GoalApproachTable &table,
                                                  const ompl::base::State *&start_state);

    void check_replacements_validity(const std::vector<Replacement> &replacements) const;
};


#endif //NEW_PLANNERS_AT2OPT_H
