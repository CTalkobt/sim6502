#ifndef ENV_CMD_H
#define ENV_CMD_H

#include "CLICommand.h"

class EnvCmd : public CLICommand {
public:
    std::string name() const override { return "env"; }
    std::string help() const override { return "Manage project environments/templates"; }
    bool execute(const std::vector<std::string>& args,
                 CPU *cpu, memory_t *mem, 
                 opcode_handler_t **p_handlers, int *p_num_handlers,
                 cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                 breakpoint_list_t *breakpoints,
                 symbol_table_t *symbols) override;
};

#endif
