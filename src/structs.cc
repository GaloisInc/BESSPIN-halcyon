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

/*! \brief catch-all error routine
 */
void balk(VeriTreeNode* tree_node, const std::string& msg, const char* filename,
        int line_number) {
    util_t::clear_status();

    char message[128];
    snprintf(message, sizeof(message), "\r%s:%d   %s [node = %p]\n", filename,
            line_number, msg.c_str(), tree_node);
    util_t::update_status(message);

    tree_node->PrettyPrintXml(std::cerr, 100);
    assert(false && "unrecoverable error!");
}

instr_t::instr_t(bb_t* parent) {
    containing_bb = parent;
}

instr_t::~instr_t() {
    containing_bb = nullptr;
}

/*! \brief parse expression and record identifiers and their use.
 */
void util_t::describe_expr(VeriExpression* expr, id_desc_list_t& desc_list,
        uint8_t type_hint, module_t* module) {
    if (expr == nullptr) {
        return;
    }

    if (auto port = dynamic_cast<VeriAnsiPortDecl*>(expr)) {
        uint32_t idx = 0;
        VeriIdDef* id_def = nullptr;

        FOREACH_ARRAY_ITEM(port->GetIds(), idx, id_def) {
            describe_expr(id_def->GetActualName(), desc_list, type_hint, module);
        }
    } else if (auto bin_op = dynamic_cast<VeriBinaryOperator*>(expr)) {
        describe_expr(bin_op->GetLeft(), desc_list, type_hint, module);
        describe_expr(bin_op->GetRight(), desc_list, type_hint, module);
    } else if (auto case_op = dynamic_cast<VeriCaseOperator*>(expr)) {
        describe_expr(case_op->GetCondition(), desc_list, STATE_USE, module);

        uint32_t idx = 0;
        VeriCaseOperatorItem* case_op_item = nullptr;

        FOREACH_ARRAY_ITEM(case_op->GetCaseItems(), idx, case_op_item) {
            VeriExpression* property_expr = case_op_item->GetPropertyExpr();
            describe_expr(property_expr, desc_list, type_hint, module);

            uint32_t idx = 0;
            VeriExpression* condition = nullptr;

            FOREACH_ARRAY_ITEM(case_op_item->GetConditions(), idx, condition) {
                describe_expr(condition, desc_list, STATE_USE, module);
            }
        }
    } else if (auto cast_op = dynamic_cast<VeriCast*>(expr)) {
        describe_expr(cast_op->GetExpr(), desc_list, type_hint, module);
    } else if (auto concat = dynamic_cast<VeriConcat*>(expr)) {
        uint32_t idx = 0;
        VeriConcatItem* concat_item = nullptr;

        FOREACH_ARRAY_ITEM(concat->GetExpressions(), idx, concat_item) {
            describe_expr(concat_item, desc_list, type_hint, module);
        }
    } else if (auto concat_item = dynamic_cast<VeriConcatItem*>(expr)) {
        describe_expr(concat_item->GetExpr(), desc_list, type_hint, module);
    } else if (auto cond_pred = dynamic_cast<VeriCondPredicate*>(expr)) {
        describe_expr(cond_pred->GetLeft(), desc_list, type_hint, module);
        describe_expr(cond_pred->GetRight(), desc_list, type_hint, module);
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
            describe_expr(expression, desc_list, type_hint, module);
        }
    } else if (auto delay_ctrl = dynamic_cast<VeriDelayOrEventControl*>(expr)) {
        describe_expr(delay_ctrl->GetDelayControl(), desc_list, STATE_USE, module);

        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(delay_ctrl->GetEventControl(), idx, expression) {
            describe_expr(expression, desc_list, STATE_USE, module);
        }

        describe_expr(delay_ctrl->GetRepeatEvent(), desc_list, type_hint, module);
    } else if (auto dot_name = dynamic_cast<VeriDotName*>(expr)) {
        describe_expr(dot_name->GetVarName(), desc_list, type_hint, module);
    } else if (auto event_expr = dynamic_cast<VeriEventExpression*>(expr)) {
        describe_expr(event_expr->GetIffCondition(), desc_list, STATE_USE, module);
        describe_expr(event_expr->GetExpr(), desc_list, type_hint, module);
    } else if (auto function_call = dynamic_cast<VeriFunctionCall*>(expr)) {
        identifier_t func_name = function_call->GetFunctionName()->GetName();
        proc_decl_t* proc_decl = module->proc_decl_by_id(func_name);

        if (proc_decl == nullptr) {
            util_t::clear_status();
            util_t::warn("failed to find function declaration '" + func_name +
                    "'\n");

            return;
        }

        uint32_t idx = 0;
        VeriIdRef* id_ref = nullptr;

        // XXX: Conservatively, assume that all
        // arguments to the function are used.

        FOREACH_ARRAY_ITEM(function_call->GetArgs(), idx, id_ref) {
            identifier_t id = id_ref->GetId()->Name();

            id_desc_t desc = { id, STATE_USE };
            desc_list.push_back(desc);
        }
    } else if (auto indexed_expr = dynamic_cast<VeriIndexedExpr*>(expr)) {
        describe_expr(indexed_expr->GetPrefixExpr(), desc_list, type_hint, module);
        describe_expr(indexed_expr->GetIndexExpr(), desc_list, STATE_USE, module);
    } else if (auto min_typ_max = dynamic_cast<VeriMinTypMaxExpr*>(expr)) {
        describe_expr(min_typ_max->GetMinExpr(), desc_list, type_hint, module);
        describe_expr(min_typ_max->GetTypExpr(), desc_list, type_hint, module);
        describe_expr(min_typ_max->GetMaxExpr(), desc_list, type_hint, module);
    } else if (auto multi_concat = dynamic_cast<VeriMultiConcat*>(expr)) {
        describe_expr(multi_concat->GetRepeat(), desc_list, STATE_USE, module);

        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(multi_concat->GetExpressions(), idx, expression) {
            describe_expr(expression, desc_list, type_hint, module);
        }
    } else if (auto id_ref = dynamic_cast<VeriIdRef*>(expr)) {
        id_desc_t desc = { id_ref->GetName(), type_hint };
        desc_list.push_back(desc);
    } else if (auto indexed_id = dynamic_cast<VeriIndexedId*>(expr)) {
        id_desc_t desc = { indexed_id->GetName(), type_hint };
        desc_list.push_back(desc);

        describe_expr(indexed_id->GetIndexExpr(), desc_list, STATE_USE, module);
    } else if (auto idx_mem_id = dynamic_cast<VeriIndexedMemoryId*>(expr)) {
        id_desc_t desc = { idx_mem_id->GetName(), type_hint };
        desc_list.push_back(desc);

        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(idx_mem_id->GetIndexes(), idx, expression) {
            describe_expr(expression, desc_list, STATE_USE, module);
        }
    } else if (auto selected_name = dynamic_cast<VeriSelectedName*>(expr)) {
        id_desc_t desc = { selected_name->GetPrefix()->GetName(), type_hint };
        desc_list.push_back(desc);

        desc = { selected_name->GetSuffix(), type_hint };
        desc_list.push_back(desc);
    } else if (auto new_expr = dynamic_cast<VeriNew*>(expr)) {
        describe_expr(new_expr->GetSizeExpr(), desc_list, type_hint, module);

        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(new_expr->GetArgs(), idx, expression) {
            describe_expr(expression, desc_list, STATE_USE, module);
        }
    } else if (auto path_pulse = dynamic_cast<VeriPathPulseVal*>(expr)) {
        describe_expr(path_pulse->GetRejectLimit(), desc_list, type_hint, module);
        describe_expr(path_pulse->GetErrorLimit(), desc_list, type_hint, module);
    } else if (auto pattern_match = dynamic_cast<VeriPatternMatch*>(expr)) {
        describe_expr(pattern_match->GetLeft(), desc_list, type_hint, module);
        describe_expr(pattern_match->GetRight(), desc_list, type_hint, module);
    } else if (auto port_conn = dynamic_cast<VeriPortConnect*>(expr)) {
        id_desc_t desc = { port_conn->GetNamedFormal(), type_hint };
        desc_list.push_back(desc);

        describe_expr(port_conn->GetConnection(), desc_list, type_hint, module);
    } else if (auto question_colon = dynamic_cast<VeriQuestionColon*>(expr)) {
        describe_expr(question_colon->GetIfExpr(), desc_list, STATE_USE, module);
        describe_expr(question_colon->GetThenExpr(), desc_list, STATE_USE, module);
        describe_expr(question_colon->GetElseExpr(), desc_list, STATE_USE, module);
    } else if (auto range = dynamic_cast<VeriRange*>(expr)) {
        describe_expr(range->GetLeft(), desc_list, type_hint, module);
        describe_expr(range->GetRight(), desc_list, type_hint, module);
    } else if (auto sysfunc = dynamic_cast<VeriSystemFunctionCall*>(expr)) {
        uint32_t idx = 0;
        VeriExpression* expression = nullptr;

        FOREACH_ARRAY_ITEM(sysfunc->GetArgs(), idx, expression) {
            describe_expr(expression, desc_list, STATE_USE, module);
        }
    } else if (auto timing_check = dynamic_cast<VeriTimingCheckEvent*>(expr)) {
        describe_expr(timing_check->GetCondition(), desc_list, STATE_USE, module);
        describe_expr(timing_check->GetTerminalDesc(), desc_list, type_hint, module);
    } else if (auto unary_op = dynamic_cast<VeriUnaryOperator*>(expr)) {
        describe_expr(unary_op->GetArg(), desc_list, type_hint, module);
    } else if (auto with_expr = dynamic_cast<VeriWith*>(expr)) {
        describe_expr(with_expr->GetLeft(), desc_list, type_hint, module);
    } else {
        balk(expr, "unhandled expression", __FILE__, __LINE__);
    }
}

void instr_t::parse_expression(VeriExpression* expr, uint8_t dst_set) {
    id_desc_list_t desc_list;
    util_t::describe_expr(expr, desc_list, dst_set, parent()->parent());

    for (id_desc_t desc : desc_list) {
        if (desc.type & STATE_DEF) {
            add_def(desc.name);
        }

        if (desc.type & STATE_USE) {
            add_use(desc.name);
        }

        if (desc.type == 0) {
            util_t::fatal("invalid state type for '" + desc.name + "'");
            assert(false && "invalid destination!");
        }
    }
}

void instr_t::parse_statement(VeriStatement* stmt) {
    assert(stmt != nullptr && "null statement!");

    if (util_t::ignored_statement(stmt)) {
        ;
    } else if (auto assign = dynamic_cast<VeriAssign*>(stmt)) {
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
    } else if (auto event_trigger = dynamic_cast<VeriEventTrigger*>(stmt)) {
        parse_expression(event_trigger->GetControl(), STATE_USE);
        parse_expression(event_trigger->GetEventName(), STATE_DEF);
    } else if (auto nonblocking = dynamic_cast<VeriNonBlockingAssign*>(stmt)) {
        parse_expression(nonblocking->GetLValue(), STATE_DEF);
        parse_expression(nonblocking->GetValue(), STATE_USE);
    } else if (auto wait = dynamic_cast<VeriWait*>(stmt)) {
        parse_expression(wait->GetCondition(), STATE_USE);
        parse_statement(wait->GetStmt());
    } else if (auto seq_block = dynamic_cast<VeriSeqBlock*>(stmt)) {
        pinstr_t* pinstr = new pinstr_t(this, stmt);
        pinstrs.push_back(pinstr);
    } else if (dynamic_cast<VeriConditionalStatement*>(stmt) != nullptr) {
        module_t* module_ds = this->parent()->parent();

        // FIXME: Wrap this basic block and its successors into an instruction.
        bb_t* new_bb = module_ds->create_empty_bb("nested", BB_HIDDEN, true);

        new_bb->append(new cmpr_t(new_bb, stmt->GetIfExpr()));
        bb_t* merge_bb = module_ds->create_empty_bb("merge", BB_ORDINARY, true);

        bb_t* then_bb = module_ds->create_empty_bb("then", BB_ORDINARY, true);
        new_bb->set_left_successor(then_bb);

        module_ds->process_statement(then_bb, stmt->GetThenStmt());
        then_bb->set_left_successor(merge_bb);

        VeriStatement* inner_statement = stmt->GetElseStmt();
        if (inner_statement != nullptr) {
            bb_t* else_bb = module_ds->create_empty_bb("else", BB_ORDINARY,
                    true);

            new_bb->set_right_successor(else_bb);

            module_ds->process_statement(else_bb, inner_statement);
            else_bb->set_left_successor(merge_bb);
        }
    } else if (auto delay_control = dynamic_cast<VeriDelayControlStatement*>(stmt)) {
        parse_expression(delay_control->GetDelay(), STATE_USE);
        parse_statement(delay_control->GetStmt());
    } else {
        balk(stmt, "unhandled instruction", __FILE__, __LINE__);
    }
}

/*! \brief basic block that contains this instruction.
 */
bb_t* instr_t::parent() {
    return containing_bb;
}

/*! \brief set of identifiers defined by this instruction.
 */
id_set_t& instr_t::defs() {
    return def_set;
}

/*! \brief set of identifiers used by this instruction.
 */
id_set_t& instr_t::uses() {
    return use_set;
}

void instr_t::add_def(identifier_t def_id) {
    def_set.insert(def_id);
}

void instr_t::add_use(identifier_t use_id) {
    use_set.insert(use_id);
}

param_t::param_t(bb_t* parent, identifier_t name) : instr_t(parent)  {
    param_name = name;
    add_def(param_name);
}

/*! \brief print instruction to the console (stderr).
 */
void param_t::dump() {
    identifier_t module_name = parent()->parent()->name();
    util_t::plain("param: " + param_name + " in module " + module_name + "\n");
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

/*! \brief print instruction to the console (stderr).
 */
void trigger_t::dump() {
    std::cerr << "trigger:";

    for (identifier_t id : id_set) {
        std::cerr << " " << id;
    }

    std::cerr << " in module " << parent()->parent()->name() << "\n";
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

/*! \brief print instruction to the console (stderr).
 */
void stmt_t::dump() {
    stmt->PrettyPrint(std::cerr, 100);
    std::cerr << " in module " << parent()->parent()->name() << "\n";
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

/*! \brief print instruction to the console (stderr).
 */
void assign_t::dump() {
    assign->PrettyPrint(std::cerr, 100);
    std::cerr << " in module " << parent()->parent()->name() << "\n";
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
        util_t::describe_expr(connect->GetConnection(), desc_list, STATE_USE,
                parent()->parent());

        conn_t connection;
        connection.remote_endpoint = connect->GetNamedFormal();

        for (id_desc_t desc : desc_list) {
            connection.id_set.insert(desc.name);
        }

        conns.push_back(connection);
    }
}

/*! \brief print instruction to the console (stderr).
 */
void invoke_t::dump() {
    std::cerr << "remote module: " << mod_name << ": ";
    mod_inst->PrettyPrint(std::cerr, 100);
    std::cerr << " in module " << parent()->parent()->name() << "\n";
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

/*! \brief print instruction to the console (stderr).
 */
void cmpr_t::dump() {
    cmpr->PrettyPrint(std::cerr, 100);
    std::cerr << " in module " << parent()->parent()->name() << "\n";
}

bool cmpr_t::operator==(const instr_t& reference) {
    if (const cmpr_t* ref = dynamic_cast<const cmpr_t*>(&reference)) {
        return cmpr == ref->cmpr;
    }

    return false;
}

pinstr_t::pinstr_t(instr_t* parent, VeriStatement* __stmt) {
    stmt = __stmt;
    containing_instr = parent;

    module_t* module_ds = parent->parent()->parent();
    bb_t* new_bb = module_ds->create_empty_bb("nested", BB_HIDDEN, true);

    module_ds->process_statement(new_bb, stmt);

    // Copy defs and uses from all instructions within this statement.
    bb_set_t reachable_bbs;
    util_t::build_reachable_set(new_bb, reachable_bbs);

    for (bb_t* bb : reachable_bbs) {
        for (instr_t* instr : bb->instrs()) {
            for (identifier_t def_id : instr->defs()) {
                parent->add_def(def_id);
            }

            for (identifier_t use_id : instr->uses()) {
                parent->add_use(use_id);
            }
        }
    }
}

/*! \brief print instruction to the console (stderr).
 */
void pinstr_t::dump() {
    stmt->PrettyPrint(std::cerr, 100);

    bb_t* bb = containing_instr->parent();
    module_t* module_ds = bb->parent();
    std::cerr << " in module " << module_ds->name() << "\n";
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

/*! \brief check whether an instruction is already present in the basic block.
 */
bool bb_t::exists(instr_t* instr) {
    return std::find(instr_list.begin(), instr_list.end(), instr) !=
        instr_list.end();
}

/*! \brief add instruction to the end of the basic block.
 */
bool bb_t::append(instr_t* new_instr) {
    if (exists(new_instr) == true) {
        assert(false && "instruction already exists or is a comparison!");
        return false;
    }

    instr_list.push_back(new_instr);
    return true;
}

/*! \brief set the first successor basic block.
 */
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

/*! \brief set the second successor basic block.
 */
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

/*! \brief first successor basic block.
 */
bb_t* bb_t::left_successor() {
    return successor_left;
}

/*! \brief second successor basic block.
 */
bb_t* bb_t::right_successor() {
    return successor_right;
}

/*! \brief instructions in the basic block.
 */
instr_list_t& bb_t::instrs() {
    return instr_list;
}

/*! \brief list of predecessors of this basic block.
 */
bb_set_t& bb_t::preds() {
    return predecessors;
}

/*! \brief count of predecessors of this basic block.
 */
uint64_t bb_t::pred_count() {
    return predecessors.size();
}

/*! \brief count of successors of this basic block.
 */
uint64_t bb_t::succ_count() {
    return (successor_left != nullptr) + (successor_right != nullptr);
}

/*! \brief print basic block to the console (stderr).
 */
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

    for (instr_t* instr : instr_list) {
        std::cerr << "    ";
        instr->dump();
    }
}

/*! \brief the entry basic block that eventually leads to this basic block.
 *
 * Verilog permits multiple "entry points" into the code (always blocks,
 * initialization blocks, continuous assignments, etc.  This function returns
 * the pointer to the basic block that is the entry point for reaching this
 * basic block.
 */
bb_t* bb_t::entry_block() {
    return entry_bb;
}

/*! \brief name of this basic block.
 */
identifier_t bb_t::name() {
    return bb_name;
};

/*! \brief set the entry block for this basic block.
 */
void bb_t::set_entry_block(bb_t* block) {
    entry_bb = block;
}

/*! \brief type of this basic block
 *
 * The analysis code processes basic blocks differently based on whether they
 * represent sequential control flow or triggered execution.
 */
state_t bb_t::block_type() {
    return bb_type;
}

/*! \brief comparison operation used to decide the successor of this basic
 * block.
 */
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

/*! \brief pointer to the module that contains this basic block.
 */
module_t* bb_t::parent() {
    return containing_module;
}

module_t::module_t(VeriModule*& module) {
    is_function = false;
    empty_dominators = true;
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

/*! \brief name of this module.
 */
identifier_t module_t::name() {
    return mod_name;
}

bb_t* module_t::create_empty_bb(identifier_t name, state_t bb_type,
        bool floating) {
    identifier_t bb_name = make_unique_bb_id(name);
    bb_t* new_block = new bb_t(this, bb_name, bb_type);

    if (floating == false) {
        basicblocks.push_back(new_block);
    }

    // Mark this as a top-level block for now,
    // but fix it if we make it a successor block.
    top_level_blocks.insert(new_block);

    return new_block;
}

/*! \brief print module's basic blocks to the console (stderr).
 *
 */
void module_t::dump() {
    for (bb_t* bb : basicblocks) {
        bb->dump();
    }
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

/*! \brief find the dominator and postdominators of each basic block.
 *
 * For simplicity, we use the O(V^2) iterative algorithm.  The Lengauer-Tarjan
 * Dominator Tree Algorithm will almost surely improve analysis performance.
 */
void module_t::build_dominator_sets() {
    if (empty_dominators == false) {
        return;
    }

    char message[128];
    snprintf(message, sizeof(message), "building dominators for module %s... ",
            name().substr(0, 8).c_str());
    util_t::update_status(message);

    for (bb_t* bb : top_level_blocks) {
        assert(bb->pred_count() == 0 && "not a top-level block!");

        bb_set_t reachable;
        util_t::build_reachable_set(bb, reachable);
        build_dominator_sets(reachable);
    }

    empty_dominators = false;
}

/*! \brief find the definitions and uses of each instruction in this module.
 */
void module_t::build_def_use_chains() {
    for (bb_t* bb : basicblocks) {
        for (instr_t* instr : bb->instrs()) {
            for (identifier_t id : instr->defs()) {
                add_def(id, instr);
            }

            for (identifier_t id : instr->uses()) {
                add_use(id, instr);
            }
        }
    }
}

bool module_t::process_connection(conn_t& connection, invoke_t* invocation,
        module_t* invoked_module) {
    if (connection.state == STATE_UNKNOWN) {
        return false;
    }

    // IMPORTANT: When processing connections, we flip the def and use flags.

    if (connection.state & STATE_USE) {
        // This pin is set by the invoked module.

        if (connection.id_set.size() > 1) {
            assert(false && "simultaneous definition of more than one id!");
            return false;
        }

        if (connection.id_set.size() == 1) {
            id_set_t::iterator it = connection.id_set.begin();
            const identifier_t& id = *it;

            add_def(id, invocation);
            invoked_module->add_use(connection.remote_endpoint, invocation);
        }
    }

    if (connection.state & STATE_DEF) {
        // This pin is read by the invoked module.

        for (const identifier_t& id : connection.id_set) {
            add_use(id, invocation);
        }

        invoked_module->add_def(connection.remote_endpoint, invocation);
    }

    return true;
}

void module_t::resolve_invoke(invoke_t* invocation, module_map_t& module_map) {
    identifier_t mod_name = invocation->module_name();
    module_map_t::iterator it = module_map.find(mod_name);

    if (it == module_map.end()) {
        util_t::fatal("referred module: " + mod_name + " in module " + name() +
                "\n");

        assert(false && "found reference to undefined module!");
        return;
    }

    module_t* module = it->second;

    for (conn_t& connection : invocation->connections()) {
        identifier_t end_id = connection.remote_endpoint;

        connection.state = module->arg_state(end_id);
        process_connection(connection, invocation, module);
    }
}

/*! \brief match module invocations with module definitions.
 */
void module_t::resolve_links(module_map_t& module_map) {
    for (bb_t* bb : basicblocks) {
        for (instr_t* instr : bb->instrs()) {
            if (invoke_t* invocation = dynamic_cast<invoke_t*>(instr)) {
                resolve_invoke(invocation, module_map);
            }
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
        char message[1024];
        snprintf(message, sizeof(message), "found %zd undefined identifier(s) "
                "in module '%s'.\n", undef_ids.size(), mod_name.c_str());

        util_t::plain("\n");
        util_t::warn(message);

        util_t::dump_set(undef_ids);
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

void module_t::process_module_items(Array* module_items) {
    if (module_items == nullptr || module_items->Size() == 0) {
        return;
    }

    uint32_t idx = 0;
    VeriModuleItem* module_item = nullptr;

    FOREACH_ARRAY_ITEM(module_items, idx, module_item) {
        process_module_item(module_item);
    }
}

void module_t::process_module_params(Array* params) {
    if (params == nullptr || params->Size() == 0) {
        return;
    }

    uint32_t idx = 0;
    VeriIdDef* id_def = nullptr;

    bb_t* bb_params = create_empty_bb("params", BB_PARAMS, false);

    FOREACH_ARRAY_ITEM(params, idx, id_def) {
        bb_params->append(new param_t(bb_params, id_def->GetName()));
    }
}

void module_t::process_module_ports(Array* port_connects) {
    if (port_connects == nullptr || port_connects->Size() == 0) {
        return;
    }

    uint32_t idx = 0;
    VeriAnsiPortDecl* decl = nullptr;

    FOREACH_ARRAY_ITEM(port_connects, idx, decl) {
        uint8_t state = STATE_UNKNOWN;

        switch (decl->GetDir()) {
            case VERI_INPUT:    state = STATE_DEF;              break;
            case VERI_OUTPUT:   state = STATE_USE;              break;
            case VERI_INOUT:    state = STATE_DEF | STATE_USE;  break;
        }

        uint32_t idx = 0;
        VeriIdDef* arg_id = nullptr;

        FOREACH_ARRAY_ITEM(decl->GetIds(), idx, arg_id) {
            identifier_t id = arg_id->GetName();

            add_arg(id, state);
            arg_ports.insert(id);
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
        balk(module_item, "SystemVerilog node", __FILE__, __LINE__);
    } else if (dynamic_cast<VeriCoverageOption*>(module_item) != nullptr ||
            dynamic_cast<VeriCoverageSpec*>(module_item) != nullptr ||
            dynamic_cast<VeriDefaultDisableIff*>(module_item) != nullptr ||
            dynamic_cast<VeriGateInstantiation*>(module_item) != nullptr ||
            dynamic_cast<VeriPathDecl*>(module_item) != nullptr ||
            dynamic_cast<VeriPulseControl*>(module_item) != nullptr ||
            dynamic_cast<VeriSpecifyBlock*>(module_item) != nullptr ||
            dynamic_cast<VeriSystemTimingCheck*>(module_item) != nullptr ||
            dynamic_cast<VeriTable*>(module_item) != nullptr ||
            dynamic_cast<VeriTimeUnit*>(module_item) != nullptr) {
        balk(module_item, "unhandled node", __FILE__, __LINE__);
    } else if (auto always = dynamic_cast<VeriAlwaysConstruct*>(module_item)) {
        bb_t* bb = create_empty_bb("always", BB_ALWAYS, false);
        process_statement(bb, always->GetStmt());
    } else if (auto continuous = dynamic_cast<VeriContinuousAssign*>(module_item)) {
        bb_t* bb = create_empty_bb("cassign", BB_CONT_ASSIGNMENT, false);

        /// "assign" outside an "always" block.
        uint32_t idx = 0;
        VeriNetRegAssign* net_reg_assign = nullptr;

        FOREACH_ARRAY_ITEM(continuous->GetNetAssigns(), idx, net_reg_assign) {
            bb->append(new assign_t(bb, net_reg_assign));
        }
    } else if (auto task_decl = dynamic_cast<VeriTaskDecl*>(module_item)) {
        bb_t* bb = create_empty_bb("taskdecl", BB_ORDINARY, false);
        proc_decl_t* proc_decl = new proc_decl_t(bb, task_decl);

        bb->append(proc_decl);
        proc_decls.emplace(proc_decl->name(), proc_decl);
    } else if (auto func_decl = dynamic_cast<VeriFunctionDecl*>(module_item)) {
        bb_t* bb = create_empty_bb("funcdecl", BB_ORDINARY, false);
        proc_decl_t* proc_decl = new proc_decl_t(bb, func_decl);

        bb->append(proc_decl);
        proc_decls.emplace(proc_decl->name(), proc_decl);
    } else if (auto initial = dynamic_cast<VeriInitialConstruct*>(module_item)) {
        bb_t* bb = create_empty_bb("initial", BB_INITIAL, false);
        process_statement(bb, initial->GetStmt());
    } else if (auto decl = dynamic_cast<VeriDataDecl*>(module_item)) {
        uint32_t idx = 0;
        VeriIdDef* arg_id = nullptr;

        if (decl->IsIODecl()) {
            state_t state = STATE_UNKNOWN;

            switch (decl->GetDir()) {
                case VERI_INPUT:    state = STATE_DEF;              break;
                case VERI_OUTPUT:   state = STATE_USE;              break;
                case VERI_INOUT:    state = STATE_DEF | STATE_USE;  break;
            }

            FOREACH_ARRAY_ITEM(decl->GetIds(), idx, arg_id) {
                identifier_t port_id = arg_id->GetName();

                if (port_exists(port_id)) {
                    update_arg(arg_id->GetName(), state);
                } else {
                    add_arg(port_id, state);
                    arg_ports.insert(port_id);
                }
            }
        }
    } else if (auto def_param = dynamic_cast<VeriDefParam*>(module_item)) {
        uint32_t idx = 0;
        VeriDefParamAssign* param_assign = nullptr;
        bb_t* bb_params = create_empty_bb("params", BB_PARAMS, false);

        FOREACH_ARRAY_ITEM(def_param->GetDefParamAssigns(), idx, param_assign) {
            identifier_t name = param_assign->GetLVal()->GetName();
            bb_params->append(new param_t(bb_params, name));
        }
    } else if (auto module = dynamic_cast<VeriModule*>(module_item)) {
        balk(module, "nested module definitions aren't supported", __FILE__,
                __LINE__);
    } else if (auto inst = dynamic_cast<VeriModuleInstantiation*>(module_item)) {
        bb_t* bb = create_empty_bb("instantiation", BB_ORDINARY, false);

        uint32_t idx = 0;
        VeriInstId* module_instance = nullptr;
        const char* module_name = inst->GetModuleName();

        FOREACH_ARRAY_ITEM(inst->GetInstances(), idx, module_instance) {
            bb->append(new invoke_t(bb, module_instance, module_name));
        }
    } else if (auto stmt = dynamic_cast<VeriStatement*>(module_item)) {
        bb_t* bb = create_empty_bb(".dangling", BB_DANGLING, false);
        process_statement(bb, stmt);
    } else {
        balk(module_item, "unhandled node", __FILE__, __LINE__);
    }
}

void module_t::process_statement(bb_t*& bb, VeriStatement* stmt) {
    uint32_t idx = 0;

    if (stmt == nullptr) {
        return;
    }

    if (util_t::ignored_statement(stmt) == true) {
        ;
    } else if (util_t::ordinary_statement(stmt) == true) {
        bb->append(new stmt_t(bb, stmt));
    } else if (dynamic_cast<VeriConditionalStatement*>(stmt) != nullptr) {
        bool floating = exists(bb) == false;

        bb->append(new cmpr_t(bb, stmt->GetIfExpr()));
        bb_t* merge_bb = create_empty_bb("merge", BB_ORDINARY, floating);

        bb_t* then_bb = create_empty_bb("then", BB_ORDINARY, floating);
        bb->set_left_successor(then_bb);

        process_statement(then_bb, stmt->GetThenStmt());
        then_bb->set_left_successor(merge_bb);

        VeriStatement* inner_statement = stmt->GetElseStmt();
        if (inner_statement != nullptr) {
            bb_t* else_bb = create_empty_bb("else", BB_ORDINARY, floating);
            bb->set_right_successor(else_bb);

            process_statement(else_bb, inner_statement);
            else_bb->set_left_successor(merge_bb);
        }

        bb = merge_bb;
    } else if (auto event_ctrl = dynamic_cast<VeriEventControlStatement*>(stmt)) {
        /// "@" expression for specifying when to trigger some actions.
        /// useful for tracking timing and non-timing leakage.

        id_set_t identifier_set;
        VeriExpression* expr = nullptr;

        FOREACH_ARRAY_ITEM(event_ctrl->GetAt(), idx, expr) {
            if (expr != nullptr) {
                id_desc_list_t desc_list;
                util_t::describe_expr(expr, desc_list, STATE_USE, this);

                for (id_desc_t desc : desc_list) {
                    identifier_set.insert(desc.name);
                }
            }
        }

        assert(bb->block_type() == BB_ALWAYS && "assumption failed!");
        bb->append(new trigger_t(bb, identifier_set));

        process_statement(bb, event_ctrl->GetStmt());
    } else if (dynamic_cast<VeriParBlock*>(stmt) != nullptr ||
            dynamic_cast<VeriSeqBlock*>(stmt) != nullptr ||
            dynamic_cast<VeriCodeBlock*>(stmt) != nullptr) {
        VeriStatement* __stmt = nullptr;

        FOREACH_ARRAY_ITEM(stmt->GetStatements(), idx, __stmt) {
            process_statement(bb, __stmt);
        }
    } else if (auto loop = dynamic_cast<VeriLoop*>(stmt)) {
        process_statement(bb, stmt->GetStmt());
    } else if (auto task_enable = dynamic_cast<VeriTaskEnable*>(stmt)) {
        bb->append(new proc_call_t(bb, task_enable));
    } else {
        balk(stmt, "unhandled node", __FILE__, __LINE__);
    }
}

/*! \brief instructions that define the requested identifier.
 */
instr_set_t& module_t::def_instrs(identifier_t identifier) {
    id_map_t::iterator it = def_map.find(identifier);

    if (it == def_map.end()) {
        util_t::fatal("id: \"" + identifier + "\"\n");
        assert(false && "failed to find def for requested id!");
    }

    return it->second;
}

/*! \brief instructions that use the requested identifier.
 */
instr_set_t& module_t::use_instrs(identifier_t identifier) {
    id_map_t::iterator it = use_map.find(identifier);

    if (it == use_map.end()) {
        util_t::fatal("id: \"" + identifier + "\"\n");
        assert(false && "failed to find use for requested id!");
    }

    return it->second;
}

/*! \brief don't use this basic block as an entry basic block.
 */
void module_t::remove_from_top_level_blocks(bb_t* bb) {
    top_level_blocks.erase(bb);
}

/*! \brief check whether this basic block already exists in the module.
 */
bool module_t::exists(bb_t* bb) {
    return std::find(basicblocks.begin(), basicblocks.end(), bb) !=
        basicblocks.end();
}

/*! \brief check whether 'lo' postdominates 'hi'.
 */
bool module_t::postdominates(bb_t* lo, bb_t* hi) {
    if (exists(lo) == false || exists(hi) == false) {
        assert(hi != nullptr);
        assert(false && "non-existent blocks as input to postdominates!");
        return false;
    }

    bb_set_t& postdom_set = postdominators.at(hi);
    return postdom_set.find(lo) != postdom_set.end();
}

/*! \brief retrieve immediate dominators of 'ref_bb'.
 */
bb_t* module_t::immediate_dominator(bb_t* ref_bb) {
    bb_map_t::iterator it = imm_dominator.find(ref_bb);
    if (it == imm_dominator.end()) {
        return nullptr;
    }

    return it->second;
}

/*! \brief retrieve immediate postdominators of 'ref_bb'.
 */
bb_t* module_t::immediate_postdominator(bb_t* ref_bb) {
    bb_map_t::iterator it = imm_postdominator.find(ref_bb);
    if (it == imm_postdominator.end()) {
        return nullptr;
    }

    return it->second;
}

/*! \brief find basic blocks that guard the execution of this basic blocks.
 */
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

/*! \brief set of ports (or arguments) used by this module.
 */
id_set_t& module_t::ports() {
    return arg_ports;
}

/*! \brief check whether the requested identifier is among the ports.
 */
bool module_t::port_exists(identifier_t id) {
    return arg_ports.find(id) != arg_ports.end();
}

identifier_t module_t::make_unique_bb_id(identifier_t id) {
    // create key if necessary.
    uint32_t counter = bb_id_map[id];
    bb_id_map[id] = counter + 1;

    char bb_name[1024];
    snprintf(bb_name, sizeof(bb_name), "%s.%u", id.c_str(), counter);

    return identifier_t(bb_name);
}

void module_t::add_def(identifier_t def_id, instr_t* def_instr) {
    def_map[def_id].insert(def_instr);
}

void module_t::add_use(identifier_t use_id, instr_t* use_instr) {
    use_map[use_id].insert(use_instr);
}

proc_decl_t* module_t::proc_decl_by_id(identifier_t id) {
    proc_decl_map_t::iterator it = proc_decls.find(id);
    if (it == proc_decls.end()) {
        return nullptr;
    }

    return it->second;
}

bool util_t::ordinary_statement(VeriStatement* stmt) {
    return dynamic_cast<VeriAssign*>(stmt) != nullptr ||
            dynamic_cast<VeriBlockingAssign*>(stmt) != nullptr ||
            dynamic_cast<VeriCaseStatement*>(stmt) != nullptr ||
            dynamic_cast<VeriDeAssign*>(stmt) != nullptr ||
            dynamic_cast<VeriDelayControlStatement*>(stmt) != nullptr ||
            dynamic_cast<VeriDisable*>(stmt) != nullptr ||
            dynamic_cast<VeriEventTrigger*>(stmt) != nullptr ||
            dynamic_cast<VeriNonBlockingAssign*>(stmt) != nullptr ||
            dynamic_cast<VeriWait*>(stmt) != nullptr;
}

bool util_t::ignored_statement(VeriStatement* stmt) {
    return dynamic_cast<VeriNullStatement*>(stmt) != nullptr ||
            dynamic_cast<VeriSystemTaskEnable*>(stmt) != nullptr;
}

uint64_t util_t::build_reachable_set(bb_t*& start_bb, bb_set_t& reachable) {
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

void util_t::clear_status() {
    util_t::plain("\r                                                        ");
    util_t::plain("\r");
}

void util_t::update_status(const char* string) {
    clear_status();
    util_t::plain(string);
}

void util_t::dump_set(id_set_t& id_set) {
    uint32_t col = 0;
    util_t::plain("\n    ");

    for (identifier_t id : id_set) {
        if (col + id.size() + 4 > 80) {
            col = 0;
            util_t::plain("\n    " + id + " ");
        } else {
            util_t::plain(id + " ");
        }

        col += id.size() + 4;
    }

    util_t::plain("\n\n");
}

proc_decl_t::proc_decl_t(bb_t* parent, VeriTaskDecl* task_decl) :
        instr_t(nullptr) {
    containing_bb = parent;
    module_t* module_ds = parent->parent();
    id = task_decl->GetSubprogramName()->GetName();

    uint32_t idx = 0;
    VeriStatement* statement = nullptr;
    begin_block = module_ds->create_empty_bb("begin", BB_HIDDEN, true);

    FOREACH_ARRAY_ITEM(task_decl->GetStatements(), idx, statement) {
        module_ds->process_statement(begin_block, statement);
    }

    VeriDataDecl* data_decl = nullptr;
    FOREACH_ARRAY_ITEM(task_decl->GetDeclarations(), idx, data_decl) {
        parse_data_decl(data_decl);
    }
}

proc_decl_t::proc_decl_t(bb_t* parent, VeriFunctionDecl* func_decl) :
        instr_t(nullptr) {
    containing_bb = parent;
    module_t* module_ds = parent->parent();
    id = func_decl->GetSubprogramName()->GetName();

    uint32_t idx = 0;
    VeriStatement* statement = nullptr;
    begin_block = module_ds->create_empty_bb("begin", BB_HIDDEN, true);

    FOREACH_ARRAY_ITEM(func_decl->GetStatements(), idx, statement) {
        module_ds->process_statement(begin_block, statement);
    }
}

void proc_decl_t::parse_data_decl(VeriDataDecl* data_decl) {
    uint32_t idx = 0;
    VeriIdDef* arg_id = nullptr;

    if (data_decl->IsIODecl()) {
        state_t state = STATE_UNKNOWN;

        switch (data_decl->GetDir()) {
            case VERI_INPUT:    state = STATE_DEF;              break;
            case VERI_OUTPUT:   state = STATE_USE;              break;
            case VERI_INOUT:    state = STATE_DEF | STATE_USE;  break;
        }

        FOREACH_ARRAY_ITEM(data_decl->GetIds(), idx, arg_id) {
            identifier_t port_id = arg_id->GetName();

            id_desc_t arg = { port_id, state };
            arguments.push_back(arg);
        }
    }
}

proc_decl_t::~proc_decl_t() {
    bb_set_t reachable_set;
    util_t::build_reachable_set(begin_block, reachable_set);

    for (bb_t* bb : reachable_set) {
        delete bb;
        bb = nullptr;
    }

    begin_block = nullptr;
    reachable_set.clear();
}

void proc_decl_t::dump() {
    util_t::plain("procedure " + id + " with body:\n");

    bb_set_t reachable_set;
    util_t::build_reachable_set(begin_block, reachable_set);

    for (bb_t* bb : reachable_set) {
        bb->dump();
        util_t::plain("\n");
    }
}

bool proc_decl_t::operator==(const instr_t& reference) {
    if (const proc_decl_t* ref = dynamic_cast<const proc_decl_t*>(&reference)) {
        return id == ref->id && containing_bb == ref->containing_bb;
    }

    return false;
}

identifier_t proc_decl_t::name() {
    return id;
}

id_desc_list_t& proc_decl_t::args() {
    return arguments;
}

proc_call_t::proc_call_t(bb_t* parent, VeriTaskEnable* task_enable) :
        instr_t(parent) {
    proc_name = task_enable->GetTaskName()->GetName();

    uint32_t idx = 0;
    VeriIdRef* id_ref = nullptr;

    FOREACH_ARRAY_ITEM(task_enable->GetArgs(), idx, id_ref) {
        identifier_t id = id_ref->GetId()->Name();
        args.push_back(id);
    }

    set_defs_and_uses();
}

void proc_call_t::set_defs_and_uses() {
    module_t* module_ds = parent()->parent();
    proc_decl_t* proc_decl = module_ds->proc_decl_by_id(proc_name);

    if (proc_decl == nullptr) {
        util_t::plain("referenced task: " + proc_name + "\n");
        assert(false && "failed to find task declaration!");

        return;
    }

    uint32_t actuals_size = args.size();
    uint32_t formals_size = proc_decl->args().size();

    if (actuals_size != formals_size) {
        assert(false && "argument count does not match between declaration and "
                "invocation!");
        return;
    }

    uint32_t idx = 0;

    for (id_desc_t formal_arg : proc_decl->args()) {
        identifier_t arg_id = args[idx];

        if (formal_arg.type & STATE_DEF) {
            // This is an input argument to the task.
            add_use(arg_id);

            // XXX: We probably need to make the 'use'
            // point inside the task or function.
        }

        if (formal_arg.type & STATE_USE) {
            // This is an output argument of the task.
            add_def(arg_id);
        }

        idx += 1;
    }
}

proc_call_t::~proc_call_t() {
    args.clear();
}

void proc_call_t::dump() {
}

bool proc_call_t::operator==(const instr_t& reference) {
    if (const proc_call_t* ref = dynamic_cast<const proc_call_t*>(&reference)) {
        return proc_name == ref->proc_name && args == ref->args;
    }

    return false;
}

const identifier_t util_t::k_reset = "\033[0m";
const identifier_t util_t::k_red = "\033[91m";
const identifier_t util_t::k_yellow = "\033[93m";
const identifier_t util_t::k_underline = "\033[4m";

const identifier_t util_t::k_warn = util_t::k_yellow + "[WARN]" +
        util_t::k_reset + " ";

const identifier_t util_t::k_fatal = util_t::k_red + "[FATAL]" +
        util_t::k_reset + " ";

void util_t::warn(identifier_t message) {
    std::cerr << k_warn << message;
}

void util_t::fatal(identifier_t message) {
    std::cerr << k_fatal << message;
}

void util_t::plain(identifier_t message) {
    std::cerr << k_reset << message;
}

void util_t::underline(identifier_t message) {
    std::cerr << k_underline << message << k_reset;
}
