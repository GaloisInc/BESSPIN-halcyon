#include <algorithm>
#include <cstring>
#include <iostream>

#include <VeriConstVal.h>
#include <VeriExpression.h>
#include <VeriId.h>
#include <VeriMisc.h>

#include "structs.h"

void instr_t::describe_expression(VeriExpression* expr,
        id_desc_list_t& desc_list, uint8_t type_hint) {
    if (dynamic_cast<VeriDefaultBinValue*>(expr) != nullptr ||
            dynamic_cast<VeriForeachOperator*>(expr) != nullptr ||
            dynamic_cast<VeriTransRangeList*>(expr) != nullptr ||
            dynamic_cast<VeriTransSet*>(expr) != nullptr) {
        std::cerr << "no-catch expression: ";
        expr->PrettyPrintXml(std::cerr, 100);
        std::cerr << "\n";
    } else if (dynamic_cast<VeriSelectCondition*>(expr) ||
            dynamic_cast<VeriSolveBefore*>(expr) ||
            dynamic_cast<VeriTaggedUnion*>(expr)) {
        std::cerr << "systemverilog? : ";
        expr->PrettyPrintXml(std::cerr, 100);
        std::cerr << "\n";
    } else if (auto port = dynamic_cast<VeriAnsiPortDecl*>(expr)) {
        uint32_t idx = 0;
        VeriIdDef* id_def = nullptr;

        FOREACH_ARRAY_ITEM(port->GetIds(), idx, id_def) {
            describe_expression(id_def->GetActualName(), desc_list, type_hint);
        }
    } else if (auto bin_op = dynamic_cast<VeriBinaryOperator*>(expr)) {
        describe_expression(bin_op->GetLeft(), desc_list, type_hint);
        describe_expression(bin_op->GetRight(), desc_list, type_hint);
    } else if (auto case_op = dynamic_cast<VeriCaseOperator*>(expr)) {
        describe_expression(case_op->GetCondition(), desc_list, STATE_USE);

        uint32_t idx = 0;
        VeriCaseOperatorItem* case_op_item = nullptr;

        FOREACH_ARRAY_ITEM(case_op->GetCaseItems(), idx, case_op_item) {
            VeriExpression* property_expr = case_op_item->GetPropertyExpr();
            describe_expression(property_expr, desc_list, type_hint);

            uint32_t idx = 0;
            VeriExpression* condition = nullptr;

            FOREACH_ARRAY_ITEM(case_op_item->GetConditions(), idx, condition) {
                describe_expression(condition, desc_list, STATE_USE);
            }
        }
    } else if (auto cast_op = dynamic_cast<VeriCast*>(expr)) {
        describe_expression(cast_op->GetExpr(), desc_list, type_hint);
    } else if (auto concat = dynamic_cast<VeriConcat*>(expr)) {
        uint32_t idx = 0;
        VeriConcatItem* concat_item = nullptr;

        FOREACH_ARRAY_ITEM(concat->GetExpressions(), idx, concat_item) {
            describe_expression(concat_item, desc_list, type_hint);
        }
    } else if (auto concat_item = dynamic_cast<VeriConcatItem*>(expr)) {
        describe_expression(concat_item->GetExpr(), desc_list, type_hint);
    } else if (auto cond_pred = dynamic_cast<VeriCondPredicate*>(expr)) {
        describe_expression(cond_pred->GetLeft(), desc_list, type_hint);
        describe_expression(cond_pred->GetRight(), desc_list, type_hint);
    } else if (dynamic_cast<VeriConst*>(expr) != nullptr ||
            dynamic_cast<VeriDataType*>(expr) != nullptr ||
            dynamic_cast<VeriDollar*>(expr) != nullptr ||
            dynamic_cast<VeriDotStar*>(expr) != nullptr ||
            dynamic_cast<VeriNull*>(expr) != nullptr ||
            dynamic_cast<VeriPortOpen*>(expr) != nullptr) {
        ;
    } else if (auto constraint_set = dynamic_cast<VeriConstraintSet*>(expr)) {
        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(constraint_set->GetExpressions(), idx, expression) {
            describe_expression(expression, desc_list, type_hint);
        }
    } else if (auto delay_ctrl = dynamic_cast<VeriDelayOrEventControl*>(expr)) {
        describe_expression(delay_ctrl->GetDelayControl(), desc_list, STATE_USE);

        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(delay_ctrl->GetEventControl(), idx, expression) {
            describe_expression(expression, desc_list, STATE_USE);
        }

        describe_expression(delay_ctrl->GetRepeatEvent(), desc_list, type_hint);
    } else if (auto dot_name = dynamic_cast<VeriDotName*>(expr)) {
        describe_expression(dot_name->GetVarName(), desc_list, type_hint);
    } else if (auto event_expr = dynamic_cast<VeriEventExpression*>(expr)) {
        describe_expression(event_expr->GetIffCondition(), desc_list, STATE_USE);
        describe_expression(event_expr->GetExpr(), desc_list, type_hint);
    } else if (auto indexed_expr = dynamic_cast<VeriIndexedExpr*>(expr)) {
        describe_expression(indexed_expr->GetPrefixExpr(), desc_list, type_hint);
        describe_expression(indexed_expr->GetIndexExpr(), desc_list, STATE_USE);
    } else if (auto min_typ_max = dynamic_cast<VeriMinTypMaxExpr*>(expr)) {
        describe_expression(min_typ_max->GetMinExpr(), desc_list, type_hint);
        describe_expression(min_typ_max->GetTypExpr(), desc_list, type_hint);
        describe_expression(min_typ_max->GetMaxExpr(), desc_list, type_hint);
    } else if (auto multi_concat = dynamic_cast<VeriMultiConcat*>(expr)) {
        describe_expression(multi_concat->GetRepeat(), desc_list, STATE_USE);

        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(multi_concat->GetExpressions(), idx, expression) {
            describe_expression(expression, desc_list, type_hint);
        }
    } else if (dynamic_cast<VeriFunctionCall*>(expr) != nullptr) {
        // handle later when we match the call with the function definition.
    } else if (auto id_ref = dynamic_cast<VeriIdRef*>(expr)) {
        id_desc_t desc = { id_ref->GetName(), type_hint };
        desc_list.push_back(desc);
    } else if (auto indexed_id = dynamic_cast<VeriIndexedId*>(expr)) {
        id_desc_t desc = { indexed_id->GetName(), type_hint };
        desc_list.push_back(desc);

        describe_expression(indexed_id->GetIndexExpr(), desc_list, STATE_USE);
    } else if (auto idx_mem_id = dynamic_cast<VeriIndexedMemoryId*>(expr)) {
        id_desc_t desc = { idx_mem_id->GetName(), type_hint };
        desc_list.push_back(desc);

        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(idx_mem_id->GetIndexes(), idx, expression) {
            describe_expression(expression, desc_list, STATE_USE);
        }
    } else if (auto selected_name = dynamic_cast<VeriSelectedName*>(expr)) {
        id_desc_t desc = { selected_name->GetPrefix()->GetName(), type_hint };
        desc_list.push_back(desc);

        desc = { selected_name->GetSuffix(), type_hint };
        desc_list.push_back(desc);
    } else if (auto new_expr = dynamic_cast<VeriNew*>(expr)) {
        describe_expression(new_expr->GetSizeExpr(), desc_list, type_hint);

        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(new_expr->GetArgs(), idx, expression) {
            describe_expression(expression, desc_list, STATE_USE);
        }
    } else if (auto path_pulse = dynamic_cast<VeriPathPulseVal*>(expr)) {
        describe_expression(path_pulse->GetRejectLimit(), desc_list, type_hint);
        describe_expression(path_pulse->GetErrorLimit(), desc_list, type_hint);
    } else if (auto pattern_match = dynamic_cast<VeriPatternMatch*>(expr)) {
        describe_expression(pattern_match->GetLeft(), desc_list, type_hint);
        describe_expression(pattern_match->GetRight(), desc_list, type_hint);
    } else if (auto port_conn = dynamic_cast<VeriPortConnect*>(expr)) {
        id_desc_t desc = { port_conn->GetNamedFormal(), type_hint };
        desc_list.push_back(desc);

        describe_expression(port_conn->GetConnection(), desc_list, type_hint);
    } else if (auto question_colon = dynamic_cast<VeriQuestionColon*>(expr)) {
        describe_expression(question_colon->GetIfExpr(), desc_list, STATE_USE);
        describe_expression(question_colon->GetThenExpr(), desc_list, STATE_USE);
        describe_expression(question_colon->GetElseExpr(), desc_list, STATE_USE);
    } else if (auto range = dynamic_cast<VeriRange*>(expr)) {
        describe_expression(range->GetLeft(), desc_list, type_hint);
        describe_expression(range->GetRight(), desc_list, type_hint);
    } else if (auto sysfunc = dynamic_cast<VeriSystemFunctionCall*>(expr)) {
        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(sysfunc->GetArgs(), idx, expression) {
            describe_expression(expression, desc_list, STATE_USE);
        }
    } else if (auto timing_check = dynamic_cast<VeriTimingCheckEvent*>(expr)) {
        describe_expression(timing_check->GetCondition(), desc_list, STATE_USE);
        describe_expression(timing_check->GetTerminalDesc(), desc_list, type_hint);
    } else if (auto unary_op = dynamic_cast<VeriUnaryOperator*>(expr)) {
        describe_expression(unary_op->GetArg(), desc_list, type_hint);
    } else if (auto with_expr = dynamic_cast<VeriWith*>(expr)) {
        describe_expression(with_expr->GetLeft(), desc_list, type_hint);
    } else {
        std::cerr << "unhandled expression: ";
        expr->PrettyPrintXml(std::cerr, 100);
        std::cerr << "\n";
    }
}

void instr_t::parse_expression(VeriExpression* expr, uint8_t dst_set) {
    id_desc_list_t desc_list;
    describe_expression(expr, desc_list, dst_set);

    for (id_desc_t desc : desc_list) {
        if (desc.type == STATE_DEF) {
            def_set.insert(desc.name);
        } else if (desc.type == STATE_USE) {
            use_set.insert(desc.name);
        } else {
            std::cerr << "[u] " << desc.name << "\n";
            assert(false && "invalid destination!");
        }
    }
}

void instr_t::parse_statement(VeriStatement* stmt) {
    assert(stmt != nullptr && "null statement!");

    if (auto assign = dynamic_cast<VeriAssign*>(stmt)) {
        parse_expression(assign->GetAssign()->GetLValExpr(), STATE_DEF);
        parse_expression(assign->GetAssign()->GetRValExpr(), STATE_USE);
    } else if (auto blocking = dynamic_cast<VeriBlockingAssign*>(stmt)) {
        parse_expression(blocking->GetLVal(), STATE_DEF);
        parse_expression(blocking->GetValue(), STATE_USE);
    } else if (auto case_stmt = dynamic_cast<VeriCaseStatement*>(stmt)) {
        parse_expression(case_stmt->GetCondition(), STATE_USE);

        uint32_t idx = 0;
        VeriCaseItem* case_item = nullptr;

        FOREACH_ARRAY_ITEM(case_stmt->GetCaseItems(), idx, case_item) {
            uint32_t idx = 0;
            VeriExpression* expression = nullptr;

            FOREACH_ARRAY_ITEM(case_item->GetConditions(), idx, expression) {
                parse_expression(expression, STATE_USE);
            }

            VeriStatement* inner_statement = case_item->GetStmt();
            parse_statement(inner_statement);
        }
    } else if (auto de_assign = dynamic_cast<VeriDeAssign*>(stmt)) {
        parse_expression(de_assign->GetLVal(), STATE_DEF);
    } else if (auto disable = dynamic_cast<VeriDisable*>(stmt)) {
        ;
    } else if (auto event_trigger = dynamic_cast<VeriEventTrigger*>(stmt)) {
        parse_expression(event_trigger->GetControl(), STATE_USE);
        parse_expression(event_trigger->GetEventName(), STATE_DEF);
    } else if (auto nonblocking = dynamic_cast<VeriNonBlockingAssign*>(stmt)) {
        parse_expression(nonblocking->GetLValue(), STATE_DEF);
        parse_expression(nonblocking->GetValue(), STATE_USE);
    } else if (auto wait = dynamic_cast<VeriWait*>(stmt)) {
        parse_expression(wait->GetCondition(), STATE_USE);
        parse_statement(wait->GetStmt());
    } else {
        stmt->PrettyPrintXml(std::cerr, 100);
        assert(false && "Unhandled instruction!");
    }
}

id_set_t& instr_t::defs() {
    return def_set;
}

id_set_t& instr_t::uses() {
    return use_set;
}

arg_t::arg_t(identifier_t name, state_t state) {
    arg_name = name;
    arg_state = state;

    if (state & STATE_DEF) {
        def_set.insert(arg_name);
    }

    if (state & STATE_USE) {
        use_set.insert(arg_name);
    }
}

void arg_t::dump() {
    std::cerr << "arg: " << arg_name << " [ " << arg_state << " ]";
}

bool arg_t::operator==(const instr_t& reference) {
    if (const arg_t* ref = dynamic_cast<const arg_t*>(&reference)) {
        return arg_name == ref->arg_name && arg_state == ref->arg_state;
    }

    return false;
}

param_t::param_t(identifier_t name) {
    param_name = name;
    def_set.insert(param_name);
}

void param_t::dump() {
    std::cerr << "param: " << param_name;
}

bool param_t::operator==(const instr_t& reference) {
    if (const param_t* ref = dynamic_cast<const param_t*>(&reference)) {
        return param_name == ref->param_name;
    }

    return false;
}

stmt_t::stmt_t(VeriStatement* __stmt) {
    stmt = __stmt;
    parse_statement(stmt);
}

VeriStatement* stmt_t::statement() {
    return stmt;
}

void stmt_t::dump() {
    stmt->PrettyPrint(std::cerr, 100);
}

bool stmt_t::operator==(const instr_t& reference) {
    if (const stmt_t* ref = dynamic_cast<const stmt_t*>(&reference)) {
        return stmt == ref->stmt;
    }

    return false;
}

assign_t::assign_t(VeriNetRegAssign* __assign) {
    assign = __assign;

    parse_expression(assign->GetLValExpr(), STATE_DEF);
    parse_expression(assign->GetRValExpr(), STATE_USE);
}

VeriNetRegAssign* assign_t::assignment() {
    return assign;
}

void assign_t::dump() {
    assign->PrettyPrint(std::cerr, 100);
}

bool assign_t::operator==(const instr_t& reference) {
    if (const assign_t* ref = dynamic_cast<const assign_t*>(&reference)) {
        return assign == ref->assign;
    }

    return false;
}

invoke_t::invoke_t(VeriInstId* __mod_inst, identifier_t __mod_name) {
    mod_inst = __mod_inst;
    mod_name = __mod_name;

    parse_invocation();
}

bool invoke_t::operator==(const instr_t& reference) {
    if (const invoke_t* ref = dynamic_cast<const invoke_t*>(&reference)) {
        return mod_inst == ref->mod_inst;
    }

    return false;
}

void invoke_t::parse_invocation() {
    const char* instance_name = mod_inst->InstName();

    uint32_t idx = 0;
    VeriPortConnect* connect = nullptr;

    FOREACH_ARRAY_ITEM(mod_inst->GetPortConnects(), idx, connect) {
        id_desc_list_t desc_list;
        describe_expression(connect->GetConnection(), desc_list, STATE_USE);

        conn_t connection;
        connection.remote_endpoint = connect->GetNamedFormal();

        for (id_desc_t desc : desc_list) {
            connection.id_set.insert(desc.name);
        }

        conns.push_back(connection);
    }
}

void invoke_t::dump() {
    std::cerr << "remote module name: " << mod_name << "\n";
    mod_inst->PrettyPrint(std::cerr, 100);
}

identifier_t invoke_t::module_name() {
    return mod_name;
}

conn_list_t& invoke_t::connections() {
    return conns;
}

cmpr_t::cmpr_t(VeriExpression* __cmpr) {
    cmpr = __cmpr;
    parse_expression(cmpr, STATE_USE);
}

VeriExpression* cmpr_t::comparison() {
    return cmpr;
}

void cmpr_t::dump() {
    cmpr->PrettyPrint(std::cerr, 100);
}

bool cmpr_t::operator==(const instr_t& reference) {
    if (const cmpr_t* ref = dynamic_cast<const cmpr_t*>(&reference)) {
        return cmpr == ref->cmpr;
    }

    return false;
}

bb_t::bb_t(const std::string& __name) {
    name = __name;

    successor_left = nullptr;
    successor_right = nullptr;
}

bb_t::~bb_t() {
    successor_left = nullptr;
    successor_right = nullptr;

    for (instr_t* instr : instr_list) {
        delete instr;
        instr = nullptr;
    }

    instr_list.clear();
    predecessors.clear();
}

bool bb_t::add_predecessor(bb_t* predecessor) {
    if (predecessor == nullptr) {
        assert(false && "attempting to insert a null predecessor!");
        return false;
    }

    if (predecessors.find(predecessor) != predecessors.end()) {
        assert(false && "attempting to insert a duplicate predecessor!");
        return false;
    }

    predecessors.insert(predecessor);
    return true;
}

bool bb_t::exists(instr_t* instr) {
    return std::find(instr_list.begin(), instr_list.end(), instr) !=
        instr_list.end();
}

bool bb_t::append(instr_t* new_instr) {
    if (exists(new_instr) == true) {
        assert(false && "instruction already exists or is a comparison!");
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
    successor->add_predecessor(this);

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
    successor->add_predecessor(this);

    return true;
}

bb_t* bb_t::left_successor() {
    return successor_left;
}

bb_t* bb_t::right_successor() {
    return successor_right;
}

instr_list_t& bb_t::instrs() {
    return instr_list;
}

bb_set_t& bb_t::preds() {
    return predecessors;
}

uint64_t bb_t::pred_count() {
    return predecessors.size();
}

uint64_t bb_t::succ_count() {
    return (successor_left != nullptr) + (successor_right != nullptr);
}

void bb_t::dump() {
    if (predecessors.size() == 0) {
        std::cerr << "-T- ";
    } else {
        std::cerr << "    ";
    }

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

module_t::module_t(const std::string& name) {
    mod_name = name;
}

module_t::~module_t() {
    for (bb_t* bb : basicblocks) {
        delete bb;
        bb = nullptr;
    }

    basicblocks.clear();
}

identifier_t module_t::name() {
    return mod_name;
}

bb_t* module_t::create_empty_basicblock(identifier_t name) {
    // create key if necessary.
    uint32_t counter = bb_id_map[name];
    bb_id_map[name] = counter + 1;

    char bb_name[1024];
    snprintf(bb_name, sizeof(bb_name), "bb.%s.%u", name.c_str(), counter);

    bb_t* new_block = new bb_t(std::string(bb_name));
    basicblocks.push_back(new_block);

    return new_block;
}

bool module_t::append(instr_t* instr) {
    bb_t* new_bb = create_empty_basicblock("lone");
    return new_bb->append(instr);
}

void module_t::dump() {
    for (bb_t* bb : basicblocks) {
        bb->dump();
    }
}

uint64_t module_t::build_reachable_set(bb_t*& start_bb, bb_set_t& reachable) {
    bb_set_t workset;
    reachable.clear();

    workset.insert(start_bb);
    reachable.insert(start_bb);

    do {
        bb_set_t::iterator it = workset.begin();
        bb_t* work_item = *it;
        workset.erase(it);

        bb_t* left = work_item->left_successor();
        bb_t* right = work_item->right_successor();

        if (left != nullptr && reachable.find(left) == reachable.end()) {
            workset.insert(left);
            reachable.insert(left);
        }

        if (right != nullptr && reachable.find(right) == reachable.end()) {
            workset.insert(right);
            reachable.insert(right);
        }
    } while (workset.size() > 0);

    return reachable.size();
}

void module_t::intersect(bb_set_t& dst_set, bb_set_t& src_set) {
    bb_set_t::iterator src_it = src_set.begin();
    bb_set_t::iterator dst_it = dst_set.begin();

    bb_set_t::iterator src_end = src_set.end();
    bb_set_t::iterator dst_end = dst_set.end();

    while (dst_it != dst_end && src_it != src_end) {
        if (*dst_it < *src_it) {
            dst_set.erase(dst_it++);
        } else if (*src_it < *dst_it) {
            ++src_it;
        } else {
            ++dst_it;
            ++src_it;
        }
    }

    dst_set.erase(dst_it, dst_set.end());
}

bool module_t::update_dominators(bb_t* focus_bb, bb_set_t& reachable) {
    bb_set_t new_dominators;

    // initialize to all basic blocks to prepare for subsequent intersection.
    new_dominators.insert(reachable.begin(), reachable.end());

    for (bb_t* pred : focus_bb->preds()) {
        bb_set_t& dom_of_pred = dominators[pred];

        intersect(new_dominators, dom_of_pred);
        new_dominators.insert(focus_bb);
    }

    bb_set_t& dom_of_focus_bb = dominators[focus_bb];

    if (dom_of_focus_bb != new_dominators) {
        dominators[focus_bb] = new_dominators;
        return true;
    }

    return false;
}

bool module_t::update_postdominators(bb_t* focus_bb, bb_set_t& reachable) {
    bb_set_t new_postdominators;

    // initialize to all basic blocks to prepare for subsequent intersection.
    new_postdominators.insert(reachable.begin(), reachable.end());

    bb_t* left = focus_bb->left_successor();
    bb_set_t& pdom_of_left_succ = postdominators[left];
    intersect(new_postdominators, pdom_of_left_succ);

    bb_t* right = focus_bb->right_successor();

    if (right != nullptr) {
        bb_set_t& pdom_of_right_succ = postdominators[right];
        intersect(new_postdominators, pdom_of_right_succ);
    }

    new_postdominators.insert(focus_bb);

    bb_set_t& pdom_of_focus_bb = postdominators[focus_bb];

    if (pdom_of_focus_bb != new_postdominators) {
        postdominators[focus_bb] = new_postdominators;
        return true;
    }

    return false;
}

bb_t* module_t::find_imm_dominator(bb_t* start_bb, bb_set_t& dom_set) {
    bb_t* imm_dominator = nullptr;
    dom_set.erase(start_bb);

    for (bb_t* bb1 : dom_set) {
        bool outside_dom_set = true;

        for (bb_t* bb2 : dom_set) {

            if (bb1 != bb2) {
                bb_set_t& bb2_dom_set = dominators[bb2];

                if (bb2_dom_set.find(bb1) != bb2_dom_set.end()) {
                    outside_dom_set = false;
                    break;
                }
            }
        }

        if (outside_dom_set == true) {
            if (imm_dominator == nullptr) {
                imm_dominator = bb1;
            } else {
                assert(false && "found multiple immediate dominators!");
                imm_dominator = nullptr;

                break;
            }
        }
    }

    dom_set.insert(start_bb);
    return imm_dominator;
}

bb_t* module_t::find_imm_postdominator(bb_t* start_bb,
        bb_set_t& pdom_set) {
    bb_t* imm_postdominator = nullptr;
    pdom_set.erase(start_bb);

    for (bb_t* bb1 : pdom_set) {
        bool outside_pdom_set = true;

        for (bb_t* bb2 : pdom_set) {
            if (bb1 != bb2) {
                bb_set_t& bb2_pdom_set = postdominators[bb2];

                if (bb2_pdom_set.find(bb1) != bb2_pdom_set.end()) {
                    outside_pdom_set = false;
                    break;
                }
            }
        }

        if (outside_pdom_set == true) {
            if (imm_postdominator == nullptr) {
                imm_postdominator = bb1;
            } else {
                assert(false && "found multiple immediate postdominators!");
                imm_postdominator = nullptr;

                break;
            }
        }
    }

    pdom_set.insert(start_bb);
    return imm_postdominator;
}

void module_t::build_dominator_sets(bb_set_t& reachable) {
    bb_set_t empty_set;

    // initialize dominator objects for blocks in the reachable set.
    for (bb_t* bb : reachable) {
        dominators.emplace(bb, empty_set);
        postdominators.emplace(bb, empty_set);

        if (bb->pred_count() == 0) {
            dominators[bb].insert(bb);
        } else {
            dominators[bb].insert(reachable.begin(), reachable.end());
        }

        if (bb->succ_count() == 0) {
            postdominators[bb].insert(bb);
        } else {
            postdominators[bb].insert(reachable.begin(), reachable.end());
        }
    }

    bool change = false;

    do {
        change = false;

        for (bb_t* bb : reachable) {
            if (bb->pred_count() > 0 && update_dominators(bb, reachable)) {
                change = true;
            }

            if (bb->succ_count() > 0 && update_postdominators(bb, reachable)) {
                change = true;
            }
        }
    } while (change == true);

    for (bb_t* bb : reachable) {
        bb_set_t& dom_set = dominators[bb];
        imm_dominator[bb] = find_imm_dominator(bb, dom_set);

        bb_set_t& pdom_set = postdominators[bb];
        imm_postdominator[bb] = find_imm_postdominator(bb, pdom_set);
    }
}

void module_t::build_dominator_sets() {
    // TODO: For simplicity, we use the O(V^2) algorithm.  The Lengauer-Tarjan
    // Dominator Tree Algorithm will likely improve the analysis performance.

    for (bb_t* bb : basicblocks) {
        if (bb->pred_count() == 0) {
            bb_set_t reachable;

            build_reachable_set(bb, reachable);
            build_dominator_sets(reachable);
        }
    }
}

void module_t::build_def_use_chains() {
    for (bb_t* bb : basicblocks) {
        for (instr_t* instr : bb->instrs()) {
            for (identifier_t id : instr->defs()) {
                def_map[id].insert(instr);
            }

            for (identifier_t id : instr->uses()) {
                use_map[id].insert(instr);
            }
        }
    }
}

bool module_t::process_connection(conn_t& connection, invoke_t* invocation) {
    if (connection.state == STATE_UNKNOWN) {
        // std::cerr << "unknown state: " << connection.remote_endpoint << "\n";
        return false;
    }

    // IMPORTANT: When processing connections, we flip the def and use flags.

    if (connection.state & STATE_USE) {
        if (connection.id_set.size() != 1) {
            assert(false && "simultaneous definition of more than one id!");
            return false;
        }

        id_set_t::iterator it = connection.id_set.begin();
        const identifier_t& id = *it;

        def_map[id].insert(invocation);
    }
    
    if (connection.state & STATE_DEF) {
        for (const identifier_t& id : connection.id_set) {
            use_map[id].insert(invocation);
        }
    }

    return true;
}

void module_t::resolve_invoke(invoke_t* invocation, module_map_t& module_map) {
    identifier_t mod_name = invocation->module_name();
    module_map_t::iterator it = module_map.find(mod_name);

    if (it == module_map.end()) {
        std::cerr << "referred module: " << mod_name << "\n";
        assert(false && "found reference to undefined module!");
        return;
    }

    module_t* module = it->second;

    for (conn_t& connection : invocation->connections()) {
        identifier_t end_id = connection.remote_endpoint;
        connection.state = module->arg_state(end_id);
        process_connection(connection, invocation);
    }
}

void module_t::resolve_links(module_map_t& module_map) {
    for (bb_t* bb : basicblocks) {
        for (instr_t* instr : bb->instrs()) {
            if (invoke_t* invocation = dynamic_cast<invoke_t*>(instr)) {
                resolve_invoke(invocation, module_map);
            } /* FIXME: else if (stmt_t* stmt = dynamic_cast<stmt_t*>(instr)) {
                VeriStatement* statement = stmt->statement();

                if (auto fcall = dynamic_cast<VeriFunctionCall*>(statement)) {
                    fcall->PrettyPrintXml(std::cerr, 100);
                }
            } */
        }
    }
}

void module_t::print_undef_ids() {
    if (is_function == true) {
        return;
    }

    id_set_t undef_ids;

    for (id_map_t::iterator it = use_map.begin(); it != use_map.end(); it++) {
        identifier_t id = it->first;
        id_map_t::iterator def_it = def_map.find(id);

        if (def_it == def_map.end()) {
            undef_ids.insert(id);
        }
    }

    if (undef_ids.size() > 0) {
        std::cerr << "undefined identifiers in module " << mod_name << ":\n";

        for (identifier_t id : undef_ids) {
            std::cerr << "  " << id << "\n";
        }
    }
}

void module_t::add_arg(identifier_t name, uint8_t state) {
    arg_states.emplace(name, state);
}

void module_t::update_arg(identifier_t name, uint8_t state) {
    arg_states[name] = state;
}

uint8_t module_t::arg_state(identifier_t name) {
    id_state_map_t::iterator it = arg_states.find(name);

    if (it == arg_states.end()) {
        return STATE_UNKNOWN;
    }

    return it->second;
}

void module_t::set_function() {
    is_function = true;
}
