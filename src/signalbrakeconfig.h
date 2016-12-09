#ifndef SIGNALBRAKECONFIG_H
#define SIGNALBRAKECONFIG_H

#include <network.h>
#include <identity.h>

#define DB_VER 3
#define EVENT_VER 2
#define EVENT_PORT 6767
#define BRAKE_PIN 5
#define TOGGLE_LEFT_PIN 4
// TODO - dont use GPIO 0
#define TOGGLE_RIGHT_PIN 0
#define BUTTON_PIN 14
#define STATUS_PIN 2
// TODO - get BIN_VERSION from VERSION file
#define BIN_VERSION 1

// 192.168.4.1 17082560
// 192.168.43.20 338405568
#define SERVER_IP_UINT32 17082560

#define HOSTNAME "phoxsigbrake"
#define OTA_HOSTNAME "phoxsigbrakeota"

#define PUBLIC_SSID "phoxlight"
#define PUBLIC_PASS "phoxlight"

// TODO - obtain these from controller
#define PRIVATE_SSID "phoxlightpriv"
#define PRIVATE_PASS "phoxlightpriv"

typedef struct SignalBrakeConfig {
    char ssid[SSID_MAX];
    char pass[PASS_MAX];
    char hostname[HOSTNAME_MAX];
    NetworkMode networkMode;
} SignalBrakeConfig;

typedef struct PrivateNetworkCreds {
    char ssid[SSID_MAX];
    char pass[PASS_MAX];
} PrivateNetworkCreds;

SignalBrakeConfig * getConfig();
Identity * getIdentity();
PrivateNetworkCreds getPrivateCreds();

int loadConfig();
int writeConfig(SignalBrakeConfig * c);
void logConfig(SignalBrakeConfig * c);
int writeDefaultConfig();

//int registerComponent(Identity * component);
// TODO - removeComponent

#endif
