#ifndef SIGNALBRAKECONFIG_H
#define SIGNALBRAKECONFIG_H

#include <network.h>
#include <identity.h>

#define OTA_SSID "phoxsignalbrake"
#define OTA_PASS "phoxsignalbrake"
#define OTA_HOSTNAME "phoxsignalbrake"

#define DB_VER 3
#define EVENT_VER 2
#define EVENT_PORT 6767
#define BUTTON_PIN 14
#define STATUS_PIN 2
// TODO - get BIN_VERSION from VERSION file
#define BIN_VERSION 1

#define PUBLIC_SSID "phoxsignalbrake"
#define PUBLIC_PASS "phoxsignalbrake"

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
PrivateNetworkCreds * getPrivateCreds();

int loadConfig();
int writeConfig(SignalBrakeConfig * c);
void logConfig(SignalBrakeConfig * c);
int writeDefaultConfig();

//int registerComponent(Identity * component);
// TODO - removeComponent

#endif
