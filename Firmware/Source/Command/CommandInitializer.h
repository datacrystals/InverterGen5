#ifndef COMMAND_INITIALIZER_H
#define COMMAND_INITIALIZER_H

class CommandInterface;

// Factory functions - each .cpp file provides one
CommandInterface* getFreqCommand();
CommandInterface* getRampCommand();
CommandInterface* getCarrierCommand();
CommandInterface* getAutoCommand();
CommandInterface* getSoftStopCommand();
CommandInterface* getEmergencyStopCommand();
CommandInterface* getEnableCommand();
CommandInterface* getImmediateCommand();
CommandInterface* getHelpCommand();

// Centralized registration
void initializeCommands();

#endif