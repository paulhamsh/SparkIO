#include "Spark.h"
#include "SparkComms.h"
#include "BluetoothSerial.h"

#define HWSER_TX 18
#define HWSER_RX 5

SparkComms::SparkComms() {
  bt = NULL;
  ser = NULL;
}

SparkComms::~SparkComms() {
}

void SparkComms::start_ser() {
  uint8_t b;
  
  ser = new HardwareSerial(2); 
  ser->begin(HW_BAUD, SERIAL_8N1, HWSER_RX, HWSER_TX);

  while (ser->available())
    b = ser->read(); 
}

void SparkComms::start_bt() {
  bt = new BluetoothSerial();
  
  if (!bt->begin (MY_NAME, true)) {
    DEBUG("Bluetooth init fail");
    while (true);
  }    
}

void SparkComms::connect_to_spark() {
  uint8_t b;
  bool connected;

  connected = false;
  while (!connected) {
    connected = bt->connect(SPARK_NAME);
    if (!(connected && bt->hasClient())) {
      connected = false;
      delay(2000);
    }
  }

  // flush anything read from Spark - just in case

  while (bt->available())
    b = bt->read(); 
}
