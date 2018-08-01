#include <algorithm>
#include <cassert>
#include <iostream>

#include <readline/readline.h>
#include <readline/history.h>

#include <Array.h>
#include <Map.h>
#include <veri_file.h>
#include <VeriId.h>
#include <VeriExpression.h>
#include <VeriMisc.h>
#include <VeriModule.h>
#include <VeriStatement.h>

#include "structs.h"
#include "dependence.h"

using namespace Verific;
module_map_t module_map;

void clear_status() {
    std::cerr << "\r                                                        ";
    std::cerr << "\r";
}

void update_status(const char* string) {
    std::cerr << "\r                                                        ";
    std::cerr << "\r" << string;
}

void destroy_module_map() {
    for (auto it = module_map.begin(); it != module_map.end(); it++) {
        module_t* module_ds = it->second;

        delete module_ds;
        module_ds = nullptr;
    }

    module_map.clear();
}

unsigned parse_modules() {
    MapIter map_iter;
    VeriModule* module = nullptr;

    char status[256];
    unsigned counter = 0;
    unsigned module_count = veri_file::AllModules()->Size();

    update_status("parsing module(s) ... ");

    FOREACH_VERILOG_MODULE(map_iter, module) {
        counter += 1;
        snprintf(status, sizeof(status), "parsing module [ %3d of %3d ] ... ",
                counter, module_count);

        update_status(status);

        module_t* module_ds = new module_t(module);
        module_map.emplace(module_ds->name(), module_ds);
    }

    clear_status();
    return module_map.size();
}

bool analyze_file(const char* filename) {
    if (veri_file::Analyze(filename, veri_file::SYSTEM_VERILOG) == false) {
        assert(false && "failed to analyze file!");
        return false;
    }

    return true;
}

const char* suffix = nullptr;

char* name_gen(const char *__text, int state) {
    static id_list_t matches;
    static size_t match_index = 0;

    if (state == 0) {
        matches.clear();
        match_index = 0;

        std::string text = std::string(__text);

        if (suffix == nullptr) {
            std::transform(text.begin(), text.end(), text.begin(), ::tolower);

            for (module_map_t::iterator it = module_map.begin();
                    it != module_map.end(); it++) {
                identifier_t module_name = it->first;

                identifier_t lcase_name = module_name;
                std::transform(lcase_name.begin(), lcase_name.end(),
                        lcase_name.begin(), ::tolower);

                if (lcase_name.size() >= text.size() &&
                        lcase_name.compare(0, text.size(), text) == 0) {
                    matches.push_back(module_name);
                }
            }
        } else {
            std::string module_name = text.substr(0, suffix - __text - 1);

            std::string field = std::string(suffix);
            std::transform(field.begin(), field.end(), field.begin(),
                    ::tolower);

            module_map_t::iterator it = module_map.find(module_name);
            if (it == module_map.end()) {
                return nullptr;
            }

            module_t* module_ds = it->second;

            for (identifier_t port : module_ds->ports()) {
                identifier_t lcase_port = port;
                std::transform(lcase_port.begin(), lcase_port.end(),
                        lcase_port.begin(), ::tolower);

                if (lcase_port.size() >= field.size() &&
                        lcase_port.compare(0, field.size(), field) == 0) {
                    matches.push_back(module_name + "." + port);
                }
            }
        }
    }

    if (match_index >= matches.size()) {
        return nullptr;
    }

    return strdup(matches[match_index++].c_str());
}

char** complete_text(const char *text, int start, int end) {
    rl_attempted_completion_over = 1;
    const char* separator = strchr(text, '.');

    if (separator == nullptr) {
        suffix = nullptr;
        rl_completion_append_character = '.';
    } else {
        rl_completion_suppress_append = 1;
        suffix = separator + 1;
    }

    return rl_completion_matches(text, name_gen);
}

void process_text(const char* __buffer) {
    const char* separator = strchr(__buffer, '.');

    if (separator == nullptr) {
        std::cerr << "need <module>.<port>, found " << __buffer << "\n";
        return;
    }

    dep_analysis_t dep_analysis;

    std::string buffer(__buffer);
    std::string mod_name = buffer.substr(0, separator - __buffer);
    std::string field = std::string(separator + 1);

    if (dep_analysis.compute_dependencies(mod_name, field, module_map)) {
        id_set_t& timing_deps = dep_analysis.leaking_timing_deps();

        if (timing_deps.size() > 0) {
            clear_status();
            std::cerr << "timing leak:";

            for (identifier_t id : timing_deps) {
                std::cerr << " " << id;
            }

            std::cerr << "\n";
        }

        id_set_t& non_timing_deps = dep_analysis.leaking_non_timing_deps();

        if (non_timing_deps.size() > 0) {
            clear_status();
            std::cerr << "non-timing leak:";

            for (identifier_t id : non_timing_deps) {
                std::cerr << " " << id;
            }

            std::cerr << "\n";
        }
    } else {
        update_status("did not find any leakage.\n");
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "USAGE: " << argv[0] << " verilog-files";
        return 1;
    }

    Message::SetMessageType("VERI-1482", VERIFIC_IGNORE);

    for (unsigned idx = 1; idx < argc; idx += 1) {
        analyze_file(argv[idx]);
        parse_modules();
    }

    update_status("building def-use chains ... ");

    for (auto it = module_map.begin(); it != module_map.end(); it++) {
        module_t* module_ds = it->second;

        module_ds->resolve_links(module_map);
        module_ds->build_def_use_chains();
        // module_ds->print_undef_ids();
    }

    clear_status();
    rl_attempted_completion_function = complete_text;

    char* buffer = nullptr;
    while ((buffer = readline(">> ")) != nullptr) {
        if (strlen(buffer) == 0) {
            continue;
        } else if (strcmp(buffer, "quit") != 0) {
            add_history(buffer);
            process_text(buffer);

            free(buffer);
        } else {
            free(buffer);
            break;
        }
    }

    destroy_module_map();
    return 0 ;
}
