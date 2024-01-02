//
// Created by Mathias Vatter on 02.01.24.
//

#pragma once

#include "JSONParser.h"

class JSONVisitor {
public:
    virtual void visit(JSONObject& object) = 0;
    virtual void visit(JSONArray& array) = 0;
    virtual void visit(JSONString& str) = 0;
    virtual void visit(JSONInt& num) = 0;
    virtual void visit(JSONFloat& num) = 0;
    virtual void visit(JSONBool& boolean) = 0;

};


class NCKPTranslator : public JSONVisitor {
public:
    void visit(JSONObject& object) override;
    void visit(JSONArray& array) override;
    void visit(JSONString& str) override;
    void visit(JSONInt& num) override;
    void visit(JSONFloat& num) override;
    void visit(JSONBool& boolean) override;

private:
    std::string m_current_property;
    int m_current_control_idx;
    std::unordered_map<std::string, int> m_ui_controls;
};
