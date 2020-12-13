// PJ104 MQ2 gas sensor module

// PJ104 software version
#define MODULE_VERSION          1
// PJ104 by default accepts configuratio change
#define MODULE_ACCEPT_CONFIG true
// PJ104 default mode of operation (0 = master-slave, 1 = multi-master)
#define MODULE_MODE             0
// PJ104 MQ2 gas sensor sensor pin
#define MODULE_GAS_PIN          0
// PJ104 MQ2 gas sensor sample threshold
#define MODULE_THRESHOLD      512
// PJ104 PIR sample rate
#define MODULE_SAMPLE_RATE   1000

// PJON configuration
// Do not use internal packet buffer (reduces memory footprint)
#define PJON_MAX_PACKETS        0

#include <PJONSoftwareBitBang.h>
#include <EEPROM.h>

// Instantiate PJON
PJONSoftwareBitBang bus;

uint8_t recipient_id;
bool accept_config_change;
uint32_t time;
bool value = 0;
uint8_t packet[3];
bool mode;

void setup() {
  // Writing default configuration in EEPROM
  if(
    EEPROM.read(4) != 'P' ||
    EEPROM.read(5) != 'J' ||
    EEPROM.read(6) != '1' ||
    EEPROM.read(7) != '0' ||
    EEPROM.read(8) != '4' ||
    EEPROM.read(9) != MODULE_VERSION
  ) EEPROM_write_default_configuration();
  EEPROM_read_configuration();
  // Use pin 1 for PJON communicaton
  bus.strategy.set_pin(1);
  // Begin PJON communication
  bus.begin();
  // Register the receiver callback called when a packet is received
  bus.set_receiver(receiver_function);
  // Setup pin which is connected to the MQ2 gas sensor
  pinMode(MODULE_GAS_PIN, INPUT);
  digitalWrite(MODULE_GAS_PIN, LOW);
}

void EEPROM_read_configuration() {
  bus.set_id(EEPROM.read(0));
  recipient_id = EEPROM.read(1);
  mode = EEPROM.read(2);
  accept_config_change = EEPROM.read(9);
};

void EEPROM_write_default_configuration() {
  // LEDAR ID
  EEPROM.update(0, PJON_NOT_ASSIGNED);
  // Recipient ID
  EEPROM.update(1, PJON_MASTER_ID);
  // Mode of operation
  EEPROM.update(2, MODULE_MODE);
  // Module name
  EEPROM.update(3, 'P');
  EEPROM.update(4, 'J');
  EEPROM.update(5, '1');
  EEPROM.update(6, '0');
  EEPROM.update(7, '4');
  EEPROM.update(8, MODULE_VERSION);
  // Accept incoming configuration
  EEPROM.update(9, MODULE_ACCEPT_CONFIG);
};

void loop() {
  bus.receive(1000 + random(0, 1000));
  if(!PJON_IO_READ(MODULE_GAS_PIN)) value = true;
  if(mode && value && (bus.send_packet(recipient_id, "1", 1) == PJON_ACK)) {
    value = false;
    time = millis();
    while((millis() - time) < MODULE_SAMPLE_RATE) bus.receive(1000);
  }
}

void receiver_function(uint8_t *payload, uint16_t length, const PJON_Packet_Info &info) {
  bool is_master =
    (info.tx.id == PJON_MASTER_ID) || (info.tx.id == recipient_id);
  // Send MQ2 gas sensor value
  if(is_master && (payload[0] == 'E')) {
    uint8_t result[2];
    result[0] = !PJON_IO_READ(MODULE_GAS_PIN) ? '1' : '0';
    bus.send_packet(recipient_id, result, 1);
  }
  // INFO REQUEST
  if(payload[0] == '?') {
    uint8_t module_name[6] = {
      EEPROM.read(3),
      EEPROM.read(4),
      EEPROM.read(5),
      EEPROM.read(6),
      EEPROM.read(7),
      EEPROM.read(8)
    };
    bus.send_packet(recipient_id, module_name, 6);
  }

  if(!accept_config_change) return;

  // DEVICE ID UPDATE
  if(is_master && (payload[0] == 'I')) {
    bus.set_id(payload[1]);
    EEPROM.update(0, payload[1]);
  }
  // MODE UPDATE
  if(is_master && (payload[0] == 'M')) {
    mode = payload[1];
    EEPROM.update(2, payload[1]);
  }
  // RECIPIENT ID UPDATE
  if(is_master && (payload[0] == 'R')) {
    recipient_id = payload[1];
    EEPROM.update(1, recipient_id);
  }
  // DANGER ZONE
  // Attention when X is received configuration is set to default
  if(is_master && (payload[0] == 'X')) {
    EEPROM_write_default_configuration();
    EEPROM_read_configuration();
  }
  // Attention when Q is received the module will stop to accept commands
  if(is_master && (payload[0] == 'Q')) {
    accept_config_change = false;
    EEPROM.update(9, 0);
  }
};
