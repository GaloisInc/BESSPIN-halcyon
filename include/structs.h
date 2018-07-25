
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
typedef std::string identifier_t;
typedef VeriModuleInstantiation* instance_t;

typedef std::set<bb_t*> bb_set_t;
typedef std::set<expr_t> expr_set_t;
typedef std::set<identifier_t> id_set_t;
typedef std::set<instr_t*> instr_ptr_set_t;
typedef std::set<instance_t> instance_set_t;

typedef std::list<bb_t*> bb_list_t;
typedef std::list<instr_t*> instr_list_t;

typedef std::map<bb_t*, bb_t*> bb_map_t;
typedef std::map<bb_t*, bb_set_t> bb_set_map_t;
typedef std::map<identifier_t, uint32_t> bb_id_map_t;
typedef std::map<identifier_t, module_t*> module_map_t;
typedef std::map<identifier_t, uint8_t> id_state_map_t;
typedef std::map<identifier_t, instr_ptr_set_t> id_map_t;

typedef struct {
    id_set_t id_set;
    identifier_t remote_endpoint;
} conn_t;

typedef std::list<conn_t> conn_list_t;

enum {
    STATE_DEF = 0,
    STATE_USE,
    STATE_DEF_AND_USE,
    STATE_UNKNOWN
};

typedef struct {
    identifier_t name;
    uint8_t type;
} id_desc_t;

typedef std::list<id_desc_t> id_desc_list_t;

class instr_t {
  private:
    id_set_t def_set, use_set;

  protected:
    void parse_statement(VeriStatement*);
    void parse_expression(VeriExpression*, uint8_t);
    void describe_expression(VeriExpression*, id_desc_list_t&, uint8_t);

  public:
    id_set_t& defs();
    id_set_t& uses();

    virtual bool operator==(const instr_t&) = 0;
};

class stmt_t : public instr_t {
  private:
    VeriStatement* statement;

  public:
    explicit stmt_t(VeriStatement*);

    VeriStatement* get_statement();
    virtual bool operator==(const instr_t&);
};

class assign_t : public instr_t {
  private:
    VeriNetRegAssign* assignment;

  public:
    explicit assign_t(VeriNetRegAssign*);

    VeriNetRegAssign* get_assignment();
    virtual bool operator==(const instr_t&);
};

class invocation_t : public instr_t {
  private:
    conn_list_t connections;
    identifier_t module_name;
    VeriInstId* instantiation;

    void parse_invocation();

  public:
    invocation_t(VeriInstId*, identifier_t);
    virtual bool operator==(const instr_t&);
};

class bb_t {
  private:
    identifier_t name;
    expr_t condition;
    bb_set_t predecessors;
    instr_list_t instr_list;

    bb_t* successor_left;
    bb_t* successor_right;

    bool add_predecessor(bb_t*);

  public:
    explicit bb_t(const identifier_t&);
    ~bb_t();

    // disable copy constructor.
    bb_t(bb_t&);

    bool exists(instr_t*);

    bool append(expr_t);
    bool append(instr_t*);

    bool set_left_successor(bb_t*&);
    bool set_right_successor(bb_t*&);

    bb_set_t& preds();
    instr_list_t& instrs();

    bb_t* left_successor();
    bb_t* right_successor();

    uint64_t pred_count();
    uint64_t succ_count();

    void dump();
    const identifier_t& get_name();
};

class module_t {
  private:
    identifier_t name;
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
    id_state_map_t arg_states;

    void build_def_use_chains();
    void build_dominator_sets(bb_set_t&);
    void intersect(bb_set_t&, bb_set_t&);
    bool update_dominators(bb_t*, bb_set_t&);
    bool update_postdominators(bb_t*, bb_set_t&);
    void augment_chains_with_links(module_map_t&);
    uint64_t build_reachable_set(bb_t*&, bb_set_t&);

    bb_t* find_imm_dominator(bb_t*, bb_set_t&);
    bb_t* find_imm_postdominator(bb_t*, bb_set_t&);

  public:
    explicit module_t(const identifier_t&);
    ~module_t();

    // disable copy constructor.
    module_t(const module_t&);

    void dump();

    bool append(instr_t*);
    void build_dominator_sets();
    void resolve_module_links(module_map_t&);
    void add_module_argument(identifier_t, uint8_t);

    bb_t* create_empty_basicblock(identifier_t);
};

#endif  // STRUCTS_H_
