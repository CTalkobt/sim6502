#include "EnvCmd.h"
#include "project_manager.h"
#include <iostream>
#include <iomanip>

extern bool g_json_mode;

bool EnvCmd::execute(const std::vector<std::string>& args,
                    CPU *cpu, memory_t *mem, 
                    opcode_handler_t **p_handlers, int *p_num_handlers,
                    cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                    breakpoint_list_t *breakpoints,
                    symbol_table_t *symbols) {
    (void)cpu; (void)mem; (void)symbols; (void)dt; (void)p_handlers; (void)p_num_handlers; (void)p_cpu_type; (void)breakpoints;

    if (args.size() < 2 || args[1] == "help") {
        if (g_json_mode) {
            std::cout << "{\"cmd\":\"env\",\"ok\":false,\"error\":\"Usage: env list | env create <id> <name> [dir] [VAR=VAL...]\"}" << std::endl;
        } else {
            std::cout << "Usage: env list" << std::endl;
            std::cout << "       env create <template_id> <project_name> [target_dir] [VAR=VAL...]" << std::endl;
        }
        return true;
    }

    if (args[1] == "list") {
        std::vector<ProjectTemplate> templates = ProjectManager::list_templates();
        if (g_json_mode) {
            std::cout << "{\"cmd\":\"env\",\"ok\":true,\"data\":{\"templates\":[";
            for (size_t i = 0; i < templates.size(); i++) {
                if (i > 0) std::cout << ",";
                std::cout << "{\"id\":\"" << templates[i].id << "\","
                          << "\"name\":\"" << templates[i].name << "\","
                          << "\"description\":\"" << templates[i].description << "\"}";
            }
            std::cout << "]}}" << std::endl;
        } else {
            std::cout << "Available Templates:" << std::endl;
            std::cout << std::left << std::setw(20) << "ID" << "NAME" << std::endl;
            std::cout << std::string(50, '-') << std::endl;
            if (templates.empty()) {
                std::cout << "(none found in templates/)" << std::endl;
            } else {
                for (const auto& t : templates) {
                    std::cout << std::left << std::setw(20) << t.id << t.name << std::endl;
                    std::cout << "  " << t.description << std::endl;
                }
            }
        }
    } 
    else if (args[1] == "create") {
        if (args.size() < 4) {
            if (g_json_mode) std::cout << "{\"cmd\":\"env\",\"ok\":false,\"error\":\"Missing arguments\"}" << std::endl;
            else std::cout << "Usage: env create <id> <name> [dir]" << std::endl;
            return true;
        }

        std::string tid = args[2];
        std::string name = args[3];
        std::string dir = (args.size() > 4) ? args[4] : name;

        std::map<std::string, std::string> overrides;
        overrides["PROJECT_NAME"] = name;

        /* Parse additional KEY=VALUE pairs from subsequent args */
        for (size_t i = 5; i < args.size(); i++) {
            size_t eq = args[i].find('=');
            if (eq != std::string::npos) {
                std::string k = args[i].substr(0, eq);
                std::string v = args[i].substr(eq + 1);
                overrides[k] = v;
            }
        }

        std::string err;
        if (ProjectManager::create_project(tid, dir, overrides, err)) {
            if (g_json_mode) {
                std::cout << "{\"cmd\":\"env\",\"ok\":true,\"data\":{\"path\":\"" << dir << "\"}}" << std::endl;
            } else {
                std::cout << "Project '" << name << "' created successfully in: " << dir << std::endl;
            }
        } else {
            if (g_json_mode) {
                std::cout << "{\"cmd\":\"env\",\"ok\":false,\"error\":\"" << err << "\"}" << std::endl;
            } else {
                std::cerr << "Error: " << err << std::endl;
            }
        }
    }
    return true;
}
