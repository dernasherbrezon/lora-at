#include "LoRaModule.h"

#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <esp32-hal-log.h>
#include <esp_intr_alloc.h>
#include <time.h>

// defined in platformio.ini
#ifndef PIN_CS
#define PIN_CS 0
#endif
#ifndef PIN_DI0
#define PIN_DI0 0
#endif
#ifndef PIN_RST
#define PIN_RST 0
#endif

#define SCK 5
#define MISO 19
#define MOSI 27

// flag to indicate that a packet was received
volatile bool receivedFlag = false;

// disable interrupt when it's not needed
volatile bool enableInterrupt = true;

int16_t LoRaModule::init(Chip *chip) {
  log_i("initialize chip: %s", chip->getName());
  reset();
  spi_bus_config_t config = {
      .mosi_io_num = MOSI,
      .miso_io_num = MISO,
      .sclk_io_num = SCK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 0,
  };
  esp_err_t code = spi_bus_initialize(HSPI_HOST, &config, 0);
  if (code != ESP_OK) {
    log_i("can't init bus");
    return code;
  }
  code = sx1278_create(HSPI_HOST, PIN_CS, &device);
  if (code != ESP_OK) {
    log_i("can't create device");
    return code;
  }
  this->type = chip->getType();
  this->fsk.syncWord = NULL;
  this->fsk.syncWordLength = 0;
  return 0;
}

void LoRaModule::reset() {
  if (this->receivingData) {
    this->stopRx();
  }
  if (this->device != NULL) {
    sx1278_destroy(this->device);
    this->device = NULL;
  }
  // some default type
  this->type = ChipType::TYPE_SX1278;
  this->activeModem = ModemType::NONE;
  this->fskInitialized = false;
  if (this->fsk.syncWord != NULL) {
    free(this->fsk.syncWord);
    this->fsk.syncWord = NULL;
  }
  this->loraInitialized = false;
}

// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
void setFlag(void) {
  // check if the interrupt is enabled
  if (!enableInterrupt) {
    return;
  }

  // we got a packet, set the flag
  receivedFlag = true;
}

int16_t LoRaModule::startLoraRx(ObservationRequest *request) {
  if (loraInitialized) {
    request->power = lora.power;
  }
  esp_err_t code = sx1278_set_opmod(SX1278_MODE_SLEEP, this->device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx1278_set_frequency(request->freq * 1E6, this->device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx1278_reset_fifo(this->device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx1278_set_lna_boost_hf(SX1278_LNA_BOOST_HF_ON, this->device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx1278_set_opmod(SX1278_MODE_STANDBY, this->device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx1278_set_lna_gain((sx1278_gain_t)(request->gain << 5), this->device);
  if (code != ESP_OK) {
    return code;
  }
  sx1278_bw_t bw;
  if (request->bw == 7.8) {
    bw = SX1278_BW_7800;
  } else if (request->bw == 10.4) {
    bw = SX1278_BW_10400;
  } else if (request->bw == 15.6) {
    bw = SX1278_BW_15600;
  } else if (request->bw == 20.8) {
    bw = SX1278_BW_20800;
  } else if (request->bw == 31.25) {
    bw = SX1278_BW_31250;
  } else if (request->bw == 41.7) {
    bw = SX1278_BW_41700;
  } else if (request->bw == 62.5) {
    bw = SX1278_BW_62500;
  } else if (request->bw == 125.0) {
    bw = SX1278_BW_125000;
  } else if (request->bw == 250.0) {
    bw = SX1278_BW_250000;
  } else if (request->bw == 500.0) {
    bw = SX1278_BW_500000;
  } else {
    return -1;
  }
  code = sx1278_set_bandwidth(bw, this->device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx1278_set_implicit_header(NULL, this->device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx1278_set_modem_config_2((sx1278_sf_t)(request->sf << 4), this->device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx1278_set_syncword(request->syncWord, this->device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx1278_set_preamble_length(request->preambleLength, this->device);
  if (code != ESP_OK) {
    return code;
  }
  code = sx1278_set_opmod(SX1278_MODE_RX_CONT, this->device);
  if (code != ESP_OK) {
    return code;
  }
  return ESP_OK;
}

int16_t LoRaModule::startFskRx(FskState *request) {
  return -1;
}

int16_t LoRaModule::startReceive() {
  return -1;
}

int16_t LoRaModule::syncLoraModemState(ObservationRequest *request) {
  return -1;
}

LoRaFrame *LoRaModule::readFrame() {
  return NULL;
}

void LoRaModule::stopRx() {
  log_i("RX stopped");
  sx1278_set_opmod(SX1278_MODE_SLEEP, this->device);
  this->receivingData = false;
  if (this->rxStoppedCallback != NULL) {
    this->rxStoppedCallback();
  }
}

bool LoRaModule::isReceivingData() {
  return this->receivingData;
}

void LoRaModule::setOnRxStartedCallback(std::function<void()> func) {
  this->rxStartedCallback = func;
}
void LoRaModule::setOnRxStoppedCallback(std::function<void()> func) {
  this->rxStoppedCallback = func;
}

int LoRaModule::getTempRaw(int8_t *value) {
  return -1;
}

int16_t LoRaModule::loraTx(uint8_t *data, size_t dataLength, ObservationRequest *request) {
  return 0;
}

int16_t LoRaModule::fskTx(uint8_t *data, size_t dataLength, FskState *request) {
  return 0;
}

int16_t LoRaModule::transmit(uint8_t *data, size_t len) {
  return 0;
}

bool LoRaModule::isSX1278() {
  if (this->type == ChipType::TYPE_SX1278) {
    return true;
  }
  return false;
}

bool LoRaModule::isSX1276() {
  if (this->type == ChipType::TYPE_SX1276_868 || this->type == ChipType::TYPE_SX1276_433 || this->type == ChipType::TYPE_SX1276) {
    return true;
  }
  return false;
}

int16_t LoRaModule::setLdro(uint8_t ldro) {
  switch (ldro) {
    case LDRO_AUTO:
      return 0;
    case LDRO_ON:
      return sx1278_set_low_datarate_optimization(SX1278_LOW_DATARATE_OPTIMIZATION_ON, this->device);
    case LDRO_OFF:
      return sx1278_set_low_datarate_optimization(SX1278_LOW_DATARATE_OPTIMIZATION_OFF, this->device);
    default:
      return -1;
  }
}

bool LoRaModule::checkIfSyncwordEqual(FskState *f, FskState *s) {
  if (f->syncWordLength != s->syncWordLength) {
    return false;
  }
  for (size_t i = 0; i < s->syncWordLength; i++) {
    if (f->syncWord[i] != s->syncWord[i]) {
      return false;
    }
  }
  return true;
}

LoRaModule::~LoRaModule() {
  reset();
}