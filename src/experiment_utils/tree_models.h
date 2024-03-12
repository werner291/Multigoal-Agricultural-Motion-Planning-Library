// Copyright (c) 2022 University College Roosevelt
//
// All rights reserved.

//
// Created by werner on 3/12/24.
//

#ifndef MGODPL_TREE_MODELS_H
#define MGODPL_TREE_MODELS_H

#include <vector>
#include <variant>
#include <json/value.h>
#include <random_numbers/random_numbers.h>
#include <unordered_map>

#include "TreeMeshes.h"
#include "../math/AABB.h"
#include "LoadedTreeModel.h"
#include "declarative/fruit_models.h"

namespace mgodpl::declarative {

	/**
	 * @struct TreeModelParameters
	 *
	 * @brief A structure to hold the parameters related to a tree model in an experiment.
	 */
	struct TreeModelParameters {
		const std::string name; //< A model name; corresponds to one of the models in the `3d-models` directory.
		const double leaf_scale; //< The scaling factor for the leaves; 1.0 skips the rescaling.
		const FruitSubset fruit_subset; //< The subset of fruits to use in the experiment.
		const int seed; //< The seed for randomizing the tree model.
	};

	Json::Value toJson(const TreeModelParameters &treeModelParameters);

	std::vector<shape_msgs::msg::Mesh> random_subset_of_meshes(random_numbers::RandomNumberGenerator &rng,
															   const experiments::LoadedTreeModel &tree_model,
															   const RandomSubset &subset_params);

	using FruitModels = std::variant<std::vector<SphericalFruit>, std::vector<MeshFruit>>;

	FruitModels instantiate_fruit_models(const experiments::LoadedTreeModel &tree_model,
										 const FruitSubset &tree_params,
										 random_numbers::RandomNumberGenerator &rng);

	/**
	 * An instantiation of the TreeModelParameters, containing a tree model and the fruit models.
	 */
	struct FullTreeModel {
		const std::shared_ptr<const experiments::LoadedTreeModel> tree_model;
		const FruitModels fruit_models;
		const shape_msgs::msg::Mesh scaled_leaves;
	};

	FullTreeModel instantiate_tree_model(const TreeModelParameters &tree_params,
										 experiments::TreeModelCache &cache,
										 random_numbers::RandomNumberGenerator &rng);

}

#endif //MGODPL_TREE_MODELS_H
