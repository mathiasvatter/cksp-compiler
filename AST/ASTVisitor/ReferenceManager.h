//
// Created by Mathias Vatter on 13.11.24.
//

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <memory>

class NodeDataStructure;
class NodeReference;

/**
 * @brief Verwaltet Referenzen zu Datenstrukturen
 */
class ReferenceManager {
private:
	std::unordered_map<std::shared_ptr<NodeDataStructure>, std::unordered_set<NodeReference*>> reference_map;
public:

	void add_reference(const std::shared_ptr<NodeDataStructure>& data_struct,
					   NodeReference* reference);

	void remove_reference(const std::shared_ptr<NodeDataStructure>& data_struct,
						  NodeReference* reference);

	void replace_data_structure(const std::shared_ptr<NodeDataStructure>& old_data_struct,
								const std::shared_ptr<NodeDataStructure>& new_data_struct);

	void replace_reference(const std::shared_ptr<NodeDataStructure>& data_struct,
						   NodeReference* old_reference,
						   NodeReference* new_reference);

};
