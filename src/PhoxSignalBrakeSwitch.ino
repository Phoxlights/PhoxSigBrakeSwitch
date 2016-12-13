#include <Esp.h>
#include <WiFiClient.h>
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

#define DEV_MODE 1

void asplode(char * err){
    Serial.printf("ERROR: %s\n", err);
    delay(1000);
    ESP.restart();
}

StatusLight status;
SignalBrakeConfig * config = getConfig();
Identity * id = getIdentity();
DigitalButton btn = buttonCreate(BUTTON_PIN, 50);

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

void onBrakeDown(){
    Serial.println("brake on");
    int ok = eventSend(serverIP, EVENT_PORT, EVENT_VER, BRAKE_ON, 0, 0, 0);
    if(!ok){
        Serial.println("couldnt send brake on event");
        return;
    }
    Serial.println("sent brake on event");
}

void onBrakeUp(){
    Serial.println("brake off");
    int ok = eventSend(serverIP, EVENT_PORT, EVENT_VER, BRAKE_OFF, 0, 0, 0);
    if(!ok){
        Serial.println("couldnt send brake off event");
        return;
    }
    Serial.println("sent brake off event");
}

int setIdleStatusLight(){
    int pattern[] = {0};
    byte faded[3] = {0,0,20};
    if(!statusLightSetPattern(status, faded, pattern)){
        Serial.println("couldnt setup status light");
        return 0;
    }
    return 1;
}

int setBusyStatusLight(){
    int pattern[] = {250,250,0};
    byte faded[3] = {0,0,20};
    if(!statusLightSetPattern(status, faded, pattern)){
        Serial.println("couldnt setup status light");
        return 0;
    }
    return 1;
}

int setFailStatusLight(){
    int pattern[] = {250,250,0};
    byte red[3] = {255,0,0};
    if(!statusLightSetPattern(status, red, pattern)){
        Serial.println("couldnt setup status light");
        return 0;
    }
    return 1;
}

int setSuccessStatusLight(){
    int pattern[] = {250,250,0};
    byte green[3] = {0,255,0};
    if(!statusLightSetPattern(status, green, pattern)){
        Serial.println("couldnt setup status light");
        return 0;
    }
    return 1;
}

void flashFailStatusLight(){
    setFailStatusLight();
    delay(3000);
    // TODO - return to previous status?
    setIdleStatusLight();
}

void flashSuccessStatusLight(){
    setSuccessStatusLight();
    delay(3000);
    // TODO - return to previous status?
    setIdleStatusLight();
}

bool registrationPending = false;

void resetRegistrationRequest(WiFiClient * client){
    registrationPending = false;
    if(client){
        if(!eventUnListenC(client)){
            Serial.println("couldn't unlisten registration response client; ignoring");
        }
    }
}

void sendRegistrationRequest(){
    Serial.println("sending registration request");

    if(registrationPending){
        Serial.println("but a registration is already pending");
        flashFailStatusLight();
        return;
    }

    if(!setBusyStatusLight()){
        Serial.println("couldnt setup status light");
        flashFailStatusLight();
        return;
    }

    registrationPending = true;
    
    // TODO - avoid using arduino api directly like this
    WiFiClient * client = new WiFiClient();

    if(!client->connect(serverIP, EVENT_PORT)){
        Serial.printf("couldnt connect to %s:%i\n", serverIP.toString().c_str(), EVENT_PORT);
        flashFailStatusLight();
        resetRegistrationRequest(client);
        return;
    }

    int ok = eventSendC(client, EVENT_VER, REGISTER_COMPONENT,
        sizeof(Identity), (void*)id, 0);

    if(!ok){
        Serial.println("couldnt send registration request");
        flashFailStatusLight();
        resetRegistrationRequest(client);
        return;
    }

    if(!eventListenC(client)){
        Serial.println("couldnt start listening for registration response");
        flashFailStatusLight();
        resetRegistrationRequest(NULL);
        return;
    }

    // TODO - setup timeout and call resetRegistrationRequest
    // if timeout is exceeded
    Serial.println("sent registration request. now we wait");
}

void receiveRegistrationResponse(Event * e, Request * r){
    Serial.println("got registration request response");
    
    if(!registrationPending){
        Serial.println("but there was no pending registration request; ignoring");
        resetRegistrationRequest(r->client);
        return;
    }

    // TODO - put this in a shared location because
    // it will be used by any phoxdevice
    struct PrivateNetworkCreds {
        char ssid[SSID_MAX];
        char pass[PASS_MAX];
    };

    // TODO - dont blindly write junk off the network?
    struct PrivateNetworkCreds * creds = (PrivateNetworkCreds*)e->body;
    strcpy(config->ssid, creds->ssid);
    strcpy(config->pass, creds->pass);

    if(!writeConfig(config)){
        Serial.println("failed to save newly registered network creds");
        flashFailStatusLight();
        return;
    }

    Serial.printf("successfully registered device to ssid: %s\n", config->ssid);
    flashSuccessStatusLight();

    resetRegistrationRequest(r->client);
}

// events to listen for in run mode
int startRunListeners(){
    int ok = eventListen(EVENT_VER, EVENT_PORT);
    if(ok){
        eventRegister(PING, ping);
        eventRegister(WHO, who);

        Serial.printf("Listening for events with EVENT_VER: %i, eventPort: %i\n",
            EVENT_VER, EVENT_PORT);
    }
    return ok;
}

// events to listen for in sync mode
int startSyncListeners(){
    int ok = eventListen(EVENT_VER, EVENT_PORT);
    if(ok){
        eventRegister(SET_DEFAULT_CONFIG, restoreDefaultConfig);
        eventRegister(SET_NETWORK_MODE, setNetworkMode);
        eventRegister(REGISTER_CONFIRM, receiveRegistrationResponse);

        Serial.printf("Listening for events with EVENT_VER: %i, eventPort: %i\n",
            EVENT_VER, EVENT_PORT);
    }
    return ok;
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

    // ota
    otaOnStart(&otaStarted);
    otaOnProgress(&otaProgress);
    otaOnError(&otaError);
    otaOnEnd(&otaEnd);
    otaStart();

    buttonOnTap(btn, sendRegistrationRequest);

    byte green[3] = {0,40,0};
    int pattern2[] = {3000,50,0};
    if(!statusLightSetPattern(status, green, pattern2)){
        Serial.println("couldnt setup status light");
    }
}

int shouldEnterSyncMode(){
    int buttonPosition;
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(BUTTON_PIN, HIGH);
    buttonPosition = digitalRead(BUTTON_PIN);

    for(int i = 0; i < 5; i++){
        if(buttonPosition == HIGH){
            return 0;
        }
        delay(200);
    }

    if(buttonPosition == HIGH){
        return 0;
    }

    return 1;
}

void setup(){
    Serial.begin(115200);
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

    // HACK - works around issue where this device
    // cannot make tcp connections to an esp which
    // is serving as an AP
    WiFi.persistent(false);

    if(shouldEnterSyncMode()){
        Serial.println("going to sync mode");
        enterSyncMode();
        return;
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

    if(!startRunListeners()){
        Serial.println("couldnt start listening for events");
    }

    // NOTE - this stuff is unsafe for run mode! make sure
    // DEV_MODE is off in production!
    if(DEV_MODE){
        if(!startSyncListeners()){
            Serial.println("couldnt start sync mode listeners");
        }
        otaOnStart(&otaStarted);
        otaOnProgress(&otaProgress);
        otaOnError(&otaError);
        otaOnEnd(&otaEnd);
        otaStart();

        buttonOnTap(btn, sendRegistrationRequest);
    }

    byte orange[3] = {20,20,0};
    if(!statusLightSetPattern(status, orange, pattern)){
        Serial.println("couldnt setup status light");
    }

    // toggle switcheroo
    ToggleSwitch toggle = toggleCreate(TOGGLE_LEFT_PIN, TOGGLE_RIGHT_PIN, 1);
    toggleOnNeutral(toggle, onToggleNeutral);
    toggleOnLeft(toggle, onToggleLeft);
    toggleOnRight(toggle, onToggleRight);

    // brake sensor
    DigitalButton brake = buttonCreate(BRAKE_PIN, 50);
    buttonOnDown(brake, onBrakeDown);
    buttonOnUp(brake, onBrakeUp);

    if(!setIdleStatusLight()){
        Serial.println("couldnt setup status light");
    }

    // debug log heap usage so i can keep an eye out for leaks
    setupEndHeap = ESP.getFreeHeap();
    Serial.printf("setupEndHeap: %i, delta: %i\n", setupEndHeap, setupStartHeap - setupEndHeap);
    loopAttach(logHeapUsage, 5000, NULL);
}

void loop(){
    loopTick();
}
