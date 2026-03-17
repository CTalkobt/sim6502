#ifndef HISTORY_CMD_H
#define HISTORY_CMD_H

#include "CLICommand.h"

class HistoryCmd : public CLICommand {
public:
    virtual std::string name() const override { return "history"; }
    virtual std::string help() const override { 
        return "Usage: sb / stepback  - Step backward in history\n"
               "       sf / stepfwd   - Step forward in history (after stepping back)";
    }
    virtual bool execute(const std::vector<std::string>& args,
                         CPU *cpu, memory_t *mem,
                         cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                         breakpoint_list_t *breakpoints,
                         symbol_table_t *symbols) override;
};

#endif
