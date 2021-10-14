
#include "AT2Opt.h"
#include "multi_goal_planners.h"
#include "approach_table.h"

using namespace multigoal;


MultiGoalPlanResult AT2Opt::plan(const std::vector<GoalSamplerPtr> &goals,
                                 const ompl::base::State *start_state,
                                 PointToPointPlanner &point_to_point_planner) {

    // Build a goal approach table.
    GoalApproachTable table = takeGoalSamples(point_to_point_planner.getPlanner()->getSpaceInformation(), goals, 50);

    // Delete any but the five best samples. (TODO: This is rather naive, surely we could vary this, maybe?)
    keepBest(*point_to_point_planner.getOptimizationObjective(), table, 5);

    // Start with a randomized solution (TODO: Or nearest-neighbours maybe?)
    ATSolution solution = random_initial_solution(point_to_point_planner, table, start_state);

    // Sanity check. TODO: Remove once the code works, maybe move to a test somewhere.
    solution.check_valid(table);

    // Keep track of missing targets. TODO Actually use this.
    std::unordered_set<size_t> missing_targets = find_missing_targets(solution, table);

    // Run until 10 seconds have passed (Maybe something about tracking convergence rates?)
    ompl::base::PlannerTerminationCondition ptc = ompl::base::timedPlannerTerminationCondition(10.0);

    while (!ptc) {
        for (size_t i = 0; i < solution.getSegments().size(); i++) {
            for (size_t j = i + 1; j < solution.getSegments().size(); j++) {

                std::vector<Replacement> replacements;

                if (j == i + 1) {

                    Replacement repl;

                    repl.first_segment = i;
                    repl.last_segment = std::min(i + 2, solution.getSegments().size() - 1);

                    repl.visitations.push_back(solution.getSegments()[j].visitation);

                    repl.visitations.push_back(solution.getSegments()[i].visitation,);

                    if (i + 2 < solution.getSegments().size()) {
                        repl.visitations.push_back(solution.getSegments()[i + 2].visitation,);
                    }

                    replacements.push_back(repl);

                } else {

                    replacements.push_back({
                                                   i, i + 1, {solution.getSegments()[j].visitation,
                                                              solution.getSegments()[i + 1].visitation}
                                           });

                    Replacement repl;

                    repl.first_segment = j;
                    repl.last_segment = std::min(j + 2, solution.getSegments().size() - 1);

                    repl.visitations.push_back(solution.getSegments()[i].visitation);

                    if (i + 2 < solution.getSegments().size()) {
                        repl.visitations.push_back(solution.getSegments()[j + 1].visitation);
                    }

                    replacements.push_back(repl);
                }

                // Validity checking.
                check_replacements_validity(replacements);

                struct NewApproachAt {
                    size_t index{};
                    GoalApproach ga;
                };

                std::vector<NewApproachAt> computed_replacements;

                for (const auto &repl: replacements) {
                    const ompl::base::State *from_state = repl.first_segment == 0 ? start_state
                                                                                  : solution.getSegments()[repl.first_segment].approach_path.getState(
                                    0);
                    for (size_t vidx = 0; vidx < repl.visitations.size(); vidx++) {
                        const auto &viz = repl.visitations[vidx];

                        ompl::base::State *goal = table[viz.target_idx][viz.approach_idx]->get();
                        auto ptp = point_to_point_planner.planToOmplState(0.1,
                                                                          from_state,
                                                                          goal);
                        if (!ptp) break; // FIXME: Make sure we break from the whole thing.

                        computed_replacements.push_back({
                                                                repl.first_segment + vidx,
                                                                GoalApproach{
                                                                        viz.target_idx,
                                                                        viz.approach_idx,
                                                                        ptp.value()
                                                                }
                                                        });

                        from_state = goal;
                    }
                }

                double old_cost = 0.0;
                double new_cost = 0.0;
                for (const auto &cr: computed_replacements) {
                    old_cost += solution.getSegments()[cr.index].approach_path.length(); // TODO Cache this, maybe?
                    new_cost += cr.ga.approach_path.length();
                }

                if (old_cost > new_cost) {
                    for (auto &cr: computed_replacements) {
                        solution.getSegments()[cr.index] = cr.ga;
                    }
                }

                solution.check_valid(table);

            }

            // TODO: Try to insert one of the missing goals after i.
        }
    }

    return solution.toMultiGoalResult();

}

void AT2Opt::check_replacements_validity(const std::vector<Replacement> &replacements) const {
    for (size_t ridx = 0; ridx < replacements.size(); ridx++) {
        // Replacements must be ordered and strictly non-overlapping.
        assert(replacements[ridx].visitations.size() ==
               (replacements[ridx].last_segment - replacements[ridx].first_segment) + 1);
        // The last segment must be later than the first.
        assert(replacements[ridx].first_segment <= replacements[ridx].last_segment);
        // Subsequent replacements must be strictly non-overlapping.
        if (ridx + 1 < replacements.size())
            assert(replacements[ridx].last_segment < replacements[ridx + 1].first_segment);
    }
}

ATSolution AT2Opt::random_initial_solution(const PointToPointPlanner &point_to_point_planner,
                                           const GoalApproachTable &table,
                                           const ompl::base::State *&start_state) {

    ATSolution solution;
    // Create an empty solution.
    // Visit goals in random order and with random approach.
    for (Visitation v: random_initial_order(table)) {
        auto ptp_result = point_to_point_planner.planToOmplState(MAX_TIME_PER_TARGET_SECONDS,
                                                                 solution.getLastState().value_or(start_state),
                                                                 table[v.target_idx][v.approach_idx]->get());
        // Drop any goals where planning failed.
        if (ptp_result) {
            solution.getSegments().push_back(GoalApproach{
                    v.target_idx, v.approach_idx, ptp_result.value()
            });
        }
    }

    return solution;
}


std::string AT2Opt::getName() {
    return "AT2Opt";
}
