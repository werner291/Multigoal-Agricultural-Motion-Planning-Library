

#include "../src/planning_scene_diff_message.h"
#include "../src/experiment_utils.h"
#include "../src/prm_multigoal.h"
#include "../src/thread_pool.hpp"
#include "../src/run_experiment.h"

#include <range/v3/all.hpp>

using namespace std;
using namespace ranges;

int main(int argc, char **argv) {

    const auto samples_per_goal = ranges::views::iota(2,10);
    const double plan_times_seconds[] = {1.0, 2.0, 5.0, 10.0};//, 15.0};//, 20.0};//, 30.0, 60.0};
    const bool optimize_segments_options[] = {true};//{false,true};

    auto allocators = ranges::views::cartesian_product(plan_times_seconds, samples_per_goal, optimize_segments_options)
        | ranges::views::transform([&](const auto tuple) -> NewMultiGoalPlannerAllocatorFn {
        return [tuple = tuple](const AppleTreePlanningScene &scene_info,
                               const ompl::base::SpaceInformationPtr &si) {
            return std::make_shared<MultigoalPrmStar>(get<0>(tuple),get<1>(tuple),get<2>(tuple));
        };
    }) | to_vector;

    ompl::msg::setLogLevel(ompl::msg::LOG_ERROR);

    run_planner_experiment(allocators, "analysis/prm_multigoal_experiment.json", 10);

    return 0;
}