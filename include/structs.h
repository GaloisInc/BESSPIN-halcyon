
#ifndef STRUCTS_H_
#define STRUCTS_H_

#include <list>
#include <map>
#include <set>
#include <stdint.h>

#include <VeriStatement.h>

using namespace Verific;

class instr_t;
class bb_t;
class module_t;
class module_call_t;

typedef VeriExpression* expr_t;
typedef const char* identifier_t;
typedef VeriModuleInstantiation* instance_t;

typedef std::set<bb_t*> bb_set_t;
typedef std::set<expr_t> expr_set_t;
typedef std::set<identifier_t> id_set_t;
typedef std::set<instr_t*> instr_ptr_set_t;
typedef std::set<instance_t> instance_set_t;

typedef std::list<bb_t*> bb_list_t;
typedef std::list<instr_t> instr_list_t;

typedef std::map<bb_t*, bb_t*> bb_map_t;
typedef std::map<bb_t*, bb_set_t> bb_set_map_t;
typedef std::map<std::string, uint32_t> bb_id_map_t;
typedef std::map<identifier_t, instr_ptr_set_t> id_map_t;

typedef struct {
    id_set_t use_set;

    identifier_t module_name;
    identifier_t remote_endpoint;
} conn_t;

typedef std::list<conn_t> conn_list_t;

enum {
    DST_DEFS = 0,
    DST_USES,
    DST_UNKNOWN
};

typedef struct {
    identifier_t name;
    uint8_t type;
} id_desc_t;

typedef std::list<id_desc_t> id_desc_list_t;

void describe_expression(VeriExpression*, id_desc_list_t&, uint8_t);

class instr_t {
  private:
    VeriStatement* instr;
    VeriNetRegAssign* assign;
    id_set_t def_set, use_set;

  public:
    explicit instr_t(VeriStatement*);
    explicit instr_t(VeriNetRegAssign*);

    void parse_stmt(VeriStatement*);
    void parse_expression(VeriExpression*, uint8_t);

    VeriStatement* get_stmt();
    VeriNetRegAssign* get_assign();

    id_set_t& defs();
    id_set_t& uses();

    bool operator==(const instr_t& ref) {
        return instr == ref.instr && def_set == ref.def_set &&
                use_set == ref.use_set;
    }
};

class bb_t {
  private:
    std::string name;
    expr_t condition;
    bb_set_t predecessors;
    instr_list_t instr_list;

    bb_t* successor_left;
    bb_t* successor_right;

    bool add_predecessor(bb_t*);

  public:
    explicit bb_t(const std::string&);
    ~bb_t();

    // disable copy constructor.
    bb_t(bb_t&);

    bool exists(instr_t&);

    bool append(expr_t);
    bool append(instr_t);

    bool set_left_successor(bb_t*&);
    bool set_right_successor(bb_t*&);

    bb_set_t& preds();
    instr_list_t& instrs();

    bb_t* left_successor();
    bb_t* right_successor();

    uint64_t pred_count();
    uint64_t succ_count();

    void dump();
    const std::string& get_name();
};

class module_t {
  private:
    std::string name;
    conn_list_t conn_list;
    instance_set_t instance_set;

    bb_id_map_t bb_id_map;
    bb_list_t basicblocks;
    bb_set_map_t dominators;
    bb_set_map_t postdominators;

    bb_map_t imm_dominator;
    bb_map_t imm_postdominator;

    id_map_t def_map;
    id_map_t use_map;
    instr_list_t instrs;

    void build_dominator_sets(bb_set_t&);
    void intersect(bb_set_t&, bb_set_t&);
    bool update_dominators(bb_t*, bb_set_t&);
    bool update_postdominators(bb_t*, bb_set_t&);
    uint64_t build_reachable_set(bb_t*&, bb_set_t&);

    bb_t* find_imm_dominator(bb_t*, bb_set_t&);
    bb_t* find_imm_postdominator(bb_t*, bb_set_t&);

  public:
    explicit module_t(const std::string&);
    ~module_t();

    // disable copy constructor.
    module_t(const module_t&);

    void dump();

    bool append(instr_t);
    void build_def_use_chains();
    void build_dominator_sets();
    void add_module_instance(instance_t);

    bb_t* create_empty_basicblock(const char*);
};

#endif  // STRUCTS_H_
