#include "../commands.h"
#include "BreakCmd.h"
#include "condition.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

extern int g_json_mode;
extern void json_err(const char *cmd, const char *msg);

bool BreakCmd::execute(const std::vector<std::string>& args,
                       CPU *cpu, memory_t *mem,
                       cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                       breakpoint_list_t *breakpoints,
                       symbol_table_t *symbols) {
    (void)cpu; (void)mem; (void)p_cpu_type; (void)dt; (void)symbols;
    
    if (args.size() < 2) {
        if (g_json_mode) json_err("break", "Usage: break <addr> [condition]");
        else cli_printf("Usage: break <addr> [condition]\n");
        return false;
    }

    const char *p = args[1].c_str();
    unsigned long addr;
    if (parse_mon_value(&p, &addr)) {
        std::string condition;
        for (size_t i = 2; i < args.size(); ++i) {
            if (i > 2) condition += " ";
            condition += args[i];
        }
        breakpoint_add(breakpoints, (unsigned short)addr, condition.empty() ? NULL : condition.c_str());
        if (g_json_mode) cli_printf("{\"cmd\":\"break\",\"ok\":true,\"data\":{\"address\":%lu}}\n", addr & 0xFFFF);
        return true;
    } else {
        if (g_json_mode) json_err("break", "Usage: break <addr> [condition]");
        else cli_printf("Usage: break <addr> [condition]\n");
        return false;
    }
}
