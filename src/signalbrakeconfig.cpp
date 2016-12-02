#include <Esp.h>
#include <objstore.h>
#include <identity.h>
#include "signalbrakeconfig.h"

#define DEFAULT_CONFIG_ID 1

// default config values
SignalBrakeConfig defaultConfig = {
    PRIVATE_SSID,
    PRIVATE_PASS,
    HOSTNAME,
    CONNECT,
};

static Identity id = {
    .model = 1000,
    .serial = ESP.getChipId(),
    .bin = BIN_VERSION,
    .eventVer = EVENT_VER,
    .dbVer = DB_VER
};

// only one config for now
static SignalBrakeConfig config;

SignalBrakeConfig * getConfig(){
    return &config;
}

Identity * getIdentity(){
    return &id;
}

PrivateNetworkCreds getPrivateCreds(){
    PrivateNetworkCreds privateCreds;
    strcpy(privateCreds.ssid, config.ssid);
    strcpy(privateCreds.pass, config.pass);
    return privateCreds;
}

int loadConfig(){
    // load from fs
    objStoreInit(DB_VER);
    if(!objStoreGet("signalbrake", DEFAULT_CONFIG_ID, &config, sizeof(SignalBrakeConfig))){
        // store defaults
        Serial.println("signalbrake config not found, storing defaults");
        if(!writeDefaultConfig()){
            return 0;
        }
        // reload config now that we have defaults
        loadConfig();
    }
    return 1;
}

int writeConfig(SignalBrakeConfig * c){
    if(!objStoreUpdate("signalbrake", DEFAULT_CONFIG_ID, c, sizeof(SignalBrakeConfig))){
        Serial.println("failed to write config");
        return 0;
    }
    return 1;
}

static void logComponent(Identity * id){
    Serial.printf("\
    {\n\
        model: %i,\n\
        serial: %i,\n\
        bin: %i,\n\
        eventVer: %i,\n\
        dbVer: %i,\n\
    }\n",
    id->model, id->serial, id->bin, id->eventVer, id->dbVer);
}

void logConfig(SignalBrakeConfig * c){
    Serial.printf("\
config: {\n\
    ssid: %s,\n\
    hostname: %s,\n\
    networkMode: %s,\n\
    }\n",
    c->ssid, c->hostname,
    c->networkMode == 0 ? "CONNECT" : c->networkMode == 1 ? "CREATE" : "OFF");
}

int writeDefaultConfig(){
    // delete existing signalbrake config
    Serial.println("wiping signalbrake config");
    objStoreWipe("signalbrake");

    int id = objStoreCreate("signalbrake", &defaultConfig, sizeof(SignalBrakeConfig));
    if(!id){
        Serial.println("failed to write default config");
        return 0;
    }
    Serial.printf("wrote default signalbrake config, with id %i\n", id);
    return 1;
}

/*
// TODO - linked list isntead of array
int registerComponent(Identity * component){
    Identity * cur;

    Serial.printf("attempting to register component\n");
    logComponent(component);

    int i;
    for(i = 0; i < MAX_COMPONENTS; i++){
        cur = &(config.components[i]);

        // this one exists already, so update
        if(cur->serial == component->serial){
            Serial.printf("updating existing component with serial %i\n", cur->serial);
            identityCopy(cur, component);
            return 1;
        }

        if(!identityIsDefined(cur)){
            // hey, heres a free slot!
            break;
        }
    }

    // dont register more than MAX_COMPONENTS
    if(i >= MAX_COMPONENTS - 1){
        Serial.printf("cannot register more components, already maxed out\n");
        return 0;
    }

    // add the component
    Serial.printf("registering component to slot %i\n", i);
    cur = &(config.components[i]);
    identityCopy(cur, component);

    return 1;
}
*/
