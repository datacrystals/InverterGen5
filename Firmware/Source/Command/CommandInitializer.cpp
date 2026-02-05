#include "CommandInitializer.h"
#include "CommandManager.h"

void initializeCommands() {
    CommandManager& mgr = CommandManager::instance();
    
    mgr.registerCommand(getFreqCommand());
    mgr.registerCommand(getRampCommand());
    mgr.registerCommand(getCarrierCommand());
    mgr.registerCommand(getAutoCommand());
    mgr.registerCommand(getAsyncCommand());
    mgr.registerCommand(getFlashCommand());
    mgr.registerCommand(getSoftStopCommand());
    mgr.registerCommand(getEmergencyStopCommand());
    mgr.registerCommand(getEnableCommand());
    mgr.registerCommand(getImmediateCommand());
    mgr.registerCommand(getHelpCommand());
}