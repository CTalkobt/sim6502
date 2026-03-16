#include "DevicesCmd.h"
#include "io_registry.h"
#include "device/vic2_io.h"
#include "device/mega65_io.h"
#include "device/cia_io.h"
#include "../commands.h"
#include <stdio.h>
#include <string.h>
#include <vector>

/* Owner for dynamically added handlers in the CLI front-end */
static std::vector<IOHandler*> g_cli_dynamic_handlers;

bool DevicesCmd::execute(const std::vector<std::string>& args,
                        CPU *cpu, memory_t *mem,
                        cpu_type_t *p_cpu_type, dispatch_table_t *dt,
                        breakpoint_list_t *breakpoints,
                        symbol_table_t *symbols) {
    (void)cpu; (void)p_cpu_type; (void)dt; (void)breakpoints; (void)symbols;

    if (!mem->io_registry) {
        if (g_json_mode) json_err("devices", "I/O Registry not initialized");
        else cli_printf("Error: I/O Registry not initialized\n");
        return false;
    }

    std::string sub = (args.size() > 1) ? args[1] : "list";

    if (sub == "list") {
        const auto& regs = mem->io_registry->get_registrations();
        if (g_json_mode) {
            cli_printf("{\"cmd\":\"devices\",\"ok\":true,\"data\":{\"devices\":[");
            for (size_t i = 0; i < regs.size(); i++) {
                if (i > 0) cli_printf(",");
                cli_printf("{\"name\":\"%s\",\"start\":%d,\"end\":%d,\"priority\":%d,\"enabled\":%s}",
                       regs[i].handler->get_handler_name(),
                       regs[i].start, regs[i].end, regs[i].priority,
                       regs[i].enabled ? "true" : "false");
            }
            cli_printf("]}}\n");
        } else {
            cli_printf("%-30s  %-12s  %-8s  %s\n", "Device Name", "Range", "Priority", "Status");
            cli_printf("----------------------------------------------------------------------\n");
            for (const auto& reg : regs) {
                char range[16];
                snprintf(range, sizeof(range), "$%04X-$%04X", reg.start, reg.end);
                cli_printf("%-30s  %-12s  %-8d  %s\n",
                       reg.handler->get_handler_name(), range, reg.priority,
                       reg.enabled ? "ENABLED" : "DISABLED");
            }
        }
    } else if (sub == "enable" || sub == "disable") {
        if (args.size() < 3) {
            if (g_json_mode) json_err("devices", "Usage: devices <enable|disable> <name>");
            else cli_printf("Usage: devices <enable|disable> <name>\n");
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
                cli_printf("{\"cmd\":\"devices\",\"ok\":true,\"data\":{\"name\":\"%s\",\"enabled\":%s}}\n",
                       target.c_str(), enable ? "true" : "false");
            } else {
                cli_printf("Device '%s' %s.\n", target.c_str(), enable ? "enabled" : "disabled");
            }
        } else {
            if (g_json_mode) json_err("devices", "Device not found");
            else cli_printf("Error: Device '%s' not found.\n", target.c_str());
        }
    } else if (sub == "add") {
        if (args.size() < 4) {
            if (g_json_mode) json_err("devices", "Usage: devices add <type> <address>");
            else cli_printf("Usage: devices add <type> <address>\n");
            return false;
        }

        std::string type = args[2];
        const char *p_addr = args[3].c_str();
        unsigned long addr;
        if (!parse_mon_value(&p_addr, &addr)) {
            if (g_json_mode) json_err("devices", "Invalid address");
            else cli_printf("Error: Invalid address '%s'\n", args[3].c_str());
            return false;
        }

        IOHandler *h = nullptr;
        uint16_t end = (uint16_t)addr;

        if (type == "vic2") {
            h = new VIC2Handler();
            end = (uint16_t)addr + 0x2E;
        } else if (type == "cia1") {
            h = new CIAHandler("CIA1", false);
            end = (uint16_t)addr + 0x0F;
        } else if (type == "cia2") {
            h = new CIAHandler("CIA2", true);
            end = (uint16_t)addr + 0x0F;
        } else if (type == "mega65_math") {
            h = new MathCoprocessorHandler();
            end = (uint16_t)addr + 0x03;
        } else if (type == "mega65_dma") {
            h = new DMAControllerHandler();
            end = (uint16_t)addr + 0x05;
        } else if (type == "sid") {
            // Stubbed until implemented
            if (g_json_mode) json_err("devices", "SID not yet implemented");
            else cli_printf("Error: SID not yet implemented\n");
            return false;
        }

        if (h) {
            g_cli_dynamic_handlers.push_back(h);
            mem->io_registry->register_handler((uint16_t)addr, end, h);
            mem->io_registry->rebuild_map(mem);
            if (g_json_mode) {
                cli_printf("{\"cmd\":\"devices\",\"ok\":true,\"data\":{\"name\":\"%s\",\"start\":%lu,\"end\":%u}}\n",
                       h->get_handler_name(), addr, end);
            } else {
                cli_printf("Device '%s' added at $%04lX-$%04X.\n", h->get_handler_name(), addr, end);
            }
        } else {
            if (g_json_mode) json_err("devices", "Unknown device type");
            else cli_printf("Error: Unknown device type '%s'\n", type.c_str());
            return false;
        }
    } else {
        if (g_json_mode) json_err("devices", "Unknown subcommand");
        else cli_printf("Unknown subcommand: %s\n", sub.c_str());
        return false;
    }

    return true;
}
