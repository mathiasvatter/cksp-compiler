//
// Created by Mathias Vatter on 13.11.24.
//

#include "ReferenceManager.h"
#include "../../ASTNodes/AST.h"

void ReferenceManager::add_reference(const std::shared_ptr<NodeDataStructure> &data_struct,
									 NodeReference *reference) {
	if (data_struct and reference) {  // Nur gültige Zeiger hinzufügen
		reference_map[data_struct].insert(reference);
	}
}

void ReferenceManager::remove_reference(const std::shared_ptr<NodeDataStructure> &data_struct,
										NodeReference *reference) {
	auto it = reference_map.find(data_struct);
	if (it != reference_map.end()) {
		it->second.erase(reference);
		if (it->second.empty()) {
			reference_map.erase(it);
		}
	}
}

void ReferenceManager::replace_data_structure(const std::shared_ptr<NodeDataStructure> &old_data_struct,
											  const std::shared_ptr<NodeDataStructure> &new_data_struct) {
	if (auto node = reference_map.extract(old_data_struct)) {
		node.key() = new_data_struct;
		reference_map.insert(std::move(node));

		// Aktualisiert jede NodeReference
		for (auto ref : reference_map[new_data_struct]) {
			if (ref) {
				ref->declaration = new_data_struct;
			}
		}
	}

//	auto &references = get_references(old_data_struct);
//	for (auto ref : references) {
//		if (ref) {
//			ref->declaration = new_data_struct;
//		}
//	}
//	add_references(new_data_struct, references);
//	remove_data_structure(old_data_struct);
}

void ReferenceManager::replace_reference(const std::shared_ptr<NodeDataStructure> &data_struct,
										 NodeReference *old_reference,
										 NodeReference *new_reference) {
	// Entferne die alte Referenz und füge die neue Referenz hinzu
	remove_reference(data_struct, old_reference);
	add_reference(data_struct, new_reference);
}

std::unordered_set<NodeType> ReferenceManager::get_reference_types(const std::shared_ptr<NodeDataStructure> &data_struct) {
	std::unordered_set<NodeType> types;
	auto references = get_references(data_struct);

	for (const auto& reference : references) {
		if (reference) {
			types.insert(reference->get_node_type());
		}
	}
	return types;
}

bool ReferenceManager::all_reference_types_match(const std::shared_ptr<NodeDataStructure> &data_struct) {
	auto ref_node_type = data_struct->get_ref_node_type(); // NodeType des data_struct
	auto references = get_references(data_struct);

	for (const auto& reference : references) {
		if (reference && reference->get_node_type() != ref_node_type) {
			return false; // Abweichender NodeType gefunden
		}
	}
	return true; // Alle NodeTypes stimmen überein
}

void ReferenceManager::cleanup() {
	for (auto it = reference_map.begin(); it != reference_map.end();) {
		if (it->first.expired()) {
			it = reference_map.erase(it); // Entferne ungültige weak_ptr
		} else {
			++it;
		}
	}
}
