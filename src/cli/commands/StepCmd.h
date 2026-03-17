#ifndef STEP_CMD_H
#define STEP_CMD_H

#include "CLICommand.h"

class StepCmd : public CLICommand {
public:
    virtual std::string name() const override { return "step"; }
    virtual std::string help() const override { 
        return "Usage: step [n]\n"
               "Execute 'n' CPU instructions. If 'n' is omitted, executes 1 instruction.\n"
               "Aliases: (empty line)";
    }
    virtual bool execute(const std::vector<std::string>& args,
                         CPU *cpu, memory_t *mem,
                         cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                         breakpoint_list_t *breakpoints,
                         symbol_table_t *symbols) override;
};

#endif
