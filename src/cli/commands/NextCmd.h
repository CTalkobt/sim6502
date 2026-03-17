#ifndef NEXT_CMD_H
#define NEXT_CMD_H

#include "CLICommand.h"

class NextCmd : public CLICommand {
public:
    virtual std::string name() const override { return "next"; }
    virtual std::string help() const override { 
        return "Usage: next\n"
               "Step over the current instruction. If it's a JSR, it will run until the return.";
    }
    virtual bool execute(const std::vector<std::string>& args,
                         CPU *cpu, memory_t *mem,
                         cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                         breakpoint_list_t *breakpoints,
                         symbol_table_t *symbols) override;
};

#endif
