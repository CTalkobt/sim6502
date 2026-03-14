#include "CommandRegistry.h"
#include "StepCmd.h"
#include "NextCmd.h"
#include "FinishCmd.h"
#include "HistoryCmd.h"
#include "BreakCmd.h"
#include "EnvCmd.h"
#include "DevicesCmd.h"
#include "IdiomsCmd.h"

CommandRegistry::CommandRegistry() {
    registerCommand(std::make_unique<StepCmd>());
    registerCommand(std::make_unique<NextCmd>());
    registerCommand(std::make_unique<FinishCmd>());
    
    auto hist = std::make_unique<HistoryCmd>();
    commands["sb"] = std::move(hist);
    registerCommand(std::make_unique<HistoryCmd>()); // registers as "history"
    
    auto sf = std::make_unique<HistoryCmd>();
    commands["sf"] = std::move(sf);

    registerCommand(std::make_unique<BreakCmd>());
    registerCommand(std::make_unique<EnvCmd>());
    registerCommand(std::make_unique<DevicesCmd>());
    registerCommand(std::make_unique<IdiomsCmd>());

    /* Legacy aliases */
    commands["list_patterns"] = std::make_unique<IdiomsCmd>();
    commands["get_pattern"] = std::make_unique<IdiomsCmd>();
}

void CommandRegistry::registerCommand(std::unique_ptr<CLICommand> cmd) {
    commands[cmd->name()] = std::move(cmd);
}

CLICommand* CommandRegistry::getCommand(const std::string& name) {
    auto it = commands.find(name);
    if (it != commands.end()) {
        return it->second.get();
    }
    return nullptr;
}

const std::map<std::string, std::unique_ptr<CLICommand>>& CommandRegistry::getAllCommands() const {
    return commands;
}
