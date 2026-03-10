#include "CommandRegistry.h"
#include "StepCmd.h"
#include "BreakCmd.h"
#include "EnvCmd.h"

CommandRegistry::CommandRegistry() {
    registerCommand(std::make_unique<StepCmd>());
    registerCommand(std::make_unique<BreakCmd>());
    registerCommand(std::make_unique<EnvCmd>());
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
