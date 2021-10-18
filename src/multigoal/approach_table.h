
#ifndef NEW_PLANNERS_APPROACH_TABLE_H
#define NEW_PLANNERS_APPROACH_TABLE_H

#include <ompl/base/goals/GoalSampleableRegion.h>
#include <ompl/base/ScopedState.h>
#include <ompl/base/OptimizationObjective.h>
#include <ompl/geometric/PathGeometric.h>
#include <unordered_set>
#include "multi_goal_planners.h"

namespace multigoal {

    /// A vector of sampleable goal regions owned through shared pointers.
    typedef std::vector<std::shared_ptr<ompl::base::GoalSampleableRegion>> SampleableGoals;

    /// A vector of vectors (effectively a matrix, but check inner vector lengths!)
    /// One inner vector for every goal, containing a number of goal samples.
    typedef std::vector<std::vector<ompl::base::ScopedStatePtr>> GoalApproachTable;

    /// An index into a GoalApproachTable
    struct Visitation {
        size_t target_idx; /// Outer vector index
        size_t approach_idx; /// Inner vector index.
    };

    /// \brief Compute the GoalApproachTable with at most k valid samples per goal.
    GoalApproachTable takeGoalSamples(const ompl::base::SpaceInformationPtr &si, const SampleableGoals &goals, int k);

    /// \brief Compute a randomized order to visit all goals, with a random approach direction every time.
    std::vector<Visitation> random_initial_order(const GoalApproachTable &goal_samples);

    /// Reduce a GoalApproachTable to the (at most) k best states, according to the optimization objective.
    /// May change internal ordering of the table.
    void keepBest(const ompl::base::OptimizationObjective &opt, GoalApproachTable &table, int keep_k);

    /// Designates an entry in a GoalApproachTable, and contains a path to that state.
    /// To be interpreted in the context of a sequence of these, see `ATSolution`.
    struct GoalApproach {
        /// Pointer into the GoalApproachTable
        Visitation visitation;
        // Path to that goal state, from a starting state to be interpreted from context.
        ompl::geometric::PathGeometric approach_path;
    };

    /// Designates a range inside of an ATSolution, and a number of Visitations to replace that range with.
    struct Replacement {
        size_t first_segment{};
        size_t last_segment{}; // End of range is inclusive.
        std::vector<Visitation> visitations;
    };

    /// Designates a single point-to-point movement in an ATSolution,
    /// and includes the computed GoalApproach to replace it with.
    struct NewApproachAt {
        size_t index{};
        GoalApproach ga;
    };

    /// Equivalent of a MultiGoalPlanResult, but defined in terms of a GoalApproachTable.
    class ATSolution {

        /// A series of point-to-point motions that can be concatenated to form a multi-goal-visiting trajectory,
        /// plus some information about how each motion relates to the `GoalApproachTable`.
        std::vector<GoalApproach> solution_;

        /// Convenience pointer to the SpaceInformation.
        ompl::base::SpaceInformationPtr si_;

    public:

        ATSolution(ompl::base::SpaceInformationPtr si);

        [[nodiscard]] std::vector<GoalApproach> &getSegments();

        [[nodiscard]] const std::vector<GoalApproach> &getSegmentsConst() const;

        /// \brief Get the state at the end of the trajectory, or none if no segments if the solution is empty. Pointer is never null.
        [[nodiscard]] std::optional<const ompl::base::State *> getLastState() const;

        /// Check internal invariants via assertions (crashes if violated)
        void check_valid(const GoalApproachTable &table) const;

        bool is_improvement(const std::vector<NewApproachAt> &replacements) const;

        bool apply_replacements(std::vector<NewApproachAt> &replacements);

        /// Strip out the ATSolution-specific information to create a MultiGoalPlanResult.
        MultiGoalPlanResult toMultiGoalResult();
    };



    /**
     * \brief Compute the set of unvisited targets in the ATSolution.
     *
     * Note: This does not check if the end-effector passes near any targets during a point-to-point motion,
     * it only looks at the level of the GoalApproachTable.
     */
    std::unordered_set<size_t> find_missing_targets(const ATSolution &solution, const GoalApproachTable &goals);

    /**
     * \brief Computes which parts of an ATSolution should be replaced in order to realize a swap
     * of targets i and j (in the current ATSolution's order). Each range is strictly disjoint.
     *
     * Requires that i < j, and that i,j are valid indices in the ATSolution.
     */
    std::vector<Replacement> replacements_for_swap(const multigoal::ATSolution &solution, size_t i, size_t j);

    /**
     * \brief Verify (through assertions) that a vector of Replacements is valid. (For debugging purposes)
     */
    void check_replacements_validity(const std::vector<Replacement> &replacements);

    /**
     * \brief Computes an ATSolution from a GoalApproachTable by visiting the goals in the order computed by `random_initial_order`.
     */
    multigoal::ATSolution random_initial_solution(const PointToPointPlanner &point_to_point_planner,
                                                  const GoalApproachTable &table,
                                                  const ompl::base::State *&start_state);

    std::optional<std::vector<multigoal::NewApproachAt>> computeNewPathSegments(const ompl::base::State *start_state,
                                                                                PointToPointPlanner &point_to_point_planner,
                                                                                const multigoal::GoalApproachTable &table,
                                                                                const multigoal::ATSolution &solution,
                                                                                const std::vector<Replacement> &replacements);
}

#endif //NEW_PLANNERS_APPROACH_TABLE_H
