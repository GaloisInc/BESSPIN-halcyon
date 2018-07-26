#include <cassert>
#include <iostream>

#include <Array.h>
#include <Map.h>
#include <veri_file.h>
#include <VeriId.h>
#include <VeriExpression.h>
#include <VeriMisc.h>
#include <VeriModule.h>
#include <VeriStatement.h>
#include <veri_tokens.h>

#include "structs.h"

using namespace Verific;
module_map_t module_map;

void process_module(VeriModule*);
void process_module_item(VeriModuleItem*, module_t*);
void process_statement(module_t*, bb_t*&, VeriStatement*);

void balk(VeriTreeNode* tree_node, const std::string& msg) {
    std::cerr << msg << " [" << tree_node << "]:\n";
    tree_node->PrettyPrintXml(std::cerr, 100);

    assert(false);
}

void process_module(VeriModule* module) {
    unsigned idx = 0;
    VeriModuleItem* module_item = nullptr;

    module_t* module_ds = new module_t(module->GetName());

    VeriIdDef* id_def = nullptr;
    Array* params = module->GetParameters();

    if (params != nullptr && params->Size() > 0) {
        bb_t* bb_params = module_ds->create_empty_basicblock("params");

        FOREACH_ARRAY_ITEM(params, idx, id_def) {
            bb_params->append(new param_t(id_def->GetName()));
        }
    }

    FOREACH_ARRAY_ITEM(module->GetModuleItems(), idx, module_item) {
        process_module_item(module_item, module_ds);
    }

    Array* port_connects = module->GetPortConnects();

    if (port_connects != nullptr && port_connects->Size() > 0) {
        VeriAnsiPortDecl* decl = nullptr;
        bb_t* arg_bb = module_ds->create_empty_basicblock("args");

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
                module_ds->add_arg(arg_id->GetName(), state);
                arg_bb->append(new arg_t(arg_id->GetName(), state));
            }
        }
    }

    module_map.emplace(std::string(module->GetName()), module_ds);
}

void process_module_item(VeriModuleItem* module_item, module_t* module_ds) {
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
        bb_t* bb = module_ds->create_empty_basicblock("always");
        process_statement(module_ds, bb, always->GetStmt());
    } else if (auto continuous = dynamic_cast<VeriContinuousAssign*>(module_item)) {
        /// "assign" outside an "always" block.
        uint32_t idx = 0;
        VeriNetRegAssign* net_reg_assign = nullptr;

        FOREACH_ARRAY_ITEM(continuous->GetNetAssigns(), idx, net_reg_assign) {
            module_ds->append(new assign_t(net_reg_assign));
        }
    } else if (auto func_decl = dynamic_cast<VeriFunctionDecl*>(module_item)) {
        // Treat this just like any other module.
        VeriName* veri_name = func_decl->GetSubprogramName();
        identifier_t function_name = veri_name->GetName();

        module_t* module_ds = new module_t(function_name);
        module_ds->set_function();

        bb_t* arg_bb = module_ds->create_empty_basicblock("args");

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
                arg_bb->append(new arg_t(arg_id->GetName(), state));
            }
        }

        module_map.emplace(function_name, module_ds);
    } else if (auto initial = dynamic_cast<VeriInitialConstruct*>(module_item)) {
        bb_t* bb = module_ds->create_empty_basicblock("initial");
        process_statement(module_ds, bb, initial->GetStmt());
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
                module_ds->update_arg(arg_id->GetName(), state);
            }
        }
    } else if (auto def_param = dynamic_cast<VeriDefParam*>(module_item)) {
        // TODO
    } else if (auto module = dynamic_cast<VeriModule*>(module_item)) {
        process_module(module);
    } else if (auto inst = dynamic_cast<VeriModuleInstantiation*>(module_item)) {
        const char* module_name = inst->GetModuleName();

        uint32_t idx = 0;
        VeriInstId* module_instance = nullptr;

        FOREACH_ARRAY_ITEM(inst->GetInstances(), idx, module_instance) {
            module_ds->append(new invoke_t(module_instance, module_name));
        }
    } else if (auto stmt = dynamic_cast<VeriStatement*>(module_item)) {
        bb_t* bb = module_ds->create_empty_basicblock(".dangling");
        process_statement(module_ds, bb, stmt);
    } else {
        balk(module_item, "unhandled node");
    }
}

void process_statement(module_t* module, bb_t*& bb, VeriStatement* stmt) {
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
        bb->append(new stmt_t(assign_stmt));
    } else if (auto blocking_assign = dynamic_cast<VeriBlockingAssign*>(stmt)) {
        /// assignment using "=" without the assign keyword
        bb->append(new stmt_t(blocking_assign));
    } else if (auto case_stmt = dynamic_cast<VeriCaseStatement*>(stmt)) {
        bb->append(new stmt_t(case_stmt));
    } else if (auto code_block = dynamic_cast<VeriCodeBlock*>(stmt)) {
        VeriStatement* __stmt = nullptr;

        FOREACH_ARRAY_ITEM(code_block->GetStatements(), idx, __stmt) {
            process_statement(module, bb, __stmt);
        }
    } else if (auto cond_stmt = dynamic_cast<VeriConditionalStatement*>(stmt)) {
        bb->append(new cmpr_t(cond_stmt->GetIfExpr()));
        bb_t* merge_bb = module->create_empty_basicblock("merge");

        bb_t* then_bb = module->create_empty_basicblock("then");
        bb->set_left_successor(then_bb);

        process_statement(module, then_bb, stmt->GetThenStmt());
        then_bb->set_left_successor(merge_bb);

        VeriStatement* inner_statement = stmt->GetElseStmt();
        if (inner_statement != nullptr) {
            bb_t* else_bb = module->create_empty_basicblock("else");
            bb->set_right_successor(else_bb);

            process_statement(module, else_bb, inner_statement);
            else_bb->set_left_successor(merge_bb);
        }

        bb = merge_bb;
    } else if (auto de_assign_stmt = dynamic_cast<VeriDeAssign*>(stmt)) {
        /// "deassign" keyword to undo the last blocking assignment.
        bb->append(new stmt_t(de_assign_stmt));
    } else if (auto delay_control = dynamic_cast<VeriDelayControlStatement*>(stmt)) {
        /// "#" followed by the delay followed by a statement.
        /// useful for tracking timing leakage.
        bb->append(new stmt_t(delay_control));
    } else if (auto disable_stmt = dynamic_cast<VeriDisable*>(stmt)) {
        /// "disable" keyword for jumping to a specific point in the code.
        /// useful for tracking timing leakage.
        bb->append(new stmt_t(disable_stmt));
    } else if (auto event_ctrl = dynamic_cast<VeriEventControlStatement*>(stmt)) {
        /// "@" expression for specifying when to trigger some actions.
        /// useful for tracking timing and non-timing leakage.
        process_statement(module, bb, event_ctrl->GetStmt());
    } else if (auto event_trigger = dynamic_cast<VeriEventTrigger*>(stmt)) {
        /// "->" or "->>" expression to specify an event to trigger.
        /// useful for tracking timing and non-timing leakage.
        bb->append(new stmt_t(event_trigger));
    } else if (auto force = dynamic_cast<VeriForce*>(stmt)) {
        /// used only during simulation (SystemVerilog?), ignore for now.
        // TODO
    } else if (auto gen_var_assign = dynamic_cast<VeriGenVarAssign*>(stmt)) {
        // TODO
    } else if (auto loop = dynamic_cast<VeriLoop*>(stmt)) {
        // TODO
    } else if (auto non_blocking_assign = dynamic_cast<VeriNonBlockingAssign*>(stmt)) {
        /// assignment using "<=" without the assign keyword
        bb->append(new stmt_t(non_blocking_assign));
    } else if (auto par_block = dynamic_cast<VeriParBlock*>(stmt)) {
        VeriStatement* __stmt = nullptr;

        FOREACH_ARRAY_ITEM(par_block->GetStatements(), idx, __stmt) {
            if (__stmt != nullptr) {
                process_statement(module, bb, __stmt);
            }
        }
    } else if (auto seq_block = dynamic_cast<VeriSeqBlock*>(stmt)) {
        VeriStatement* __stmt = nullptr;

        FOREACH_ARRAY_ITEM(seq_block->GetStatements(), idx, __stmt) {
            if (__stmt != nullptr) {
                process_statement(module, bb, __stmt);
            }
        }
    } else if (auto sys_task = dynamic_cast<VeriSystemTaskEnable*>(stmt)) {
        /// used for generating inputs and output, ignore.
    } else if (auto task = dynamic_cast<VeriTaskEnable*>(stmt)) {
        /// similar to system tasks, ignore?
    } else if (auto wait = dynamic_cast<VeriWait*>(stmt)) {
        /// "wait" keyword for waiting for an expression to be true.
        /// useful for tracking timing leakage.
        bb->append(new stmt_t(wait));
    } else {
        balk(stmt, "unhandled node");
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "USAGE: " << argv[0] << " verilog-file";
        return 1;
    }

    if (veri_file::Analyze(argv[1], veri_file::SYSTEM_VERILOG) == false) {
        return 2;
    }

    MapIter map_iter;
    VeriModule* module = nullptr;

    FOREACH_VERILOG_MODULE(map_iter, module) {
        process_module(module);
    }

    for (auto it = module_map.begin(); it != module_map.end(); it++) {
        module_t* module_ds = it->second;

        module_ds->resolve_links(module_map);
        module_ds->build_def_use_chains();

        module_ds->print_undef_ids();
    }

    for (auto it = module_map.begin(); it != module_map.end(); it++) {
        module_t* module_ds = it->second;

        delete module_ds;
        module_ds = nullptr;
    }

    module_map.clear();
    return 0 ;
}
