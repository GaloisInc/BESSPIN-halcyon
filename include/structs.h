
#ifndef STRUCTS_H_
#define STRUCTS_H_

#include <list>
#include <map>
#include <set>
#include <vector>

#include <stdint.h>

#include <VeriStatement.h>

using namespace Verific;

class instr_t;
class bb_t;
class module_t;
class module_call_t;

typedef uint8_t state_t;
typedef VeriExpression* expr_t;
typedef std::string identifier_t;
typedef VeriModuleInstantiation* instance_t;

typedef std::set<bb_t*> bb_set_t;
typedef std::set<expr_t> expr_set_t;
typedef std::set<instr_t*> instr_set_t;
typedef std::set<identifier_t> id_set_t;
typedef std::set<instance_t> instance_set_t;

typedef std::list<bb_t*> bb_list_t;
typedef std::list<instr_t*> instr_list_t;

typedef std::vector<identifier_t> id_list_t;

typedef std::map<bb_t*, bb_t*> bb_map_t;
typedef std::map<bb_t*, bb_set_t> bb_set_map_t;
typedef std::map<identifier_t, uint32_t> bb_id_map_t;
typedef std::map<identifier_t, instr_set_t> id_map_t;
typedef std::map<identifier_t, module_t*> module_map_t;
typedef std::map<identifier_t, state_t> id_state_map_t;

typedef struct {
    state_t state;
    id_set_t id_set;
    identifier_t remote_endpoint;
} conn_t;

typedef std::list<conn_t> conn_list_t;

enum {
    STATE_UNKNOWN   = 0,
    STATE_DEF       = 1,
    STATE_USE       = 2,
};

enum {
    BB_ALWAYS = 0,
    BB_PARAMS,
    BB_ARGS,
    BB_CONT_ASSIGNMENT,
    BB_INITIAL,
    BB_DANGLING,
    BB_ORDINARY,
};

typedef struct {
    identifier_t name;
    state_t type;
} id_desc_t;

typedef std::list<id_desc_t> id_desc_list_t;

/*!
 * Instruction (abstract) class.
 */
class instr_t {
  private:
    bb_t* containing_bb;

  protected:
    id_set_t def_set, use_set;

    void parse_statement(VeriStatement*);
    void parse_expression(VeriExpression*, state_t);

  public:
    instr_t(bb_t*);
    instr_t(const instr_t&) = delete;

    ~instr_t();

    bb_t* parent();
    id_set_t& defs();
    id_set_t& uses();
    static void describe_expr(VeriExpression*, id_desc_list_t&, state_t);

    virtual void dump() = 0;
    virtual bool operator==(const instr_t&) = 0;
};

/*!
 * Class that represents a module port.
 */
class arg_t : public instr_t {
  private:
    state_t arg_state;
    identifier_t arg_name;

  public:
    arg_t(const arg_t&) = delete;
    explicit arg_t(bb_t*, identifier_t, state_t);

    virtual void dump();
    virtual bool operator==(const instr_t&);
};

/*!
 * Class that represents a parameter constant.
 */
class param_t : public instr_t {
  private:
    identifier_t param_name;

  public:
    param_t(const param_t&) = delete;
    explicit param_t(bb_t*, identifier_t);

    virtual void dump();
    virtual bool operator==(const instr_t&);
};

/*!
 * Class that represents identifiers used as triggers in 'always' blocks.
 */
class trigger_t : public instr_t {
  private:
    id_set_t id_set;

  public:
    trigger_t(const trigger_t&) = delete;
    explicit trigger_t(bb_t*, id_set_t&);

    id_set_t& trigger_ids();

    virtual void dump();
    virtual bool operator==(const instr_t&);
};

/*!
 * Class that represents a generic statement.
 */
class stmt_t : public instr_t {
  private:
    VeriStatement* stmt;

  public:
    stmt_t(const stmt_t&) = delete;
    explicit stmt_t(bb_t*, VeriStatement*);

    virtual void dump();
    VeriStatement* statement();
    virtual bool operator==(const instr_t&);
};

/*!
 * Class that represents a continuous assignment.
 *
 * We need a class that is separate from stmt_t since the underlying Verific
 * object is not of the VeriStatement* type.
 */
class assign_t : public instr_t {
  private:
    VeriNetRegAssign* assign;

  public:
    assign_t(const assign_t&) = delete;
    explicit assign_t(bb_t*, VeriNetRegAssign*);

    virtual void dump();
    VeriNetRegAssign* assignment();
    virtual bool operator==(const instr_t&);
};

/*!
 * Class that represents a module instantiation.
 */
class invoke_t : public instr_t {
  private:
    conn_list_t conns;
    VeriInstId* mod_inst;
    identifier_t mod_name;

    void parse_invocation();

  public:
    explicit invoke_t(bb_t*, VeriInstId*, identifier_t);

    virtual void dump();
    identifier_t module_name();
    conn_list_t& connections();
    virtual bool operator==(const instr_t&);
};

/*!
 * Class that represents an expression used as predicate in if-statements and
 * loops.
 */
class cmpr_t : public instr_t {
  private:
    VeriExpression* cmpr;

  public:
    cmpr_t(const cmpr_t&) = delete;
    explicit cmpr_t(bb_t*, VeriExpression*);

    virtual void dump();
    VeriExpression* comparison();
    virtual bool operator==(const instr_t&);
};

/*!
 * Class that represents a basic block.
 */
class bb_t {
  private:
    bb_t* entry_bb;
    state_t bb_type;
    identifier_t bb_name;
    bb_set_t predecessors;
    instr_list_t instr_list;
    module_t* containing_module;

    bb_t* successor_left;
    bb_t* successor_right;

    bool add_predecessor(bb_t*);

  public:
    explicit bb_t(module_t*, const identifier_t&, state_t);
    ~bb_t();

    // disable copy constructor.
    bb_t(bb_t&);

    bool exists(instr_t*);
    bool append(instr_t*);
    bool set_left_successor(bb_t*&);
    bool set_right_successor(bb_t*&);

    bb_set_t& preds();
    module_t* parent();
    identifier_t name();
    cmpr_t* comparison();
    state_t block_type();
    instr_list_t& instrs();

    bb_t* entry_block();
    bb_t* left_successor();
    bb_t* right_successor();

    uint64_t pred_count();
    uint64_t succ_count();

    void dump();
    void set_entry_block(bb_t*);
};

/*!
 * Class that represents a module.
 */
class module_t {
  private:
    bool is_function;
    identifier_t mod_name;
    instance_set_t instance_set;

    bb_id_map_t bb_id_map;
    bb_list_t basicblocks;
    bb_set_map_t dominators;
    bb_set_t top_level_blocks;
    bb_set_map_t postdominators;

    bb_map_t imm_dominator;
    bb_map_t imm_postdominator;

    id_map_t def_map;
    id_map_t use_map;
    id_list_t arg_ports;
    id_state_map_t arg_states;

    state_t arg_state(identifier_t);
    void intersect(bb_set_t&, bb_set_t&);
    bool update_dominators(bb_t*, bb_set_t&);
    bool process_connection(conn_t&, invoke_t*);
    bool update_postdominators(bb_t*, bb_set_t&);
    void augment_chains_with_links(module_map_t&);
    uint64_t build_reachable_set(bb_t*&, bb_set_t&);

    void process_module_items(Array*);
    void process_module_ports(Array*);
    void process_module_params(Array*);
    void process_module_item(VeriModuleItem*);
    void process_statement(bb_t*&, VeriStatement*);

    bb_t* create_empty_bb(identifier_t, state_t);

    bb_t* find_imm_dominator(bb_t*, bb_set_t&);
    bb_t* find_imm_postdominator(bb_t*, bb_set_t&);

    void add_arg(identifier_t, state_t);
    void build_dominator_sets(bb_set_t&);
    void update_arg(identifier_t, state_t);
    void resolve_invoke(invoke_t*, module_map_t&);

  public:
    explicit module_t(VeriModule*&);
    ~module_t();

    // disable copy constructor.
    module_t(const module_t&);

    void dump();
    void print_undef_ids();
    void build_def_use_chains();
    void build_dominator_sets();
    void resolve_links(module_map_t&);
    void remove_from_top_level_blocks(bb_t*);
    void populate_guard_blocks(bb_t*, bb_set_t&);

    bb_t* immediate_dominator(bb_t*);
    bb_t* immediate_postdominator(bb_t*);

    id_list_t& ports();
    identifier_t name();
    instr_set_t& def_instrs(identifier_t);
    instr_set_t& use_instrs(identifier_t);

    bool exists(bb_t*);
    bool is_port(identifier_t);
    bool postdominates(bb_t* source, bb_t* sink);
};

#endif  // STRUCTS_H_
