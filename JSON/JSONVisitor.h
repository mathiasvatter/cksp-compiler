//
// Created by Mathias Vatter on 02.01.24.
//

#pragma once

#include "JSONParser.h"
#include "../BuiltinsProcessing/DefinitionProvider.h"

class JSONVisitor {
public:
    virtual void visit(JSONObject& object) = 0;
    virtual void visit(JSONArray& array) = 0;
    virtual void visit(JSONString& str) = 0;
    virtual void visit(JSONInt& num) = 0;
    virtual void visit(JSONFloat& num) = 0;
    virtual void visit(JSONBool& boolean) = 0;

};

inline std::map<int, std::string> UI_CONTROL_INDEX = {
        {0, "ui_panel"},
        {1, "ui_button"},
        {2, "ui_file_selector"},
        {3, "ui_knob"},
        {4, "ui_label"},
        {5, "ui_level_meter"},
        {6, "ui_menu"},
        {7, "ui_slider"},
        {8, "ui_switch"},
        {9, "ui_table"},
        {10, "ui_text_edit"},
        {11, "ui_value_edit"},
        {12, "ui_waveform"},
        {13, "ui_wavetable"},
        {14, "ui_xy"},
        {15, "ui_mouse_area"},
};

class NCKPTranslator : public JSONVisitor {
public:
	explicit NCKPTranslator(DefinitionProvider* definition_provider);
	void visit(JSONObject& object) override;
    void visit(JSONArray& array) override;
    void visit(JSONString& str) override;
    void visit(JSONInt& num) override;
    void visit(JSONFloat& num) override;
    void visit(JSONBool& boolean) override;

    std::vector<std::unique_ptr<DataStructure>> collect_ui_variables();
private:
	DefinitionProvider* m_def_provider;

	std::stack<std::pair<std::string, JSONValue*>> m_panel_prefixes;
	JSONValue* m_current_object = nullptr;
	JSONValue* m_current_panel_object = nullptr;
    std::string m_current_property;
    int m_current_control_idx;
    std::unordered_map<std::string, int> m_ui_controls;
//	const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &m_builtin_widgets;
//    std::vector<std::unique_ptr<NodeUIControl>> m_ui_variables;
};
