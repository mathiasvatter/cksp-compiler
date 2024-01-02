//
// Created by Mathias Vatter on 02.01.24.
//

#include "JSONVisitor.h"

std::string remove_double_quotes(const std::string &input) {
    if (input.size() >= 2 && input.front() == '"' && input.back() == '"') {
        return input.substr(1, input.size() - 2);
    } else {
        return input;
    }
}

void NCKPTranslator::visit(JSONBool &boolean) {

}

void NCKPTranslator::visit(JSONString &str) {
    if(m_current_property == "\"id\"") {
        m_ui_controls.insert({remove_double_quotes(str.value), m_current_control_idx});
    }
}

void NCKPTranslator::visit(JSONInt &num) {
    if(m_current_property == "\"index\"") {
        m_current_control_idx = num.value;
    }
}

void NCKPTranslator::visit(JSONFloat &num) {

}

void NCKPTranslator::visit(JSONArray &array) {
    for(auto &elem: array.elements) {
        elem->accept(*this);
    }
}

void NCKPTranslator::visit(JSONObject &object) {
    for(auto &pair : object.properties) {
        m_current_property = pair.first;
        pair.second->accept(*this);
    }
}

