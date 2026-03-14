#ifndef NEXT_CMD_H
#define NEXT_CMD_H

#include "CLICommand.h"

class NextCmd : public CLICommand {
public:
    virtual std::string name() const override { return "next"; }
    virtual std::string help() const override { return "next - Step over subroutine calls"; }
    virtual bool execute(const std::vector<std::string>& args,
                         CPU *cpu, memory_t *mem,
                         cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                         breakpoint_list_t *breakpoints,
                         symbol_table_t *symbols) override;
};

#endif
