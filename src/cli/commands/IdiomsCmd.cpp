#include "IdiomsCmd.h"
#include "patterns.h"
#include <iostream>
#include <iomanip>
#include <cstring>

extern int g_json_mode;

static void print_body_json(const snippet_t *sn, const std::string& cmd_name) {
    std::cout << "{\"cmd\":\"" << cmd_name << "\",\"ok\":true,\"data\":{\"name\":\""
              << sn->name << "\",\"body\":\"";
    for (const char *c = sn->body; *c; c++) {
        if (*c == '"')       std::cout << "\\\"";
        else if (*c == '\\') std::cout << "\\\\";
        else if (*c == '\n') std::cout << "\\n";
        else                 std::cout << *c;
    }
    std::cout << "\"}}" << std::endl;
}

bool IdiomsCmd::execute(const std::vector<std::string>& args,
                       CPU *cpu, memory_t *mem,
                       cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                       breakpoint_list_t *breakpoints,
                       symbol_table_t *symbols) {
    (void)cpu; (void)mem; (void)symbols; (void)dt; (void)p_cpu_type; (void)breakpoints;

    const std::string& cmd = args[0];

    /* ── get_pattern <name>  (alias used by test_patterns.py and MCP) ── */
    if (cmd == "get_pattern") {
        if (args.size() < 2) {
            if (g_json_mode) std::cout << "{\"cmd\":\"get_pattern\",\"ok\":false,\"error\":\"Missing pattern name\"}" << std::endl;
            else std::cout << "Usage: get_pattern <name>" << std::endl;
            return true;
        }
        const snippet_t *sn = snippet_find(args[1].c_str());
        if (!sn) {
            if (g_json_mode) std::cout << "{\"cmd\":\"get_pattern\",\"ok\":false,\"error\":\"Pattern not found\"}" << std::endl;
            else std::cout << "Pattern not found: " << args[1] << std::endl;
        } else {
            if (g_json_mode) print_body_json(sn, "get_pattern");
            else std::cout << sn->body << std::endl;
        }
        return true;
    }

    /* ── list_patterns  (alias) ── */
    if (cmd == "list_patterns") {
        if (g_json_mode) {
            std::cout << "{\"cmd\":\"list_patterns\",\"ok\":true,\"data\":{\"patterns\":[";
            for (int i = 0; i < g_snippet_count; i++) {
                if (i > 0) std::cout << ",";
                std::cout << "{\"name\":\"" << g_snippets[i].name << "\","
                          << "\"category\":\"" << g_snippets[i].category << "\","
                          << "\"processor\":\"" << g_snippets[i].processor << "\","
                          << "\"summary\":\"" << g_snippets[i].summary << "\"}";
            }
            std::cout << "]}}" << std::endl;
        } else {
            std::cout << "Available Patterns (Assembly Snippets):" << std::endl;
            std::cout << std::left << std::setw(20) << "NAME" << std::setw(10) << "CPU" << "SUMMARY" << std::endl;
            std::cout << std::string(60, '-') << std::endl;
            for (int i = 0; i < g_snippet_count; i++) {
                std::cout << std::left << std::setw(20) << g_snippets[i].name
                          << std::setw(10) << g_snippets[i].processor
                          << g_snippets[i].summary << std::endl;
            }
        }
        return true;
    }

    /* ── idioms list | idioms get <name> ── */
    if (args.size() < 2 || args[1] == "help") {
        if (g_json_mode) {
            std::cout << "{\"cmd\":\"idioms\",\"ok\":false,\"error\":\"Usage: idioms list | idioms get <name>\"}" << std::endl;
        } else {
            std::cout << "Usage: idioms list" << std::endl;
            std::cout << "       idioms get <name>" << std::endl;
        }
        return true;
    }

    if (args[1] == "list") {
        if (g_json_mode) {
            std::cout << "{\"cmd\":\"idioms\",\"ok\":true,\"data\":{\"idioms\":[";
            for (int i = 0; i < g_snippet_count; i++) {
                if (i > 0) std::cout << ",";
                std::cout << "{\"name\":\"" << g_snippets[i].name << "\","
                          << "\"category\":\"" << g_snippets[i].category << "\","
                          << "\"processor\":\"" << g_snippets[i].processor << "\","
                          << "\"summary\":\"" << g_snippets[i].summary << "\"}";
            }
            std::cout << "]}}" << std::endl;
        } else {
            std::cout << "Available Idioms (Assembly Snippets):" << std::endl;
            std::cout << std::left << std::setw(20) << "NAME" << std::setw(10) << "CPU" << "SUMMARY" << std::endl;
            std::cout << std::string(60, '-') << std::endl;
            for (int i = 0; i < g_snippet_count; i++) {
                std::cout << std::left << std::setw(20) << g_snippets[i].name
                          << std::setw(10) << g_snippets[i].processor
                          << g_snippets[i].summary << std::endl;
            }
        }
    }
    else if (args[1] == "get") {
        if (args.size() < 3) {
            if (g_json_mode) std::cout << "{\"cmd\":\"idioms\",\"ok\":false,\"error\":\"Missing idiom name\"}" << std::endl;
            else std::cout << "Usage: idioms get <name>" << std::endl;
            return true;
        }
        const snippet_t *sn = snippet_find(args[2].c_str());
        if (!sn) {
            if (g_json_mode) std::cout << "{\"cmd\":\"idioms\",\"ok\":false,\"error\":\"Idiom not found\"}" << std::endl;
            else std::cout << "Idiom not found: " << args[2] << std::endl;
        } else {
            if (g_json_mode) print_body_json(sn, "idioms");
            else std::cout << sn->body << std::endl;
        }
    }
    return true;
}
