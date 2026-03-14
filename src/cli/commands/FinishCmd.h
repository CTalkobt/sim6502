#ifndef FINISH_CMD_H
#define FINISH_CMD_H

#include "CLICommand.h"

class FinishCmd : public CLICommand {
public:
    virtual std::string name() const override { return "finish"; }
    virtual std::string help() const override { return "finish - Run until current subroutine returns"; }
    virtual bool execute(const std::vector<std::string>& args,
                         CPU *cpu, memory_t *mem,
                         cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                         breakpoint_list_t *breakpoints,
                         symbol_table_t *symbols) override;
};

#endif
