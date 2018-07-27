#include <algorithm>
#include <cstring>
#include <iostream>

#include <VeriConstVal.h>
#include <VeriExpression.h>
#include <VeriId.h>
#include <VeriMisc.h>
#include <VeriModule.h>
#include <veri_tokens.h>

#include "structs.h"

void balk(VeriTreeNode* tree_node, const std::string& msg) {
    std::cerr << msg << " [" << tree_node << "]:\n";
    tree_node->PrettyPrintXml(std::cerr, 100);

    assert(false && "unrecoverable error!");
}

instr_t::instr_t(bb_t* parent) {
    containing_bb = parent;
}

instr_t::~instr_t() {
    containing_bb = nullptr;
}

void instr_t::describe_expr(VeriExpression* expr, id_desc_list_t& desc_list,
        uint8_t type_hint) {
    if (expr == nullptr) {
        return;
    }

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
            describe_expr(id_def->GetActualName(), desc_list, type_hint);
        }
    } else if (auto bin_op = dynamic_cast<VeriBinaryOperator*>(expr)) {
        describe_expr(bin_op->GetLeft(), desc_list, type_hint);
        describe_expr(bin_op->GetRight(), desc_list, type_hint);
    } else if (auto case_op = dynamic_cast<VeriCaseOperator*>(expr)) {
        describe_expr(case_op->GetCondition(), desc_list, STATE_USE);

        uint32_t idx = 0;
        VeriCaseOperatorItem* case_op_item = nullptr;

        FOREACH_ARRAY_ITEM(case_op->GetCaseItems(), idx, case_op_item) {
            VeriExpression* property_expr = case_op_item->GetPropertyExpr();
            describe_expr(property_expr, desc_list, type_hint);

            uint32_t idx = 0;
            VeriExpression* condition = nullptr;

            FOREACH_ARRAY_ITEM(case_op_item->GetConditions(), idx, condition) {
                describe_expr(condition, desc_list, STATE_USE);
            }
        }
    } else if (auto cast_op = dynamic_cast<VeriCast*>(expr)) {
        describe_expr(cast_op->GetExpr(), desc_list, type_hint);
    } else if (auto concat = dynamic_cast<VeriConcat*>(expr)) {
        uint32_t idx = 0;
        VeriConcatItem* concat_item = nullptr;

        FOREACH_ARRAY_ITEM(concat->GetExpressions(), idx, concat_item) {
            describe_expr(concat_item, desc_list, type_hint);
        }
    } else if (auto concat_item = dynamic_cast<VeriConcatItem*>(expr)) {
        describe_expr(concat_item->GetExpr(), desc_list, type_hint);
    } else if (auto cond_pred = dynamic_cast<VeriCondPredicate*>(expr)) {
        describe_expr(cond_pred->GetLeft(), desc_list, type_hint);
        describe_expr(cond_pred->GetRight(), desc_list, type_hint);
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
            describe_expr(expression, desc_list, type_hint);
        }
    } else if (auto delay_ctrl = dynamic_cast<VeriDelayOrEventControl*>(expr)) {
        describe_expr(delay_ctrl->GetDelayControl(), desc_list, STATE_USE);

        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(delay_ctrl->GetEventControl(), idx, expression) {
            describe_expr(expression, desc_list, STATE_USE);
        }

        describe_expr(delay_ctrl->GetRepeatEvent(), desc_list, type_hint);
    } else if (auto dot_name = dynamic_cast<VeriDotName*>(expr)) {
        describe_expr(dot_name->GetVarName(), desc_list, type_hint);
    } else if (auto event_expr = dynamic_cast<VeriEventExpression*>(expr)) {
        describe_expr(event_expr->GetIffCondition(), desc_list, STATE_USE);
        describe_expr(event_expr->GetExpr(), desc_list, type_hint);
    } else if (auto indexed_expr = dynamic_cast<VeriIndexedExpr*>(expr)) {
        describe_expr(indexed_expr->GetPrefixExpr(), desc_list, type_hint);
        describe_expr(indexed_expr->GetIndexExpr(), desc_list, STATE_USE);
    } else if (auto min_typ_max = dynamic_cast<VeriMinTypMaxExpr*>(expr)) {
        describe_expr(min_typ_max->GetMinExpr(), desc_list, type_hint);
        describe_expr(min_typ_max->GetTypExpr(), desc_list, type_hint);
        describe_expr(min_typ_max->GetMaxExpr(), desc_list, type_hint);
    } else if (auto multi_concat = dynamic_cast<VeriMultiConcat*>(expr)) {
        describe_expr(multi_concat->GetRepeat(), desc_list, STATE_USE);

        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(multi_concat->GetExpressions(), idx, expression) {
            describe_expr(expression, desc_list, type_hint);
        }
    } else if (dynamic_cast<VeriFunctionCall*>(expr) != nullptr) {
        // handle later when we match the call with the function definition.
    } else if (auto id_ref = dynamic_cast<VeriIdRef*>(expr)) {
        id_desc_t desc = { id_ref->GetName(), type_hint };
        desc_list.push_back(desc);
    } else if (auto indexed_id = dynamic_cast<VeriIndexedId*>(expr)) {
        id_desc_t desc = { indexed_id->GetName(), type_hint };
        desc_list.push_back(desc);

        describe_expr(indexed_id->GetIndexExpr(), desc_list, STATE_USE);
    } else if (auto idx_mem_id = dynamic_cast<VeriIndexedMemoryId*>(expr)) {
        id_desc_t desc = { idx_mem_id->GetName(), type_hint };
        desc_list.push_back(desc);

        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(idx_mem_id->GetIndexes(), idx, expression) {
            describe_expr(expression, desc_list, STATE_USE);
        }
    } else if (auto selected_name = dynamic_cast<VeriSelectedName*>(expr)) {
        id_desc_t desc = { selected_name->GetPrefix()->GetName(), type_hint };
        desc_list.push_back(desc);

        desc = { selected_name->GetSuffix(), type_hint };
        desc_list.push_back(desc);
    } else if (auto new_expr = dynamic_cast<VeriNew*>(expr)) {
        describe_expr(new_expr->GetSizeExpr(), desc_list, type_hint);

        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(new_expr->GetArgs(), idx, expression) {
            describe_expr(expression, desc_list, STATE_USE);
        }
    } else if (auto path_pulse = dynamic_cast<VeriPathPulseVal*>(expr)) {
        describe_expr(path_pulse->GetRejectLimit(), desc_list, type_hint);
        describe_expr(path_pulse->GetErrorLimit(), desc_list, type_hint);
    } else if (auto pattern_match = dynamic_cast<VeriPatternMatch*>(expr)) {
        describe_expr(pattern_match->GetLeft(), desc_list, type_hint);
        describe_expr(pattern_match->GetRight(), desc_list, type_hint);
    } else if (auto port_conn = dynamic_cast<VeriPortConnect*>(expr)) {
        id_desc_t desc = { port_conn->GetNamedFormal(), type_hint };
        desc_list.push_back(desc);

        describe_expr(port_conn->GetConnection(), desc_list, type_hint);
    } else if (auto question_colon = dynamic_cast<VeriQuestionColon*>(expr)) {
        describe_expr(question_colon->GetIfExpr(), desc_list, STATE_USE);
        describe_expr(question_colon->GetThenExpr(), desc_list, STATE_USE);
        describe_expr(question_colon->GetElseExpr(), desc_list, STATE_USE);
    } else if (auto range = dynamic_cast<VeriRange*>(expr)) {
        describe_expr(range->GetLeft(), desc_list, type_hint);
        describe_expr(range->GetRight(), desc_list, type_hint);
    } else if (auto sysfunc = dynamic_cast<VeriSystemFunctionCall*>(expr)) {
        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(sysfunc->GetArgs(), idx, expression) {
            describe_expr(expression, desc_list, STATE_USE);
        }
    } else if (auto timing_check = dynamic_cast<VeriTimingCheckEvent*>(expr)) {
        describe_expr(timing_check->GetCondition(), desc_list, STATE_USE);
        describe_expr(timing_check->GetTerminalDesc(), desc_list, type_hint);
    } else if (auto unary_op = dynamic_cast<VeriUnaryOperator*>(expr)) {
        describe_expr(unary_op->GetArg(), desc_list, type_hint);
    } else if (auto with_expr = dynamic_cast<VeriWith*>(expr)) {
        describe_expr(with_expr->GetLeft(), desc_list, type_hint);
    } else {
        std::cerr << "unhandled expression: ";
        expr->PrettyPrintXml(std::cerr, 100);
        std::cerr << "\n";
    }
}

void instr_t::parse_expression(VeriExpression* expr, uint8_t dst_set) {
    id_desc_list_t desc_list;
    describe_expr(expr, desc_list, dst_set);

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

bb_t* instr_t::parent() {
    return containing_bb;
}

id_set_t& instr_t::defs() {
    return def_set;
}

id_set_t& instr_t::uses() {
    return use_set;
}

arg_t::arg_t(bb_t* parent, identifier_t name, state_t state) : instr_t(parent) {
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

param_t::param_t(bb_t* parent, identifier_t name) : instr_t(parent)  {
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

trigger_t::trigger_t(bb_t* parent, id_set_t& trigger_ids) : instr_t(parent)  {
    id_set = trigger_ids;
    def_set.insert(id_set.begin(), id_set.end());
}

void trigger_t::dump() {
    std::cerr << "trigger:";

    for (identifier_t id : id_set) {
        std::cerr << " " << id;
    }
}

id_set_t& trigger_t::trigger_ids() {
    return id_set;
}

bool trigger_t::operator==(const instr_t& reference) {
    if (const trigger_t* ref = dynamic_cast<const trigger_t*>(&reference)) {
        return id_set == ref->id_set;
    }

    return false;
}

stmt_t::stmt_t(bb_t* parent, VeriStatement* __stmt) : instr_t(parent)  {
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

assign_t::assign_t(bb_t* parent, VeriNetRegAssign* __assign)
        : instr_t(parent)  {
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

invoke_t::invoke_t(bb_t* parent, VeriInstId* __inst, identifier_t __name)
        : instr_t(parent)  {
    mod_inst = __inst;
    mod_name = __name;

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
        describe_expr(connect->GetConnection(), desc_list, STATE_USE);

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

cmpr_t::cmpr_t(bb_t* parent, VeriExpression* __cmpr) : instr_t(parent) {
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

bb_t::bb_t(module_t* parent, const identifier_t& __name, state_t __bb_type) {
    bb_name = __name;
    entry_bb = nullptr;
    bb_type = __bb_type;
    containing_module = parent;

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

    // The successor block is no longer a top-level basic block.
    containing_module->remove_from_top_level_blocks(successor);

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

    // The successor block is no longer a top-level basic block.
    containing_module->remove_from_top_level_blocks(successor);

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

    std::cerr << "[" << bb_name << "]  pointing to  ";

    if (successor_left != nullptr) {
        std::cerr << successor_left->bb_name << " and ";
    } else {
        std::cerr << "-nil- and ";
    }

    if (successor_right != nullptr) {
        std::cerr << successor_right->bb_name << "\n";
    } else {
        std::cerr << "-nil-\n";
    }
}

bb_t* bb_t::entry_block() {
    return entry_bb;
}

identifier_t bb_t::name() {
    return bb_name;
};

void bb_t::set_entry_block(bb_t* block) {
    entry_bb = block;
}

state_t bb_t::block_type() {
    return bb_type;
}

cmpr_t* bb_t::comparison() {
    if (instr_list.size() == 0) {
        return nullptr;
    }

    instr_t* last_instr = instr_list.back();
    cmpr_t* comparison = dynamic_cast<cmpr_t*>(last_instr);

    if (comparison == nullptr) {
        return nullptr;
    }

    return comparison;
}

module_t* bb_t::parent() {
    return containing_module;
}

module_t::module_t(VeriModule*& module) {
    mod_name = module->GetName();

    process_module_items(module->GetModuleItems());
    process_module_params(module->GetParameters());
    process_module_ports(module->GetPortConnects());
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

bb_t* module_t::create_empty_bb(identifier_t name, state_t bb_type) {
    // create key if necessary.
    uint32_t counter = bb_id_map[name];
    bb_id_map[name] = counter + 1;

    char bb_name[1024];
    snprintf(bb_name, sizeof(bb_name), "bb.%s.%u", name.c_str(), counter);

    bb_t* new_block = new bb_t(this, std::string(bb_name), bb_type);
    basicblocks.push_back(new_block);

    // Mark this as a top-level block for now,
    // but fix it if we make it a successor block.
    top_level_blocks.insert(new_block);

    return new_block;
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
    start_bb->set_entry_block(start_bb);

    do {
        bb_set_t::iterator it = workset.begin();
        bb_t* work_item = *it;
        workset.erase(it);

        bb_t* left = work_item->left_successor();
        bb_t* right = work_item->right_successor();

        if (left != nullptr && reachable.find(left) == reachable.end()) {
            workset.insert(left);
            reachable.insert(left);
            left->set_entry_block(start_bb);
        }

        if (right != nullptr && reachable.find(right) == reachable.end()) {
            workset.insert(right);
            reachable.insert(right);
            right->set_entry_block(start_bb);
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

    for (bb_t* bb : top_level_blocks) {
        assert(bb->pred_count() == 0 && "not a top-level block!");

        bb_set_t reachable;
        build_reachable_set(bb, reachable);
        build_dominator_sets(reachable);
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

void module_t::process_module_items(Array* module_items) {
    if (module_items == nullptr || module_items->Size() == 0) {
        return;
    }

    unsigned idx = 0;
    VeriModuleItem* module_item = nullptr;

    FOREACH_ARRAY_ITEM(module_items, idx, module_item) {
        process_module_item(module_item);
    }
}

void module_t::process_module_params(Array* params) {
    if (params == nullptr || params->Size() == 0) {
        return;
    }

    unsigned idx = 0;
    VeriIdDef* id_def = nullptr;

    bb_t* bb_params = create_empty_bb("params", BB_PARAMS);

    FOREACH_ARRAY_ITEM(params, idx, id_def) {
        bb_params->append(new param_t(bb_params, id_def->GetName()));
    }
}

void module_t::process_module_ports(Array* port_connects) {
    if (port_connects == nullptr || port_connects->Size() == 0) {
        return;
    }

    unsigned idx = 0;
    VeriAnsiPortDecl* decl = nullptr;
    bb_t* arg_bb = create_empty_bb("args", BB_ARGS);

    FOREACH_ARRAY_ITEM(port_connects, idx, decl) {
        uint8_t state = STATE_UNKNOWN;

        switch (decl->GetDir()) {
            case VERI_INPUT:    state = STATE_DEF;              break;
            case VERI_OUTPUT:   state = STATE_USE;              break;
            case VERI_INOUT:    state = STATE_DEF | STATE_USE;  break;
        }

        unsigned idx = 0;
        VeriIdDef* arg_id = nullptr;

        FOREACH_ARRAY_ITEM(decl->GetIds(), idx, arg_id) {
            identifier_t id = arg_id->GetName();

            if (state & STATE_USE) {
                out_ports.insert(id);
            }

            add_arg(id, state);
            arg_bb->append(new arg_t(arg_bb, id, state));
        }
    }
}

void module_t::process_module_item(VeriModuleItem* module_item) {
    if (dynamic_cast<VeriBindDirective*>(module_item) != nullptr ||
            dynamic_cast<VeriBinDecl*>(module_item) != nullptr ||
            dynamic_cast<VeriClass*>(module_item) != nullptr ||
            dynamic_cast<VeriClockingDecl*>(module_item) != nullptr ||
            dynamic_cast<VeriCovergroup*>(module_item) != nullptr ||
            dynamic_cast<VeriExportDecl*>(module_item) != nullptr ||
            dynamic_cast<VeriImportDecl*>(module_item) != nullptr ||
            dynamic_cast<VeriLetDecl*>(module_item) != nullptr ||
            dynamic_cast<VeriModport*>(module_item) != nullptr ||
            dynamic_cast<VeriModportDecl*>(module_item) != nullptr ||
            dynamic_cast<VeriNetAlias*>(module_item) != nullptr ||
            dynamic_cast<VeriOperatorBinding*>(module_item) != nullptr ||
            dynamic_cast<VeriPropertyDecl*>(module_item) != nullptr ||
            dynamic_cast<VeriSequenceDecl*>(module_item) != nullptr) {
        balk(module_item, "SystemVerilog node");
    } else if (dynamic_cast<VeriCoverageOption*>(module_item) != nullptr ||
            dynamic_cast<VeriCoverageSpec*>(module_item) != nullptr ||
            dynamic_cast<VeriDefaultDisableIff*>(module_item) != nullptr ||
            dynamic_cast<VeriGateInstantiation*>(module_item) != nullptr ||
            dynamic_cast<VeriGenerateBlock*>(module_item) != nullptr ||
            dynamic_cast<VeriGenerateCase*>(module_item) != nullptr ||
            dynamic_cast<VeriGenerateConditional*>(module_item) != nullptr ||
            dynamic_cast<VeriGenerateConstruct*>(module_item) != nullptr ||
            dynamic_cast<VeriGenerateFor*>(module_item) != nullptr ||
            dynamic_cast<VeriPathDecl*>(module_item) != nullptr ||
            dynamic_cast<VeriPulseControl*>(module_item) != nullptr ||
            dynamic_cast<VeriSpecifyBlock*>(module_item) != nullptr ||
            dynamic_cast<VeriSystemTimingCheck*>(module_item) != nullptr ||
            dynamic_cast<VeriTable*>(module_item) != nullptr ||
            dynamic_cast<VeriTaskDecl*>(module_item) != nullptr ||
            dynamic_cast<VeriTimeUnit*>(module_item) != nullptr) {
        balk(module_item, "unhandled node");
    } else if (auto always = dynamic_cast<VeriAlwaysConstruct*>(module_item)) {
        bb_t* bb = create_empty_bb("always", BB_ALWAYS);
        process_statement(bb, always->GetStmt());
    } else if (auto continuous = dynamic_cast<VeriContinuousAssign*>(module_item)) {
        bb_t* bb = create_empty_bb("cassign", BB_CONT_ASSIGNMENT);

        /// "assign" outside an "always" block.
        uint32_t idx = 0;
        VeriNetRegAssign* net_reg_assign = nullptr;

        FOREACH_ARRAY_ITEM(continuous->GetNetAssigns(), idx, net_reg_assign) {
            bb->append(new assign_t(bb, net_reg_assign));
        }
    } else if (auto func_decl = dynamic_cast<VeriFunctionDecl*>(module_item)) {
    /*
        // Treat this just like any other module.
        VeriName* veri_name = func_decl->GetSubprogramName();
        identifier_t function_name = veri_name->GetName();

        module_t* module_ds = new module_t(function_name);
        module_ds->set_function();

        bb_t* arg_bb = module_ds->create_empty_bb("args", BB_ARGS);

        uint32_t idx = 0;
        VeriAnsiPortDecl* decl = nullptr;

        FOREACH_ARRAY_ITEM(func_decl->GetAnsiIOList(), idx, decl) {
            uint8_t state = STATE_UNKNOWN;

            switch (decl->GetDir()) {
                case VERI_INPUT:    state = STATE_DEF;              break;
                case VERI_OUTPUT:   state = STATE_USE;              break;
                case VERI_INOUT:    state = STATE_DEF | STATE_USE;  break;
            }

            unsigned idx = 0;
            VeriIdDef* arg_id = nullptr;

            FOREACH_ARRAY_ITEM(decl->GetIds(), idx, arg_id) {
                module_ds->add_arg(arg_id->GetName(), state);
                arg_bb->append(new arg_t(arg_bb, arg_id->GetName(), state));
            }
        }

        module_map.emplace(function_name, module_ds);
    */
    } else if (auto initial = dynamic_cast<VeriInitialConstruct*>(module_item)) {
        bb_t* bb = create_empty_bb("initial", BB_INITIAL);
        process_statement(bb, initial->GetStmt());
    } else if (auto decl = dynamic_cast<VeriDataDecl*>(module_item)) {
        unsigned idx = 0;
        VeriIdDef* arg_id = nullptr;

        if (decl->IsIODecl()) {
            state_t state = STATE_UNKNOWN;

            switch (decl->GetDir()) {
                case VERI_INPUT:    state = STATE_DEF;              break;
                case VERI_OUTPUT:   state = STATE_USE;              break;
                case VERI_INOUT:    state = STATE_DEF | STATE_USE;  break;
            }

            FOREACH_ARRAY_ITEM(decl->GetIds(), idx, arg_id) {
                update_arg(arg_id->GetName(), state);
            }
        }
    } else if (auto def_param = dynamic_cast<VeriDefParam*>(module_item)) {
        // TODO
    } else if (auto module = dynamic_cast<VeriModule*>(module_item)) {
        // TOOD: process_module(module);
    } else if (auto inst = dynamic_cast<VeriModuleInstantiation*>(module_item)) {
        bb_t* bb = create_empty_bb("instantiation", BB_ORDINARY);

        uint32_t idx = 0;
        VeriInstId* module_instance = nullptr;
        const char* module_name = inst->GetModuleName();

        FOREACH_ARRAY_ITEM(inst->GetInstances(), idx, module_instance) {
            bb->append(new invoke_t(bb, module_instance, module_name));
        }
    } else if (auto stmt = dynamic_cast<VeriStatement*>(module_item)) {
        bb_t* bb = create_empty_bb(".dangling", BB_DANGLING);
        process_statement(bb, stmt);
    } else {
        balk(module_item, "unhandled node");
    }
}

void module_t::process_statement(bb_t*& bb, VeriStatement* stmt) {
    unsigned idx = 0;

    if (dynamic_cast<VeriAssertion*>(stmt) != nullptr ||
            dynamic_cast<VeriJumpStatement*>(stmt) != nullptr ||
            dynamic_cast<VeriRandsequence*>(stmt) != nullptr ||
            dynamic_cast<VeriSequentialInstantiation*>(stmt) != nullptr ||
            dynamic_cast<VeriWaitOrder*>(stmt) != nullptr ||
            dynamic_cast<VeriWithStmt*>(stmt) != nullptr) {
        balk(stmt, "SystemVerilog node");
    } else if (dynamic_cast<VeriNullStatement*>(stmt)) {
        balk(stmt, "unhandled node");
    } else if (auto assign_stmt = dynamic_cast<VeriAssign*>(stmt)) {
        /// "assign" inside an always block.
        bb->append(new stmt_t(bb, assign_stmt));
    } else if (auto blocking_assign = dynamic_cast<VeriBlockingAssign*>(stmt)) {
        /// assignment using "=" without the assign keyword
        bb->append(new stmt_t(bb, blocking_assign));
    } else if (auto case_stmt = dynamic_cast<VeriCaseStatement*>(stmt)) {
        bb->append(new stmt_t(bb, case_stmt));
    } else if (auto code_block = dynamic_cast<VeriCodeBlock*>(stmt)) {
        VeriStatement* __stmt = nullptr;

        FOREACH_ARRAY_ITEM(code_block->GetStatements(), idx, __stmt) {
            process_statement(bb, __stmt);
        }
    } else if (auto cond_stmt = dynamic_cast<VeriConditionalStatement*>(stmt)) {
        bb->append(new cmpr_t(bb, cond_stmt->GetIfExpr()));
        bb_t* merge_bb = create_empty_bb("merge", BB_ORDINARY);

        bb_t* then_bb = create_empty_bb("then", BB_ORDINARY);
        bb->set_left_successor(then_bb);

        process_statement(then_bb, stmt->GetThenStmt());
        then_bb->set_left_successor(merge_bb);

        VeriStatement* inner_statement = stmt->GetElseStmt();
        if (inner_statement != nullptr) {
            bb_t* else_bb = create_empty_bb("else", BB_ORDINARY);
            bb->set_right_successor(else_bb);

            process_statement(else_bb, inner_statement);
            else_bb->set_left_successor(merge_bb);
        }

        bb = merge_bb;
    } else if (auto de_assign_stmt = dynamic_cast<VeriDeAssign*>(stmt)) {
        /// "deassign" keyword to undo the last blocking assignment.
        bb->append(new stmt_t(bb, de_assign_stmt));
    } else if (auto delay_control = dynamic_cast<VeriDelayControlStatement*>(stmt)) {
        /// "#" followed by the delay followed by a statement.
        /// useful for tracking timing leakage.
        bb->append(new stmt_t(bb, delay_control));
    } else if (auto disable_stmt = dynamic_cast<VeriDisable*>(stmt)) {
        /// "disable" keyword for jumping to a specific point in the code.
        /// useful for tracking timing leakage.
        bb->append(new stmt_t(bb, disable_stmt));
    } else if (auto event_ctrl = dynamic_cast<VeriEventControlStatement*>(stmt)) {
        /// "@" expression for specifying when to trigger some actions.
        /// useful for tracking timing and non-timing leakage.

        id_set_t identifier_set;
        VeriExpression* expr = nullptr;

        FOREACH_ARRAY_ITEM(event_ctrl->GetAt(), idx, expr) {
            if (expr != nullptr) {
                id_desc_list_t desc_list;
                instr_t::describe_expr(expr, desc_list, STATE_USE);

                for (id_desc_t desc : desc_list) {
                    identifier_set.insert(desc.name);
                }
            }
        }

        assert(bb->block_type() == BB_ALWAYS && "assumption failed!");
        bb->append(new trigger_t(bb, identifier_set));

        process_statement(bb, event_ctrl->GetStmt());
    } else if (auto event_trigger = dynamic_cast<VeriEventTrigger*>(stmt)) {
        /// "->" or "->>" expression to specify an event to trigger.
        /// useful for tracking timing and non-timing leakage.
        bb->append(new stmt_t(bb, event_trigger));
    } else if (auto force = dynamic_cast<VeriForce*>(stmt)) {
        /// used only during simulation (SystemVerilog?), ignore for now.
        // TODO
    } else if (auto gen_var_assign = dynamic_cast<VeriGenVarAssign*>(stmt)) {
        // TODO
    } else if (auto loop = dynamic_cast<VeriLoop*>(stmt)) {
        // TODO
    } else if (auto non_blocking_assign = dynamic_cast<VeriNonBlockingAssign*>(stmt)) {
        /// assignment using "<=" without the assign keyword
        bb->append(new stmt_t(bb, non_blocking_assign));
    } else if (auto par_block = dynamic_cast<VeriParBlock*>(stmt)) {
        VeriStatement* __stmt = nullptr;

        FOREACH_ARRAY_ITEM(par_block->GetStatements(), idx, __stmt) {
            process_statement(bb, __stmt);
        }
    } else if (auto seq_block = dynamic_cast<VeriSeqBlock*>(stmt)) {
        VeriStatement* __stmt = nullptr;

        FOREACH_ARRAY_ITEM(seq_block->GetStatements(), idx, __stmt) {
            if (__stmt != nullptr) {
                process_statement(bb, __stmt);
            }
        }
    } else if (auto sys_task = dynamic_cast<VeriSystemTaskEnable*>(stmt)) {
        /// used for generating inputs and output, ignore.
    } else if (auto task = dynamic_cast<VeriTaskEnable*>(stmt)) {
        /// similar to system tasks, ignore?
    } else if (auto wait = dynamic_cast<VeriWait*>(stmt)) {
        /// "wait" keyword for waiting for an expression to be true.
        /// useful for tracking timing leakage.
        bb->append(new stmt_t(bb, wait));
    } else {
        balk(stmt, "unhandled node");
    }
}

instr_set_t& module_t::def_instrs(identifier_t identifier) {
    id_map_t::iterator it = def_map.find(identifier);
    assert(it != def_map.end() && "failed to find def for requested id!");

    return it->second;
}

instr_set_t& module_t::use_instrs(identifier_t identifier) {
    id_map_t::iterator it = use_map.find(identifier);
    assert(it != use_map.end() && "failed to find use for requested id!");

    return it->second;
}

void module_t::remove_from_top_level_blocks(bb_t* bb) {
    top_level_blocks.erase(bb);
}

bool module_t::exists(bb_t* bb) {
    return std::find(basicblocks.begin(), basicblocks.end(), bb) !=
        basicblocks.end();
}

bool module_t::postdominates(bb_t* lo, bb_t* hi) {
    if (exists(lo) == false || exists(hi) == false) {
        assert(hi != nullptr);
        assert(false && "non-existent blocks as input to postdominates!");
        return false;
    }

    bb_set_t& postdom_set = postdominators.at(hi);
    return postdom_set.find(lo) != postdom_set.end();
}

bb_t* module_t::immediate_dominator(bb_t* ref_bb) {
    bb_map_t::iterator it = imm_dominator.find(ref_bb);
    if (it == imm_dominator.end()) {
        return nullptr;
    }

    return it->second;
}

bb_t* module_t::immediate_postdominator(bb_t* ref_bb) {
    bb_map_t::iterator it = imm_postdominator.find(ref_bb);
    if (it == imm_postdominator.end()) {
        return nullptr;
    }

    return it->second;
}

void module_t::populate_guard_blocks(bb_t* ref_bb, bb_set_t& guard_blocks) {
    bb_t* hi_block = ref_bb;
    bb_t* entry_block = ref_bb->entry_block();

    while (hi_block != entry_block) {
        while (hi_block != nullptr && postdominates(ref_bb, hi_block)) {
            hi_block = immediate_dominator(hi_block);
        }

        if (hi_block == nullptr) {
            break;
        }

        guard_blocks.insert(hi_block);

        // Continue upwards from the newly discovered condition block.
        ref_bb = hi_block;
    }
}

id_set_t& module_t::output_ports() {
    return out_ports;
}
