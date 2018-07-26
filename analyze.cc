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

    module_map_t module_map;

    FOREACH_VERILOG_MODULE(map_iter, module) {
        module_t* module_ds = new module_t(module);
        module_map.emplace(module_ds->name(), module_ds);
    }

    for (auto it = module_map.begin(); it != module_map.end(); it++) {
        module_t* module_ds = it->second;

        module_ds->resolve_links(module_map);
        module_ds->build_def_use_chains();
    }

    for (auto it = module_map.begin(); it != module_map.end(); it++) {
        module_t* module_ds = it->second;

        delete module_ds;
        module_ds = nullptr;
    }

    module_map.clear();
    return 0 ;
}
