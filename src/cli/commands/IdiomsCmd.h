#ifndef IDIOMS_CMD_H
#define IDIOMS_CMD_H

#include "CLICommand.h"

class IdiomsCmd : public CLICommand {
public:
    std::string name() const override { return "idioms"; }
    std::string help() const override { 
        return "Usage: idioms list\n"
               "       idioms get <name>\n"
               "List or retrieve common 6502 assembly code snippets/idioms.";
    }
    bool execute(const std::vector<std::string>& args,
                 CPU *cpu, memory_t *mem,
                 cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                 breakpoint_list_t *breakpoints,
                 symbol_table_t *symbols) override;
};

#endif
