#ifndef DEVICES_CMD_H
#define DEVICES_CMD_H

#include "CLICommand.h"

class DevicesCmd : public CLICommand {
public:
    std::string name() const override { return "devices"; }
    std::string help() const override { 
        return "Manage I/O devices\n"
               "  devices list           - List registered devices\n"
               "  devices enable <name>  - Enable a device\n"
               "  devices disable <name> - Disable a device";
    }
    bool execute(const std::vector<std::string>& args,
                 CPU *cpu, memory_t *mem, 
                 opcode_handler_t **p_handlers, int *p_num_handlers,
                 cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                 breakpoint_list_t *breakpoints,
                 symbol_table_t *symbols) override;
};

#endif
