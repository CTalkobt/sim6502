#ifndef ENV_CMD_H
#define ENV_CMD_H

#include "CLICommand.h"

class EnvCmd : public CLICommand {
public:
    std::string name() const override { return "env"; }
    std::string help() const override { 
        return "Usage: env list\n"
               "       env create <template> <name> [dir] [VAR=VAL...]\n"
               "Manage project environments and templates.";
    }
    bool execute(const std::vector<std::string>& args,
                 CPU *cpu, memory_t *mem,
                 cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                 breakpoint_list_t *breakpoints,
                 symbol_table_t *symbols) override;
};

#endif
