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

void trace_definition(identifier_t identifier, module_t* module_ds) {
    module_ds->build_dominator_sets();

    id_set_t trigger_ids;

    id_set_t workset;
    id_set_t seen_set;
    workset.insert(identifier);
    seen_set.insert(identifier);

    do {
        id_set_t::iterator it = workset.begin();
        identifier_t id = *it;

        workset.erase(it);

        instr_set_t& instr_set = module_ds->def_instrs(id);
        for (instr_t* instr : instr_set) {
            bb_t* parent_bb = instr->parent();
            bb_t* entry_bb = parent_bb->entry_block();

            if (module_ds->postdominates(parent_bb, entry_bb) == false) {
                trigger_ids.insert(id);
            }

            for (identifier_t id_use : instr->uses()) {
                if (seen_set.find(id_use) == seen_set.end()) {
                    workset.insert(id_use);
                    seen_set.insert(id_use);
                }
            }
        }
    } while (workset.size() > 0);

    if (trigger_ids.size() > 0) {
        std::cerr << "aaha!\n";
        for (identifier_t id : trigger_ids) {
            std::cerr << id << " ";
        }

        std::cerr << "\n";
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
