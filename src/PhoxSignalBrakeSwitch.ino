#include <Esp.h>
#include <loop.h>
#include <statuslight.h>
#include <network.h>
#include <ota.h>
#include <digitalbutton.h>
#include <toggleswitch.h>
#include <eventReceiver.h>
#include <eventSender.h>
#include <eventRegistry.h>
#include <objstore.h>
#include <event.h>
#include <signalbrakeconfig.h>

void asplode(char * err){
    Serial.printf("ERROR: %s\n", err);
    delay(1000);
    ESP.restart();
}

StatusLight status;
SignalBrakeConfig * config = getConfig();
Identity * id = getIdentity();

IPAddress serverIP = IPAddress(SERVER_IP_UINT32);

void otaStarted(){
    Serial.println("ota start");
    statusLightStop(status);
}

void otaProgress(unsigned int progress, unsigned int total){
    Serial.print("ota progress");
}

void otaError(ota_error_t err){
    Serial.println("ota err");
}

void otaEnd(){
    Serial.println("ota end");
}

int setupStartHeap, setupEndHeap, prevHeap;
void logHeapUsage(void * state){
    int currHeap = ESP.getFreeHeap();
    int delta = setupEndHeap - currHeap;
    Serial.printf("currHeap: %i, delta: %i\n", currHeap, delta);
    prevHeap = currHeap;
}

// flashes status light real quick
void flash(){
    byte white[3] = {50,50,50};
    int pattern[] = {50, 100, 0};
    if(!statusLightSetPattern(status, white, pattern)){
        Serial.println("couldnt flash status light");
    }
    delay(50);
    statusLightStop(status);
}

void setNetworkMode(Event * e, Request * r){
    int mode = (e->body[1] << 8) + e->body[0];
    Serial.printf("setting network mode to %i\n", mode); 
    config->networkMode = (NetworkMode)mode;
    writeConfig(config);
    delay(100);
    flash();
}

void restoreDefaultConfig(Event * e, Request * r){
    Serial.println("restoring default signalbrake config");
    if(!writeDefaultConfig()){
        Serial.println("could not restore default config");
        return;
    }
    Serial.println("restored default config");
    delay(100);
    flash();
}

void ping(Event * e, Request * r){
    Serial.printf("got ping with requestId %i. I should respond\n", e->header->requestId);
    Serial.printf("responding to %s:%i\n", r->remoteIP.toString().c_str(), r->remotePort);
    if(!eventSendC(r->client, EVENT_VER, PONG, 0, NULL, NULL)){
        Serial.printf("ruhroh");
        return;
    }
    flash();
}

//int eventSendC(WiFiClient * client, int version, int opCode, int length, void * body, int responseId){
void who(Event * e, Request * r){
    Serial.printf("someone wants to know who i am\n");
    if(!eventSendC(r->client, EVENT_VER, WHO, sizeof(Identity), (void*)&id, NULL)){
        Serial.printf("ruhroh");
        return;
    }
}

void onToggleNeutral(TogglePosition last){
    Serial.println("toggle neutral");

    int offEvent;
    if(last == LEFT){
        offEvent = SIGNAL_L_OFF;
    } else if(last == RIGHT){
        offEvent = SIGNAL_R_OFF;
    }

    int ok = eventSend(serverIP, EVENT_PORT, EVENT_VER, offEvent, 0, 0, 0);
    if(!ok){
        Serial.println("couldnt send off event");
        return;
    }
    Serial.println("sent off event");
}
void onToggleLeft(TogglePosition last){
    Serial.println("toggle left");
    int ok = eventSend(serverIP, EVENT_PORT, EVENT_VER, SIGNAL_L_ON, 0, 0, 0);
    if(!ok){
        Serial.println("couldnt send left event");
        return;
    }
    Serial.println("sent left event");
}
void onToggleRight(TogglePosition last){
    Serial.println("toggle right");
    int ok = eventSend(serverIP, EVENT_PORT, EVENT_VER, SIGNAL_R_ON, 0, 0, 0);
    if(!ok){
        Serial.println("couldnt send right event");
        return;
    }
    Serial.println("sent right event");
}

/*
void requestRegisterComponent(Event * e, Request * r){
    Serial.printf("someone wants me to register a thing\n");
    // TODO - i suspect some sort of sanitization
    // and bounds checking should occur here
    Identity * component = (Identity*)e->body;
    if(!registerComponent(component)){
        Serial.printf("failed to register component\n");
        return;
    }

    // persist new config to disk
    if(!writeConfig(config)){
        Serial.printf("failed to write config to disk\n");
    }

    PrivateNetworkCreds creds = getPrivateCreds();
    if(!eventSendC(r->client, EVENT_VER, REGISTER_CONFIRM, sizeof(PrivateNetworkCreds), (void*)&creds, NULL)){
        Serial.printf("failed to respond to registration request\n");
        return;
    }
    flash();
}
*/

bool canOTA = true;
void neverOTAEver(){
    // button was released after boot, 
    // so don't allow OTA mode to happen
    canOTA = false;
}
void enterSyncMode(){
    Serial.println("entering sync mode");

    // status light
    byte blue[3] = {0,0,40};
    byte red[3] = {40,0,0};
    int pattern[] = {500,50,0};
    if(!statusLightSetPattern(status, blue, pattern)){
        Serial.println("couldnt setup status light");
    }

    // stop network so it can be restarted in
    // connect mode
    if(!networkStop()){
        Serial.println("couldn't stop network");
    }

    Serial.printf("OTA attempting to connect to ssid: %s, pass: %s\n",
        PUBLIC_SSID, PUBLIC_PASS);

    if(!networkConnect(PUBLIC_SSID, PUBLIC_PASS)){
        Serial.println("couldnt connect to ota network");
        statusLightSetPattern(status, red, pattern);
        return;
    }
    networkAdvertise(OTA_HOSTNAME);
    Serial.printf("OTA advertising hostname: %s\n", OTA_HOSTNAME);

    // enable SET_NETWORK_MODE endpoint just in case it isnt,
    // this way a device with NETWORK_MODE off will be able to
    // be turned back on
    eventListen(EVENT_VER, EVENT_PORT);
    eventRegister(SET_NETWORK_MODE, setNetworkMode);
    Serial.printf("Listening for SET_NETWORK_MODE with EVENT_VER: %i, eventPort: %i\n",
        EVENT_VER, EVENT_PORT);

    // ota
    otaOnStart(&otaStarted);
    otaOnProgress(&otaProgress);
    otaOnError(&otaError);
    otaOnEnd(&otaEnd);
    otaStart();

    byte green[3] = {0,40,0};
    int pattern2[] = {3000,50,0};
    if(!statusLightSetPattern(status, green, pattern2)){
        Serial.println("couldnt setup status light");
    }
}

void setup(){
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n");

    setupStartHeap = ESP.getFreeHeap();
    Serial.printf("setupStartHeap: %i\n", setupStartHeap);

    status = statusLightCreate(STATUS_PIN, 16);

    byte purple[3] = {20,0,20};
    int pattern[] = {1000,50,0};
    if(!statusLightSetPattern(status, purple, pattern)){
        Serial.println("couldnt setup status light");
    }

    // load config from fs
    loadConfig();
    logConfig(config);

    // status light
    byte blue[3] = {0,0,40};
    if(!statusLightSetPattern(status, blue, pattern)){
        Serial.println("couldnt setup status light");
    }

    // start network
    switch(config->networkMode){
        case CONNECT:
            if(!networkConnect(config->ssid, config->pass)){
                Serial.println("couldnt bring up network");
            }
            networkAdvertise(config->hostname);
            break;
        case CREATE:
            if(!networkCreate(config->ssid, config->pass, IPAddress(192,168,4,1))){
                Serial.println("couldnt create up network");
            }
            networkAdvertise(config->hostname);
            break;
        case OFF:
            Serial.println("turning network off");
            if(!networkOff()){
                Serial.println("couldnt turn off network");
            }
            break;
        default:
            Serial.println("couldnt load network mode, defaulting to CONNECT");
            if(!networkConnect(config->ssid, config->pass)){
                Serial.println("couldnt bring up network");
            }
            networkAdvertise(config->hostname);
            break;
    }

    byte green[3] = {0,40,0};
    if(!statusLightSetPattern(status, green, pattern)){
        Serial.println("couldnt setup status light");
    }

    byte yellow[3] = {20,20,20};
    if(!statusLightSetPattern(status, yellow, pattern)){
        Serial.println("couldnt setup status light");
    }

    if(eventListen(EVENT_VER, EVENT_PORT)){
        eventRegister(PING, ping);
        eventRegister(WHO, who);

        // these should eventually go to a safer API
        // (maybe in OTA mode only or something)
        eventRegister(SET_DEFAULT_CONFIG, restoreDefaultConfig);
        eventRegister(SET_NETWORK_MODE, setNetworkMode);
    }

    // TODO HACK REMOVE
    otaOnStart(&otaStarted);
    otaOnProgress(&otaProgress);
    otaOnError(&otaError);
    otaOnEnd(&otaEnd);
    otaStart();

    byte orange[3] = {20,20,0};
    if(!statusLightSetPattern(status, orange, pattern)){
        Serial.println("couldnt setup status light");
    }

    statusLightStop(status);

    // switch presets
    // OTA mode
    DigitalButton btn = buttonCreate(BUTTON_PIN, 50);
    buttonOnUp(btn, neverOTAEver);
    buttonOnHold(btn, enterSyncMode, 4000);

    // toggle switcheroo
    ToggleSwitch toggle = toggleCreate(TOGGLE_LEFT_PIN, TOGGLE_RIGHT_PIN, 1);
    toggleOnNeutral(toggle, onToggleNeutral);
    toggleOnLeft(toggle, onToggleLeft);
    toggleOnRight(toggle, onToggleRight);

    // debug log heap usage so i can keep an eye out for leaks
    setupEndHeap = ESP.getFreeHeap();
    Serial.printf("setupEndHeap: %i, delta: %i\n", setupEndHeap, setupStartHeap - setupEndHeap);
    loopAttach(logHeapUsage, 5000, NULL);
}

void loop(){
    loopTick();
}
