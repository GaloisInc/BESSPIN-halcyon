#include <algorithm>
#include <iostream>
#include <VeriExpression.h>

#include "structs.h"

bb_t::bb_t(const std::string& __name) {
    name = __name;

    condition = nullptr;
    successor_left = nullptr;
    successor_right = nullptr;
}

bb_t::~bb_t() {
    successor_left = nullptr;
    successor_right = nullptr;

    instr_list.clear();
    predecessors.clear();
}

bool bb_t::add_predecessor(bb_t*& predecessor) {
    if (predecessor == nullptr) {
        assert(false && "attempting to insert a null predecessor!");
        return false;
    }

    if (predecessors.find(predecessor) != predecessors.end()) {
        assert(false && "attempting to insert a duplicate predecessor!");
        return false;
    }

    predecessors.insert(predecessor);
}

instr_t bb_t::last_instr() {
    if (instr_list.size() == 0) {
        return nullptr;
    }

    instr_list_t::reverse_iterator it = instr_list.rbegin();
    return *it;
}

bool bb_t::exists(const instr_t& instr) {
    return std::find(instr_list.begin(), instr_list.end(), instr) !=
        instr_list.end();
}

bool bb_t::append(const expr_t& expr) {
    if (condition != nullptr) {
        assert(false && "attempting to overwrite condition expression!");
        return false;
    }

    if (expr == nullptr) {
        assert(false && "invalid condition expression!");
        return false;
    }

    condition = expr;
    return true;
}

bool bb_t::append(const instr_t& new_instr) {
    if (exists(new_instr) == true) {
        assert(false && "instruction already exists!");
        return false;
    }

    if (dynamic_cast<VeriConditionalStatement*>(new_instr)) {
        assert(false && "attempting to insert conditional statement!");
        return false;
    }

    instr_list.push_back(new_instr);
    return true;
}

bool bb_t::set_left_successor(bb_t*& successor) {
    if (successor == nullptr) {
        assert(false && "empty successor pointer!");
        return false;
    }

    if (successor_left != nullptr) {
        assert(false && "overwriting non-empty successor!");
        return false;
    }

    successor_left = successor;
    return true;
}

bool bb_t::set_right_successor(bb_t*& successor) {
    if (successor == nullptr) {
        assert(false && "empty successor pointer!");
        return false;
    }

    if (successor_right != nullptr) {
        assert(false && "overwriting non-empty successor!");
        return false;
    }

    successor_right = successor;
    return true;
}

void bb_t::dump(uint32_t level = 1) {
    std::cerr << "[" << name << "]  pointing to  ";

    if (successor_left != nullptr) {
        std::cerr << successor_left->name << " and ";
    } else {
        std::cerr << "-nil- and ";
    }

    if (successor_right != nullptr) {
        std::cerr << successor_right->name << "\n";
    } else {
        std::cerr << "-nil-\n";
    }
}

module_t::module_t(const std::string& __name) {
    name = __name;
}

module_t::~module_t() {
    for (bb_list_t::iterator it = basicblocks.begin(); it != basicblocks.end();
            it++) {
        bb_t*& bb = *it;

        delete bb;
        bb = nullptr;
    }

    basicblocks.clear();
    assignments.clear();
}

bb_t* module_t::create_empty_basicblock(const char* name) {
    // create key if necessary.
    uint32_t counter = bb_id_map[name];
    bb_id_map[name] = counter + 1;

    char bb_name[1024];
    snprintf(bb_name, sizeof(bb_name), "bb.%s.%u", name, counter);

    bb_t* new_block = new bb_t(std::string(bb_name));
    basicblocks.push_back(new_block);

    return new_block;
}

bool module_t::append_assignment(assign_t& assignment) {
    if (dynamic_cast<VeriContinuousAssign*>(assignment) == nullptr) {
        assert(false && "attempting to insert a non-continuous assignment!");
        return false;
    }

    assignments.push_back(assignment);
}

void module_t::dump() {
    for (bb_list_t::iterator it = basicblocks.begin(); it != basicblocks.end();
            it++) {
        bb_t*& bb = *it;
        bb->dump();
    }
}
