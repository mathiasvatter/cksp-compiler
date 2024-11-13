//
// Created by Mathias Vatter on 13.11.24.
//

#include "ReferenceManager.h"
#include "../ASTNodes/AST.h"

void ReferenceManager::add_reference(const std::shared_ptr<NodeDataStructure> &data_struct,
									 NodeReference *reference) {
	if (reference) {  // Nur gültige Zeiger hinzufügen
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
	auto it = reference_map.find(old_data_struct);
	if (it != reference_map.end()) {
		auto references = std::move(it->second);
		reference_map.erase(it);
		reference_map[new_data_struct] = std::move(references);

		// Aktualisiert jede NodeReference
		for (auto ref : reference_map[new_data_struct]) {
			if (ref) {
				ref->declaration = new_data_struct;
			}
		}
	}
}

void ReferenceManager::replace_reference(const std::shared_ptr<NodeDataStructure> &data_struct,
										 NodeReference *old_reference,
										 NodeReference *new_reference) {
	// Entferne die alte Referenz und füge die neue Referenz hinzu
	remove_reference(data_struct, old_reference);
	add_reference(data_struct, new_reference);
}
