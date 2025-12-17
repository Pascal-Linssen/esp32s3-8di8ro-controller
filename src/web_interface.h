#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>

// Function to handle web server requests
void handleWebServer();

// Get HTML page as response string
String getHtmlPage();

// Get system status as JSON string
String getStatusJSON();

// Parse relay control commands
void parseRelayCommand(const char* cmd);

#endif
