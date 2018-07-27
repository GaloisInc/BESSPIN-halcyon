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

void destroy_module_map(module_map_t& module_map) {
    for (auto it = module_map.begin(); it != module_map.end(); it++) {
        module_t* module_ds = it->second;

        delete module_ds;
        module_ds = nullptr;
    }

    module_map.clear();
}

unsigned parse_modules(module_map_t& module_map) {
    MapIter map_iter;
    VeriModule* module = nullptr;

    FOREACH_VERILOG_MODULE(map_iter, module) {
        module_t* module_ds = new module_t(module);
        module_map.emplace(module_ds->name(), module_ds);
    }

    for (auto it = module_map.begin(); it != module_map.end(); it++) {
        module_t* module_ds = it->second;

        module_ds->resolve_links(module_map);
        module_ds->build_def_use_chains();
    }

    return module_map.size();
}

bool analyze_file(const char* filename) {
    if (veri_file::Analyze(filename, veri_file::SYSTEM_VERILOG) == false) {
        assert(false && "failed to analyze file!");
        return false;
    }

    return true;
}

enum {
    DEP_TIMING = 0,
    DEP_ORDINARY,
};

typedef struct tag_dependence_t {
    state_t type;
    identifier_t id;

    const bool operator<(const struct tag_dependence_t& reference) const {
        return id < reference.id;
    }
} dependence_t;

typedef std::set<dependence_t> dep_set_t;

void add_new_ids(id_set_t& ids, dep_set_t& workset, dep_set_t& seen_set,
        state_t dependence_type) {
    for (identifier_t id_use : ids) {
        bool found = false;

        for (dependence_t dependence : seen_set) {
            if (dependence.id == id_use) {
                found = true;
                break;
            }
        }

        if (found == false) {
            dependence_t dependence = { dependence_type, id_use };
            workset.insert(dependence);
            seen_set.insert(dependence);
        }
    }
}

void gather_implicit_dependencies(instr_t* instr, dependence_t& dependence,
        dep_set_t& workset, dep_set_t& seen_set) {
    bb_t* bb = instr->parent();
    module_t* module_ds = bb->parent();

    bb_set_t guard_blocks;
    module_ds->populate_guard_blocks(bb, guard_blocks);

    for (bb_t* guard_block : guard_blocks) {
        cmpr_t* comparison = guard_block->comparison();
        assert(comparison != nullptr && "invalid comparison!");

        add_new_ids(comparison->uses(), workset, seen_set, dependence.type);
    }
}

void gather_timing_dependencies(instr_t* instr, dep_set_t& workset,
        dep_set_t& seen_set) {
    bb_t* bb = instr->parent();
    bb_t* entry_block = bb->entry_block();
    instr_list_t& instrs = entry_block->instrs();
    assert(instrs.size() > 0 && "invalid basic block!");

    instr_list_t::iterator it = instrs.begin();
    instr_t* first_instr = *it;

    trigger_t* trigger = dynamic_cast<trigger_t*>(first_instr);
    assert(trigger != nullptr && "invalid always block!");

    add_new_ids(trigger->trigger_ids(), workset, seen_set, DEP_TIMING);
}

bool gather_dependencies(instr_t* instr, dep_set_t& workset,
        dep_set_t& seen_set, dependence_t& dependence) {
    bb_t* bb = instr->parent();
    bb_t* entry_bb = bb->entry_block();
    module_t* module_ds = bb->parent();

    // Gather explicit dependencies.
    add_new_ids(instr->uses(), workset, seen_set, dependence.type);

    // Gather implicit dependencies.
    if (module_ds->postdominates(bb, entry_bb) == false) {
        gather_implicit_dependencies(instr, dependence, workset, seen_set);
    }

    // Gather timing dependencies.
    if (entry_bb->block_type() == BB_ALWAYS) {
        if (dependence.type == DEP_TIMING) {
            return true;
        }

        gather_timing_dependencies(instr, workset, seen_set);
    }

    return false;
}

void trace_definition(identifier_t identifier, module_t* module_ds) {
    module_ds->build_dominator_sets();

    dep_set_t workset;
    dep_set_t seen_set;

    dependence_t dependence = { DEP_ORDINARY, identifier };
    workset.insert(dependence);
    seen_set.insert(dependence);

    bool timing_leak = false;

    do {
        dep_set_t::iterator it = workset.begin();
        dependence_t dependence = *it;

        workset.erase(it);
        instr_set_t& instr_set = module_ds->def_instrs(dependence.id);

        for (instr_t* instr : instr_set) {
            if (gather_dependencies(instr, workset, seen_set, dependence)) {
                timing_leak = true;
            }
        }
    } while (timing_leak == false && workset.size() > 0);

    if (timing_leak == true) {
        std::cerr << "aaha!\n";
    } else {
        std::cerr << "meh.\n";
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "USAGE: " << argv[0] << " verilog-file";
        return 1;
    }

    const char* filename = argv[1];
    analyze_file(filename);

    module_map_t module_map;
    parse_modules(module_map);

    module_map_t::iterator it = module_map.find("MulDiv");
    assert(it != module_map.end() && "failed to find MulDiv module!");

    trace_definition("io_resp_valid", it->second);

    destroy_module_map(module_map);
    return 0 ;
}
