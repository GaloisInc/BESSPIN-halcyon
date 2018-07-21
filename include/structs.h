
#ifndef STRUCTS_H_
#define STRUCTS_H_

#include <list>
#include <map>
#include <set>
#include <stdint.h>

#include <VeriStatement.h>

using namespace Verific;

class bb_t;

typedef VeriExpression* expr_t;
typedef VeriStatement* instr_t;
typedef VeriContinuousAssign* assign_t;

typedef std::set<bb_t*> bb_set_t;
typedef std::list<bb_t*> bb_list_t;
typedef std::list<instr_t> instr_list_t;
typedef std::list<assign_t> assign_list_t;
typedef std::map<std::string, uint32_t> bb_id_map_t;

class bb_t {
  private:
    std::string name;
    expr_t condition;
    bb_set_t predecessors;
    instr_list_t instr_list;

    bb_t* successor_left;
    bb_t* successor_right;

    bool add_predecessor(bb_t*&);

  public:
    explicit bb_t(const std::string&);
    ~bb_t();

    // disable copy constructor.
    bb_t(const bb_t&);

    instr_t last_instr();
    bool exists(const instr_t&);

    bool append(const expr_t&);
    bool append(const instr_t&);

    bool set_left_successor(bb_t*&);
    bool set_right_successor(bb_t*&);

    bb_t* left_successor();
    bb_t* right_successor();

    void dump(uint32_t);
};

class module_t {
  private:
    std::string name;

    bb_id_map_t bb_id_map;
    bb_list_t basicblocks;
    assign_list_t assignments;

  public:
    explicit module_t(const std::string&);
    ~module_t();

    // disable copy constructor.
    module_t(const module_t&);

    void dump();
    bool append_assignment(assign_t&);
    bb_t* create_empty_basicblock(const char*);
};

#endif  // STRUCTS_H_
