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

#include "structs.h"

using namespace Verific;

void process_module(VeriModule*);
void process_module_item(VeriModuleItem*, module_t&);
void process_statement(module_t&, bb_t*&, VeriStatement*);

void balk(VeriTreeNode* tree_node, const std::string& msg) {
    std::cerr << msg << " [" << tree_node << "]:\n";
    tree_node->PrettyPrintXml(std::cerr, 100);

    assert(false);
}

void process_module(VeriModule* module) {
    unsigned idx = 0;
    VeriModuleItem* module_item = nullptr;

    module_t module_ds(module->GetName());

    FOREACH_ARRAY_ITEM(module->GetModuleItems(), idx, module_item) {
        process_module_item(module_item, module_ds);
    }

    module_ds.dump();
}

void process_module_item(VeriModuleItem* module_item, module_t& module_ds) {
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
            dynamic_cast<VeriFunctionDecl*>(module_item) != nullptr ||
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
        bb_t* bb = module_ds.create_empty_basicblock("always");
        process_statement(module_ds, bb, always->GetStmt());
    } else if (auto continuous_assign = dynamic_cast<VeriContinuousAssign*>(module_item)) {
        /// "assign" outside an "always" block.
        module_ds.append_assignment(continuous_assign);
    } else if (auto initial = dynamic_cast<VeriInitialConstruct*>(module_item)) {
        bb_t* bb = module_ds.create_empty_basicblock("initial");
        process_statement(module_ds, bb, initial->GetStmt());
    } else if (auto data_decl = dynamic_cast<VeriDataDecl*>(module_item)) {
#if 0
        unsigned idx = 0;
        VeriIdDef* decl_id = nullptr;

        FOREACH_ARRAY_ITEM(data_decl->GetIds(), idx, decl_id) {
            std::cerr << decl_id->GetName() << "\n";
        }
#endif
    } else if (auto def_param = dynamic_cast<VeriDefParam*>(module_item)) {
        // TODO
    } else if (auto module = dynamic_cast<VeriModule*>(module_item)) {
        process_module(module);
    } else if (auto module_inst = dynamic_cast<VeriModuleInstantiation*>(module_item)) {
        // TODO
    } else if (auto stmt = dynamic_cast<VeriStatement*>(module_item)) {
        bb_t* bb = module_ds.create_empty_basicblock(".dangling");
        process_statement(module_ds, bb, stmt);
    } else {
        balk(module_item, "unhandled node");
    }
}

void process_statement(module_t& module, bb_t*& bb, VeriStatement* stmt) {
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
        bb->append(assign_stmt);
    } else if (auto blocking_assign = dynamic_cast<VeriBlockingAssign*>(stmt)) {
        /// assignment using "=" without the assign keyword
        bb->append(blocking_assign);
    } else if (auto case_stmt = dynamic_cast<VeriCaseStatement*>(stmt)) {
        /// "case", "casex", or "casez" statement
        VeriCaseItem* case_item = nullptr;

        FOREACH_ARRAY_ITEM(case_stmt->GetCaseItems(), idx, case_item) {
            VeriStatement* inner_statement = case_item->GetStmt();
            // TODO: process_statement(inner_statement);
        }
    } else if (auto code_block = dynamic_cast<VeriCodeBlock*>(stmt)) {
        VeriStatement* __stmt = nullptr;

        FOREACH_ARRAY_ITEM(code_block->GetStatements(), idx, __stmt) {
            process_statement(module, bb, __stmt);
        }
    } else if (auto cond_stmt = dynamic_cast<VeriConditionalStatement*>(stmt)) {
        bb->append(cond_stmt->GetIfExpr());
        bb_t* merge_bb = module.create_empty_basicblock("merge");

        bb_t* then_bb = module.create_empty_basicblock("then");
        bb->set_left_successor(then_bb);

        process_statement(module, then_bb, stmt->GetThenStmt());
        then_bb->set_left_successor(merge_bb);

        VeriStatement* inner_statement = stmt->GetElseStmt();
        if (inner_statement != nullptr) {
            bb_t* else_bb = module.create_empty_basicblock("else");
            bb->set_right_successor(else_bb);

            process_statement(module, else_bb, inner_statement);
            else_bb->set_left_successor(merge_bb);
        }

        bb = merge_bb;
    } else if (auto de_assign_stmt = dynamic_cast<VeriDeAssign*>(stmt)) {
        /// "deassign" keyword to undo the last blocking assignment.
        bb->append(de_assign_stmt);
    } else if (auto delay_control = dynamic_cast<VeriDelayControlStatement*>(stmt)) {
        /// "#" followed by the delay followed by a statement.
        /// useful for tracking timing leakage.
        bb->append(delay_control);
    } else if (auto disable_stmt = dynamic_cast<VeriDisable*>(stmt)) {
        /// "disable" keyword for jumping to a specific point in the code.
        /// useful for tracking timing leakage.
        bb->append(disable_stmt);
    } else if (auto event_ctrl = dynamic_cast<VeriEventControlStatement*>(stmt)) {
        /// "@" expression for specifying when to trigger some actions.
        /// useful for tracking timing and non-timing leakage.
        process_statement(module, bb, event_ctrl->GetStmt());
    } else if (auto event_trigger = dynamic_cast<VeriEventTrigger*>(stmt)) {
        /// "->" or "->>" expression to specify an event to trigger.
        /// useful for tracking timing and non-timing leakage.
        // TODO: process_event_trigger_stmt(event_trigger);
    } else if (auto force = dynamic_cast<VeriForce*>(stmt)) {
        /// used only during simulation (SystemVerilog?), ignore for now.
        // TODO: process_force_stmt(force);
    } else if (auto gen_var_assign = dynamic_cast<VeriGenVarAssign*>(stmt)) {
        // TODO: process_gen_var_assign_stmt(gen_var_assign);
    } else if (auto loop = dynamic_cast<VeriLoop*>(stmt)) {
        // TODO: process_loop_stmt(loop);
    } else if (auto non_blocking_assign = dynamic_cast<VeriNonBlockingAssign*>(stmt)) {
        /// assignment using "<=" without the assign keyword
        bb->append(non_blocking_assign);
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
        bb->append(wait);
    } else {
        balk(stmt, "unhandled node");
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "USAGE: " << argv[0] << " verilog-file";
        return 1;
    }

    if (veri_file::Analyze(argv[1]) == false) {
        return 2;
    }

    MapIter map_iter;
    VeriModule* module = nullptr;

    FOREACH_VERILOG_MODULE(map_iter, module) {
        process_module(module);
    }

    return 0 ;
}
