#ifndef HELP_CMD_H
#define HELP_CMD_H

#include "CLICommand.h"

class HelpCmd : public CLICommand {
public:
    virtual std::string name() const override { return "help"; }
    virtual std::string help() const override { return "Usage: help [command]\nDisplay general help or detailed help for a specific command."; }
    virtual bool execute(const std::vector<std::string>& args,
                         CPU *cpu, memory_t *mem,
                         cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                         breakpoint_list_t *breakpoints,
                         symbol_table_t *symbols) override;
};

#endif
