
#ifndef DEPENDENCE_H_
#define DEPENDENCE_H_

#include "structs.h"

class dep_analysis_t {
  private:
    enum {
        DEP_TIMING = 0,
        DEP_ORDINARY,
    };

    typedef struct tag_dependence_t {
        state_t type;
        identifier_t id;
        module_t* module_ds;

        const bool operator<(const struct tag_dependence_t& ref) const {
            return std::tie(module_ds, id) < std::tie(ref.module_ds, ref.id);
        }
    } dependence_t;

    typedef std::set<dependence_t> dep_set_t;

    id_set_t arg_set;
    dep_set_t workset, seen_set;

    void add_new_ids(id_set_t&, state_t, module_t*);

    void gather_timing_dependencies(instr_t*);
    void gather_implicit_dependencies(instr_t*, dependence_t&);
    void gather_inter_module_dependencies(invoke_t*, state_t, module_map_t&);
    bool gather_dependencies(instr_t* instr, dependence_t& dependence,
            module_map_t&);

  public:
    id_set_t& leaking_args();
    bool trace_timing_leak(identifier_t, identifier_t, module_map_t&);
};

#endif  // DEPENDENCE_H_
