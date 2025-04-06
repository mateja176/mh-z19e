#include "PubSubClient.h"

/* Variables */

const char *NAME = "mick";
const char *VERSION = "0.0.1";

/* WiFi */

const unsigned long WIFI_DELAY_MS = 5000;
const unsigned long AP_THRESHOLD_MS = 4000;

/* PubSub */

const IPAddress PUBSUB_BROKER_IP_ADDRESS = {192, 168, 1, 18};
const unsigned int PUBSUB_PORT = 1883;
const char *PUBSUB_USER = NULL;
const char *PUBSUB_PW = NULL;
const char *TOPIC_INPUT = "input";
const unsigned long PUBSUB_DELAY_MS = 2000;

/* Hardware */

const uint8_t TX_PIN = D1;
const uint8_t RX_PIN = D2;
const uint8_t MEASUREMENT_INTERVAL_MS = 2000;
