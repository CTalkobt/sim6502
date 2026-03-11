#include "DevicesCmd.h"
#include "io_registry.h"
#include "../commands.h"
#include <stdio.h>
#include <string.h>

bool DevicesCmd::execute(const std::vector<std::string>& args,
                        CPU *cpu, memory_t *mem, 
                        opcode_handler_t **p_handlers, int *p_num_handlers,
                        cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                        breakpoint_list_t *breakpoints,
                        symbol_table_t *symbols) {
    (void)cpu; (void)p_handlers; (void)p_num_handlers; (void)p_cpu_type; (void)dt; (void)breakpoints; (void)symbols;

    if (!mem->io_registry) {
        if (g_json_mode) json_err("devices", "I/O Registry not initialized");
        else printf("Error: I/O Registry not initialized\n");
        return false;
    }

    std::string sub = (args.size() > 1) ? args[1] : "list";

    if (sub == "list") {
        const auto& regs = mem->io_registry->get_registrations();
        if (g_json_mode) {
            printf("{\"cmd\":\"devices\",\"ok\":true,\"data\":{\"devices\":[");
            for (size_t i = 0; i < regs.size(); i++) {
                if (i > 0) printf(",");
                printf("{\"name\":\"%s\",\"start\":%d,\"end\":%d,\"priority\":%d,\"enabled\":%s}",
                       regs[i].handler->get_handler_name(),
                       regs[i].start, regs[i].end, regs[i].priority,
                       regs[i].enabled ? "true" : "false");
            }
            printf("]}}\n");
        } else {
            printf("%-30s  %-12s  %-8s  %s\n", "Device Name", "Range", "Priority", "Status");
            printf("----------------------------------------------------------------------\n");
            for (const auto& reg : regs) {
                char range[16];
                snprintf(range, sizeof(range), "$%04X-$%04X", reg.start, reg.end);
                printf("%-30s  %-12s  %-8d  %s\n",
                       reg.handler->get_handler_name(), range, reg.priority,
                       reg.enabled ? "ENABLED" : "DISABLED");
            }
        }
    } else if (sub == "enable" || sub == "disable") {
        if (args.size() < 3) {
            if (g_json_mode) json_err("devices", "Usage: devices <enable|disable> <name>");
            else printf("Usage: devices <enable|disable> <name>\n");
            return false;
        }

        bool enable = (sub == "enable");
        std::string target = args[2];
        for (size_t i = 3; i < args.size(); i++) target += " " + args[i];

        bool found = false;
        const auto& regs = mem->io_registry->get_registrations();
        for (const auto& reg : regs) {
            if (strcasecmp(reg.handler->get_handler_name(), target.c_str()) == 0) {
                mem->io_registry->set_enabled(reg.handler, enable);
                found = true;
            }
        }

        if (found) {
            mem->io_registry->rebuild_map(mem);
            if (g_json_mode) {
                printf("{\"cmd\":\"devices\",\"ok\":true,\"data\":{\"name\":\"%s\",\"enabled\":%s}}\n",
                       target.c_str(), enable ? "true" : "false");
            } else {
                printf("Device '%s' %s.\n", target.c_str(), enable ? "enabled" : "disabled");
            }
        } else {
            if (g_json_mode) json_err("devices", "Device not found");
            else printf("Error: Device '%s' not found.\n", target.c_str());
        }
    } else {
        if (g_json_mode) json_err("devices", "Unknown subcommand");
        else printf("Unknown subcommand: %s\n", sub.c_str());
        return false;
    }

    return true;
}
