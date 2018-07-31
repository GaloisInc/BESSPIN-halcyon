#include "dependence.h"

void dep_analysis_t::add_new_ids(id_set_t& ids, state_t dependence_type,
        module_t* module_ds) {
    for (identifier_t id : ids) {
        bool found = false;

        for (dependence_t dependence : seen_set) {
            if (dependence.id == id && dependence.module_ds == module_ds &&
                    dependence.type == dependence_type) {
                found = true;
                break;
            }
        }

        if (found == false) {
            if (module_ds->is_port(id)) {
                if (dependence_type == DEP_TIMING) {
                    timing_deps.insert(module_ds->name() + "." + id);
                } else {
                    non_timing_deps.insert(module_ds->name() + "." + id);
                }
            }

            dependence_t dependence = { dependence_type, id, module_ds };
            workset.insert(dependence);
            seen_set.insert(dependence);
        }
    }
}

void dep_analysis_t::gather_implicit_dependencies(instr_t* instr,
        state_t& dependence_type) {
    bb_t* bb = instr->parent();
    module_t* module_ds = bb->parent();

    bb_set_t guard_blocks;
    module_ds->populate_guard_blocks(bb, guard_blocks);

    for (bb_t* guard_block : guard_blocks) {
        cmpr_t* comparison = guard_block->comparison();
        assert(comparison != nullptr && "invalid comparison!");

        add_new_ids(comparison->uses(), dependence_type, module_ds);
    }
}

void dep_analysis_t::gather_timing_dependencies(instr_t* instr) {
    bb_t* bb = instr->parent();
    bb_t* entry_block = bb->entry_block();
    module_t* module_ds = entry_block->parent();

    instr_list_t& instrs = entry_block->instrs();
    assert(instrs.size() > 0 && "invalid basic block!");

    instr_list_t::iterator it = instrs.begin();
    instr_t* first_instr = *it;

    trigger_t* trigger = dynamic_cast<trigger_t*>(first_instr);
    assert(trigger != nullptr && "invalid always block!");

    add_new_ids(trigger->trigger_ids(), DEP_TIMING, module_ds);
}

void dep_analysis_t::gather_inter_module_dependencies(invoke_t* invoke,
        state_t dependence_type, module_map_t& module_map) {
    module_map_t::iterator it = module_map.find(invoke->module_name());
    assert(it != module_map.end() && "failed to find invoked module!");

    module_t* module_ds = it->second;
    id_list_t& ports = module_ds->ports();

    // Find which arguments in the caller are tainted, then
    // transfer taint to the corresponding arguments in the callee.
    id_set_t new_taints;
    unsigned counter = 0;

    for (conn_t connection : invoke->connections()) {
        for (identifier_t id : connection.id_set) {
            for (dependence_t dependence : seen_set) {
                if (dependence.id == id) {
                    new_taints.insert(ports[counter]);
                    break;
                }
            }
        }

        counter += 1;
    }

    if (new_taints.size() > 0) {
        module_ds->build_dominator_sets();
    }

    add_new_ids(new_taints, dependence_type, module_ds);
}

bool dep_analysis_t::gather_dependencies(instr_t* instr,
        dependence_t& dependence, module_map_t& module_map) {
    bool timing_leakage = false;

    bb_t* bb = instr->parent();
    bb_t* entry_bb = bb->entry_block();
    module_t* module_ds = bb->parent();

    if (invoke_t* invoke = dynamic_cast<invoke_t*>(instr)) {
        gather_inter_module_dependencies(invoke, dependence.type, module_map);
    }

    // Gather explicit dependencies.
    add_new_ids(instr->uses(), dependence.type, module_ds);

    // Gather implicit dependencies.
    if (module_ds->postdominates(bb, entry_bb) == false) {
        gather_implicit_dependencies(instr, dependence.type);
    }

    // Gather timing dependencies.
    if (entry_bb->block_type() == BB_ALWAYS) {
        if (dependence.type == DEP_TIMING) {
            timing_leakage = true;
        }

        gather_timing_dependencies(instr);
    }

    return timing_leakage;
}

/*! \brief analyze the requested fieldname for leakage.
 */
bool dep_analysis_t::compute_dependencies(identifier_t module_name,
        identifier_t identifier, module_map_t& module_map) {
    workset.clear();
    seen_set.clear();

    timing_deps.clear();
    non_timing_deps.clear();

    module_map_t::iterator it = module_map.find(module_name);
    assert(it != module_map.end() && "failed to find requested module!");

    module_t* module_ds = it->second;

    fprintf(stderr, "\r                                                     ");
    fprintf(stderr, "\rbuilding dominator trees ... ");

    module_ds->build_dominator_sets();

    fprintf(stderr, "\r                                                     ");
    fprintf(stderr, "\rtracing definitions ... ");

    dependence_t dependence = { DEP_ORDINARY, identifier, module_ds };
    workset.insert(dependence);
    seen_set.insert(dependence);

    do {
        dep_set_t::iterator it = workset.begin();

        dependence_t dependence = *it;
        module_t* module_ds = dependence.module_ds;

        workset.erase(it);
        instr_set_t& instr_set = module_ds->def_instrs(dependence.id);

        for (instr_t* instr : instr_set) {
            gather_dependencies(instr, dependence, module_map);
        }
    } while (workset.size() > 0);

    return timing_deps.size() > 0 || non_timing_deps.size() > 0;
}

/*! \brief list of module ports that are leaked through timing channels.
 */
id_set_t& dep_analysis_t::leaking_timing_deps() {
    return timing_deps;
}

/*! \brief list of module ports that are leaked through non-timing channels.
 */
id_set_t& dep_analysis_t::leaking_non_timing_deps() {
    return non_timing_deps;
}
