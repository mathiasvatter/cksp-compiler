//
// Created by Mathias Vatter on 13.11.24.
//

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include "../../../misc/HashFunctions.h"

class NodeDataStructure;
class NodeReference;

/**
 * @brief Verwaltet Referenzen zu Datenstrukturen
 */
class ReferenceManager {
private:
	std::unordered_map<std::weak_ptr<NodeDataStructure>, std::unordered_set<NodeReference*>, WeakPtrHash, WeakPtrEqual>
	    reference_map;

public:

	void reset() {
		reference_map.clear();
	}

	/// Removes all weak_ptrs that are expired
	void cleanup();

	[[nodiscard]] const std::unordered_map<std::weak_ptr<NodeDataStructure>, std::unordered_set<NodeReference*>,
	    WeakPtrHash, WeakPtrEqual>& get_reference_map() const {
		return reference_map;
	}

	/// returns the references of a certain data_structure
	std::unordered_set<NodeReference*>& get_references(const std::shared_ptr<NodeDataStructure>& data_struct) {
		return reference_map[data_struct];
	}

	/// Gibt eine Menge aller NodeTypes der Referenzen für ein bestimmtes data_struct zurück
	std::unordered_set<NodeType> get_reference_types(const std::shared_ptr<NodeDataStructure>& data_struct);

	void add_reference(const std::shared_ptr<NodeDataStructure>& data_struct,
					   NodeReference* reference);

	void remove_reference(const std::shared_ptr<NodeDataStructure>& data_struct,
						  NodeReference* reference);

	void remove_data_structure(const std::shared_ptr<NodeDataStructure>& data_struct) {
		reference_map.erase(data_struct);
	}

	void add_references(const std::shared_ptr<NodeDataStructure>& data_struct,
						const std::unordered_set<NodeReference*>& references) {
		reference_map[data_struct].insert(references.begin(), references.end());
	}

	void replace_data_structure(const std::shared_ptr<NodeDataStructure>& old_data_struct,
								const std::shared_ptr<NodeDataStructure>& new_data_struct);

	void replace_reference(const std::shared_ptr<NodeDataStructure>& data_struct,
						   NodeReference* old_reference,
						   NodeReference* new_reference);

	/// Checks if all NodeTypes of the references of a data_struct match the NodeType of data_struct
	bool all_reference_types_match(const std::shared_ptr<NodeDataStructure>& data_struct);
	static bool reference_type_exists(const std::unordered_set<NodeType>& types, NodeType type) {
		return types.find(type) != types.end();
	}
};
