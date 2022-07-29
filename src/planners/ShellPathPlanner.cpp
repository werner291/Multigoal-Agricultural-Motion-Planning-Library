
#include "ShellPathPlanner.h"
#include "../traveling_salesman.h"
#include "../probe_retreat_move.h"
#include "../DronePathLengthObjective.h"
#include "../experiment_utils.h"
#include <utility>

ShellPathPlanner::ShellPathPlanner(bool applyShellstateOptimization,
								   std::shared_ptr<SingleGoalPlannerMethods> methods,
								   std::shared_ptr<const ShellBuilder>  shellBuilder) :
		apply_shellstate_optimization(applyShellstateOptimization),
		methods(std::move(methods)), shell_builder(std::move(shellBuilder)) {}

MultiGoalPlanner::PlanResult ShellPathPlanner::plan(
		const ompl::base::SpaceInformationPtr &si,
		const ompl::base::State *start,
		const std::vector<ompl::base::GoalPtr> &goals,
		const AppleTreePlanningScene &planning_scene,
		ompl::base::PlannerTerminationCondition& ptc) {

	auto shell = shell_builder->buildShell(planning_scene, si);

    auto approaches = planApproaches(si, goals, *shell, ptc);

    PlanResult result {{}};

    if (approaches.empty()) {
        return result;
    }

    auto ordering = computeApproachOrdering(start, goals, approaches, *shell);

    auto first_approach = planFirstApproach(start, approaches[ordering[0]].second);

    if (!first_approach) {
        return result;
    }

    return assembleFullPath(si, goals, *shell, approaches, ordering, result, *first_approach);
}

MultiGoalPlanner::PlanResult ShellPathPlanner::assembleFullPath(
		const ompl::base::SpaceInformationPtr &si,
		const std::vector<ompl::base::GoalPtr> &goals,
		OMPLSphereShellWrapper &ompl_shell,
		const std::vector<std::pair<size_t, ompl::geometric::PathGeometric>> &approaches,
		const std::vector<size_t> &ordering,
		MultiGoalPlanner::PlanResult &result,
		ompl::geometric::PathGeometric &initial_approach) const {

    result.segments.push_back({approaches[ordering[0]].first, initial_approach});

    for (size_t i = 1; i < ordering.size(); ++i) {

        ompl::geometric::PathGeometric goal_to_goal(si);

        auto segment_path = retreat_move_probe(
                goals,
                ompl_shell,
                result,
                goal_to_goal,
                approaches[ordering[i - 1]],
                approaches[ordering[i]]
                );

        auto start = std::chrono::steady_clock::now();
        segment_path = optimize(segment_path, std::make_shared<DronePathLengthObjective>(si), si);
        auto end = std::chrono::steady_clock::now();

        result.segments.push_back({
            approaches[ordering[0]].first,
            segment_path
        });

    }

    return result;
}

ompl::geometric::PathGeometric
ShellPathPlanner::retreat_move_probe(const std::vector<ompl::base::GoalPtr> &goals, OMPLSphereShellWrapper &ompl_shell,
									 MultiGoalPlanner::PlanResult &result,
									 ompl::geometric::PathGeometric &goal_to_goal,
									 const std::pair<size_t, ompl::geometric::PathGeometric> &approach_a,
									 const std::pair<size_t, ompl::geometric::PathGeometric> &approach_b) const {

    goal_to_goal.append(approach_a.second);
    goal_to_goal.reverse();

    auto a = goals[approach_a.first].get();
    auto b = goals[approach_b.first].get();

    goal_to_goal.append(ompl_shell.path_on_shell(a,b));

    goal_to_goal.append(approach_b.second);

    return goal_to_goal;
}

std::optional<ompl::geometric::PathGeometric>
ShellPathPlanner::planFirstApproach(const ompl::base::State *start,
                                    ompl::geometric::PathGeometric &approach_path) {

    auto start_to_shell = methods->state_to_state(
            start,
            approach_path.getState(0)
    );

    if (start_to_shell) {
        start_to_shell->append(approach_path);
    }

    return start_to_shell;
}

std::vector<size_t> ShellPathPlanner::computeApproachOrdering(
        const ompl::base::State *start,
        const std::vector<ompl::base::GoalPtr> &goals,
        const std::vector<std::pair<size_t, ompl::geometric::PathGeometric>> &approaches,
        const OMPLSphereShellWrapper& shell) const {

    return tsp_open_end(
            [&](auto i) {
                return shell.predict_path_length(start, goals[approaches[i].first].get());
            },
            [&](auto i, auto j) {
                return shell.predict_path_length(
                        goals[approaches[i].first].get(),
                        goals[approaches[j].first].get()
                );
            },
            approaches.size()
    );
}

std::vector<std::pair<size_t, ompl::geometric::PathGeometric>>
ShellPathPlanner::planApproaches(const ompl::base::SpaceInformationPtr &si,
								 const std::vector<ompl::base::GoalPtr> &goals,
								 const OMPLSphereShellWrapper &ompl_shell,
								 ompl::base::PlannerTerminationCondition &ptc) const {

    std::vector<std::pair<size_t, ompl::geometric::PathGeometric>> approaches;

    for (const auto& [goal_i, goal] : goals | ranges::views::enumerate) {
        if (auto approach = planApproachForGoal(si, ompl_shell, goal)) {
			assert(approach->getStateCount() > 0);
            approaches.emplace_back(
                    goal_i,
                    *approach
            );
        }

		checkPtc(ptc);

    }

    return approaches;
}

std::optional<ompl::geometric::PathGeometric> ShellPathPlanner::planApproachForGoal(
        const ompl::base::SpaceInformationPtr &si,
        const OMPLSphereShellWrapper &ompl_shell,
        const ompl::base::GoalPtr &goal) const {

    ompl::base::ScopedState shell_state(si);
    ompl_shell.state_on_shell(goal.get(), shell_state.get());

	if (ompl_shell.project(goal.get()).z() < 0) {

		moveit::core::RobotState start_state(si->getStateSpace()->as<DroneStateSpace>()->getRobotModel());
		si->getStateSpace()->as<DroneStateSpace>()->copyToRobotState(start_state, shell_state.get());

		std::cout << ompl_shell.project(goal.get()) << " shell state: " << start_state << std::endl;

		std::cout << "Validity: " << si->isValid(shell_state.get()) << std::endl;
	}

    auto approach_path = methods->state_to_goal(shell_state.get(), goal);

    if (apply_shellstate_optimization && approach_path) {
        *approach_path = optimizeExit(
                goal.get(),
                *approach_path,
                std::make_shared<DronePathLengthObjective>(si),
                ompl_shell,
                si
        );
    }

    return approach_path;
}

Json::Value ShellPathPlanner::parameters() const {
    Json::Value result;

	result["shell_builder_params"] = shell_builder->parameters();
    result["apply_shellstate_optimization"] = apply_shellstate_optimization;
    result["ptp"] = methods->parameters();

    return result;
}

std::string ShellPathPlanner::name() const {
    return "ShellPathPlanner";
}

std::shared_ptr<OMPLSphereShellWrapper> PaddedSphereShellAroundLeavesBuilder::buildShell(const AppleTreePlanningScene &scene_info,
																						 const ompl::base::SpaceInformationPtr &si) const {

	auto enclosing = compute_enclosing_sphere(scene_info.scene_msg, 0.0);

	enclosing.radius += padding * (enclosing.center.z() - enclosing.radius);

	std::cout << "Enclosing sphere: " << enclosing.center << " " << enclosing.radius << std::endl;

	auto shell = std::make_shared<SphereShell>(enclosing.center, enclosing.radius);

	return std::make_shared<OMPLSphereShellWrapper>(shell, si);

}

Json::Value PaddedSphereShellAroundLeavesBuilder::parameters() const {
	Json::Value result;
	result["padding"] = padding;
	return result;
}

PaddedSphereShellAroundLeavesBuilder::PaddedSphereShellAroundLeavesBuilder(double padding) : padding(padding) {
}
