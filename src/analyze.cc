#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <sstream>

#include <readline/readline.h>
#include <readline/history.h>

#include <json/json.h>
#include <json/reader.h>

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

void destroy_module_map() {
    for (auto it = module_map.begin(); it != module_map.end(); it++) {
        module_t* module_ds = it->second;

        delete module_ds;
        module_ds = nullptr;
    }

    module_map.clear();
}

uint32_t parse_modules() {
    MapIter map_iter;
    VeriModule* module = nullptr;

    char status[256];
    uint32_t counter = 0;
    uint32_t module_count = veri_file::AllModules()->Size();

    util_t::update_status("parsing module(s) ... ");

    FOREACH_VERILOG_MODULE(map_iter, module) {
        counter += 1;
        snprintf(status, sizeof(status), "parsing module [ %3d of %3d ] ... ",
                counter, module_count);

        util_t::update_status(status);

        module_t* module_ds = new module_t(module);
        module_map.emplace(module_ds->name(), module_ds);
    }

    util_t::clear_status();
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
        util_t::warn("need <module>.<port>, found '" + identifier_t(__buffer) +
                "'\n");
        return;
    }

    dep_analysis_t dep_analysis;

    std::string buffer(__buffer);
    std::string mod_name = buffer.substr(0, separator - __buffer);
    std::string field = std::string(separator + 1);

    if (dep_analysis.compute_dependencies(mod_name, field, module_map)) {
        util_t::update_status("\n");

        id_set_t& timing_deps = dep_analysis.leaking_timing_deps();

        if (timing_deps.size() > 0) {
            util_t::clear_status();

            util_t::underline("found timing leak:");
            util_t::dump_set(timing_deps);
        }

        id_set_t& non_timing_deps = dep_analysis.leaking_non_timing_deps();

        if (non_timing_deps.size() > 0) {
            util_t::clear_status();

            util_t::underline("found non-timing leak:");
            util_t::dump_set(non_timing_deps);
        }
    } else {
        util_t::update_status("did not find any leakage.\n");
    }
}

void do_repl() {
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
}

void do_one_signal(std::string mod, std::string fld, int &outIdx, Json::Value &out) {
    dep_analysis_t dep_analysis;

    bool compute = dep_analysis.compute_dependencies(mod,
                                                     fld,
                                                     module_map);
    if (compute) {
        Json::Value result;
        id_set_t& timing_deps = dep_analysis.leaking_timing_deps();
        id_set_t& non_timing_deps = dep_analysis.leaking_non_timing_deps();

        int idx = 0;
        for (auto id : timing_deps) {
            result["timing"][idx] = id;
            idx++;
        }

        idx = 0;
        for (auto id : non_timing_deps) {
            result["non_timing"][idx] = id;
            idx++;
        }
        result["module"] = mod;
        result["field"]  = fld;
        out[outIdx] = result;
    } else {
        out[outIdx]["module"] = mod;
        out[outIdx]["field"]  = fld;
        out[outIdx]["non_timing"] = Json::Value(Json::arrayValue);
        out[outIdx]["timing"] = Json::Value(Json::arrayValue);
    }
    outIdx++;
}


Json::Value processJSON(Json::Value root) {
    int outIdx = 0;
    Json::Value out(Json::arrayValue);

    for (auto s : root["signals"]) {
        std::string mod = s["module"].asString().c_str();
        std::string fld = s["field"].asString().c_str();
        std::vector<std::string> fields;
        if (fld.back() == '*') {
            fld.pop_back();

            module_map_t::iterator it = module_map.find(mod);
            module_t* module_ds = it->second;

            for (identifier_t port : module_ds->ports()) {
                identifier_t lcase_port = port;
                std::transform(lcase_port.begin(), lcase_port.end(),
                               lcase_port.begin(), ::tolower);

                if (lcase_port.size() >= fld.size() &&
                    lcase_port.compare(0, fld.size(), fld) == 0) {
                    fields.push_back(port);
                }
            }
        } else {
            fields.push_back(fld);
        }

        for (auto fld : fields) {
            do_one_signal(mod, fld, outIdx, out);
        }
    }

    return out;
}

int main(int argc, char **argv) {
    bool interactive = true;
    std::vector<std::string> sourceFiles;
    Json::Value root;

    if (argc < 2) {
        std::cout << root;
        std::cerr << "USAGE: " << argv[0] << " verilog-files";
        std::cerr << "       " << argv[0] << " <JSON spec>";
        return 1;
    }

    if (argc == 2) {
        // Try to parse a JSON spec
        std::ifstream file;
        file.open(argv[1]);
        Json::CharReaderBuilder builder;
        builder["collectComments"] = false;
        std::string errs;
        bool ok = Json::parseFromStream(builder, file, &root, &errs);
        if (ok) {
            interactive = false;
            // TODO: Check JSON schema (is that a thing?)
            for (int i = 0; i < root["sources"].size(); i++) {
                sourceFiles.push_back(root["sources"][i].asString());
            }
        }
    }

    if (interactive) {
        for (int i = 1; i < argc; i++) {
            sourceFiles.push_back(std::string(argv[i]));
        }
    }

    Message::SetMessageType("VERI-1482", VERIFIC_IGNORE);

    util_t::update_status("analyzing input files ... ");

    for (auto f : sourceFiles) {
        analyze_file(f.c_str());
    }

    parse_modules();

    util_t::update_status("building def-use chains ... ");

    for (auto it = module_map.begin(); it != module_map.end(); it++) {
        module_t* module_ds = it->second;
        module_ds->resolve_links(module_map);
    }

    for (auto it = module_map.begin(); it != module_map.end(); it++) {
        module_t* module_ds = it->second;

        module_ds->build_def_use_chains();
        // module_ds->print_undef_ids();
    }

    util_t::clear_status();
    rl_attempted_completion_function = complete_text;

    if (interactive) {
        do_repl();
    } else {
        Json::Value out = processJSON(root);
        std::cout << out << std::endl;
    }

    destroy_module_map();
    return 0 ;
}
