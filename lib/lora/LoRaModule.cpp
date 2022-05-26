#include "LoRaModule.h"

#include <RadioLib.h>
#include <esp32-hal-log.h>
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

// flag to indicate that a packet was received
volatile bool receivedFlag = false;

// disable interrupt when it's not needed
volatile bool enableInterrupt = true;

int16_t LoRaModule::init(Chip *chip) {
  log_i("initialize chip: %s", chip->getName());
  reset();
  this->module = new Module(PIN_CS, PIN_DI0, PIN_RST);
  if (isSX1278()) {
    SX1278 *sx = new SX1278(module);
    this->phys = sx;
  } else if (isSX1276()) {
    SX1276 *sx = new SX1276(module);
    this->phys = sx;
  } else {
    log_e("unsupported chip found: %d", chip->getType());
    return ERR_CHIP_NOT_FOUND;
  }
  this->type = chip->getType();
  this->fsk.syncWord = NULL;
  this->fsk.syncWordLength = 0;
  return ERR_NONE;
}

void LoRaModule::reset() {
  if (this->receivingData) {
    this->stopRx();
  }
  if (this->module != NULL) {
    free(this->module);
    this->module = NULL;
  }
  if (this->phys != NULL) {
    free(this->phys);
    this->phys = NULL;
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

int16_t LoRaModule::startLoraRx(LoraState *request) {
  if (loraInitialized) {
    request->power = lora.power;
  }
  int16_t status = syncLoraModemState(request);
  if (status != ERR_NONE) {
    log_e("unable to init rx lora: %d", status);
    return status;
  }
  return startReceive();
}

int16_t LoRaModule::startFskRx(FskState *request) {
  if (fskInitialized) {
    request->power = fsk.power;
  }
  int16_t status = syncFskModemState(request);
  if (status != ERR_NONE) {
    log_e("unable to init rx fsk: %d", status);
    return status;
  }
  return startReceive();
}

int16_t LoRaModule::startReceive() {
  SX127x *genericSx;
  if (isSX1278()) {
    SX1278 *sx = (SX1278 *)phys;
    genericSx = sx;
  } else if (isSX1276()) {
    SX1276 *sx = (SX1276 *)phys;
    genericSx = sx;
  } else {
    return ERR_CHIP_NOT_FOUND;
  }
  genericSx->setDio0Action(setFlag);
  int16_t status = genericSx->startReceive();
  if (status == ERR_NONE) {
    this->receivingData = true;
    log_i("RX started");
    if (this->rxStartedCallback != NULL) {
      this->rxStartedCallback();
    }
  } else {
    log_e("unable to start rx: %d", status);
  }
  return status;
}

int16_t LoRaModule::syncLoraModemState(LoraState *request) {
  int16_t status;
  if (isSX1278()) {
    SX1278 *sx = (SX1278 *)this->phys;
    if (activeModem != ModemType::LORA) {
      status = sx->begin();
      // modem changed. force update of all parameters
      loraInitialized = false;
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.freq != request->freq) {
      status = sx->setFrequency(request->freq);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.bw != request->bw) {
      status = sx->setBandwidth(request->bw);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.cr != request->cr) {
      status = sx->setCodingRate(request->cr);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.gain != request->gain) {
      status = sx->setGain(request->gain);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.ldro != request->ldro) {
      status = setLdro(request->ldro);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.power != request->power) {
      status = sx->setOutputPower(request->power);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.preambleLength != request->preambleLength) {
      status = sx->setPreambleLength(request->preambleLength);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.sf != request->sf) {
      status = sx->setSpreadingFactor(request->sf);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.syncWord != request->syncWord) {
      status = sx->setSyncWord(request->syncWord);
      RADIOLIB_ASSERT(status);
    }
  } else if (isSX1276()) {
    SX1276 *sx = (SX1276 *)phys;
    if (activeModem != ModemType::LORA) {
      status = sx->begin();
      // modem changed. force update of all parameters
      loraInitialized = false;
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.freq != request->freq) {
      status = sx->setFrequency(request->freq);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.bw != request->bw) {
      status = sx->setBandwidth(request->bw);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.cr != request->cr) {
      status = sx->setCodingRate(request->cr);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.gain != request->gain) {
      status = sx->setGain(request->gain);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.ldro != request->ldro) {
      status = setLdro(request->ldro);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.power != request->power) {
      status = sx->setOutputPower(request->power);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.preambleLength != request->preambleLength) {
      status = sx->setPreambleLength(request->preambleLength);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.sf != request->sf) {
      status = sx->setSpreadingFactor(request->sf);
      RADIOLIB_ASSERT(status);
    }
    if (!loraInitialized || lora.syncWord != request->syncWord) {
      status = sx->setSyncWord(request->syncWord);
      RADIOLIB_ASSERT(status);
    }
  } else {
    return ERR_CHIP_NOT_FOUND;
  }
  memcpy(&lora, request, sizeof(LoraState));
  loraInitialized = true;
  activeModem = ModemType::LORA;
  return ERR_NONE;
}

int16_t LoRaModule::syncFskModemState(FskState *request) {
  int16_t status;
  if (isSX1278()) {
    SX1278 *sx = (SX1278 *)this->phys;
    if (activeModem != ModemType::FSK) {
      status = sx->beginFSK();
      // modem changed. force update of all parameters
      fskInitialized = false;
      RADIOLIB_ASSERT(status);

      status = sx->variablePacketLengthMode(SX127X_MAX_PACKET_LENGTH_FSK);
      RADIOLIB_ASSERT(status);
    }

    if (!fskInitialized || fsk.ook != request->ook) {
      status = sx->setOOK(request->ook);
      RADIOLIB_ASSERT(status);
    }

    if (!fskInitialized || fsk.br != request->br) {
      status = sx->setBitRate(request->br);
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || fsk.freq != request->freq) {
      status = sx->setFrequency(request->freq);
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || fsk.freqDev != request->freqDev) {
      status = sx->setFrequencyDeviation(request->freqDev);
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || fsk.gain != request->gain) {
      status = sx->setGain(request->gain);
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || fsk.power != request->power) {
      status = sx->setOutputPower(request->power);
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || fsk.preambleLength != request->preambleLength) {
      status = sx->setPreambleLength(request->preambleLength);
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || fsk.rxBw != request->rxBw) {
      status = sx->setRxBandwidth(request->rxBw);
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || fsk.sh != request->sh) {
      if (request->ook) {
        status = sx->setDataShapingOOK(request->sh);
      } else {
        status = sx->setDataShaping(request->sh);
      }
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || checkIfSyncwordEqual(&fsk, request)) {
      status = sx->setSyncWord(request->syncWord, request->syncWordLength);
      RADIOLIB_ASSERT(status);
    }

  } else if (isSX1276()) {
    SX1276 *sx = (SX1276 *)phys;
    if (activeModem != ModemType::FSK) {
      status = sx->beginFSK();
      // modem changed. force update of all parameters
      fskInitialized = false;
      RADIOLIB_ASSERT(status);

      status = sx->variablePacketLengthMode(SX127X_MAX_PACKET_LENGTH_FSK);
      RADIOLIB_ASSERT(status);
    }

    if (!fskInitialized || fsk.ook != request->ook) {
      status = sx->setOOK(request->ook);
      RADIOLIB_ASSERT(status);
    }

    if (!fskInitialized || fsk.br != request->br) {
      status = sx->setBitRate(request->br);
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || fsk.freq != request->freq) {
      status = sx->setFrequency(request->freq);
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || fsk.freqDev != request->freqDev) {
      status = sx->setFrequencyDeviation(request->freqDev);
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || fsk.gain != request->gain) {
      status = sx->setGain(request->gain);
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || fsk.power != request->power) {
      status = sx->setOutputPower(request->power);
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || fsk.preambleLength != request->preambleLength) {
      status = sx->setPreambleLength(request->preambleLength);
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || fsk.rxBw != request->rxBw) {
      status = sx->setRxBandwidth(request->rxBw);
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || fsk.sh != request->sh) {
      if (request->ook) {
        status = sx->setDataShapingOOK(request->sh);
      } else {
        status = sx->setDataShaping(request->sh);
      }
      RADIOLIB_ASSERT(status);
    }
    if (!fskInitialized || checkIfSyncwordEqual(&fsk, request)) {
      status = sx->setSyncWord(request->syncWord, request->syncWordLength);
      RADIOLIB_ASSERT(status);
    }
  } else {
    return ERR_CHIP_NOT_FOUND;
  }
  if (fsk.syncWord != NULL) {
    free(fsk.syncWord);
  }
  memcpy(&fsk, request, sizeof(FskState));
  fsk.syncWord = (uint8_t *)malloc(sizeof(uint8_t) * request->syncWordLength);
  if (fsk.syncWord == NULL) {
    return ERR_UNKNOWN;
  }
  memcpy(fsk.syncWord, request->syncWord, sizeof(uint8_t) * request->syncWordLength);

  fskInitialized = true;
  activeModem = ModemType::FSK;
  return ERR_NONE;
}

LoRaFrame *LoRaModule::loop() {
  if (!receivedFlag) {
    return NULL;
  }
  enableInterrupt = false;
  receivedFlag = false;

  LoRaFrame *result = readFrame();

  // put module back to listen mode
  int16_t status = ERR_NONE;
  if (isSX1278()) {
    SX1278 *sx = (SX1278 *)this->phys;
    status = sx->startReceive();
  } else if (isSX1276()) {
    SX1276 *sx = (SX1276 *)this->phys;
    status = sx->startReceive();
  }
  if (status != ERR_NONE) {
    log_e("unable to put module back to receive mode: %d", status);
  }

  // we're ready to receive more packets,
  // enable interrupt service routine
  enableInterrupt = true;
  return result;
}

LoRaFrame *LoRaModule::readFrame() {
  LoRaFrame *result = new LoRaFrame();
  result->setDataLength(this->phys->getPacketLength());
  uint8_t *data = (uint8_t *)malloc(sizeof(uint8_t) * result->getDataLength());
  if (data == NULL) {
    free(result);
    return NULL;
  }
  result->setData(data);

  int16_t status = this->phys->readData(data, result->getDataLength());
  if (status != ERR_NONE) {
    delete result;
    log_e("unable to read the frame: %d", status);
    return NULL;
  }
  time_t now;
  time(&now);
  result->setTimestamp(now);
  /**
    do not correct frequency error
    doppler correction should be more sophisticated, i.e.
     - take frequency error upon reception,
     - take time of the reception and calculate expected doppler frequency error
     - the difference between expected error and actual error is the "real
  error" RadioLib however doesn't expose a function for explicit frequency
  correction
  **/
  if (isSX1278()) {
    SX1278 *sx = (SX1278 *)this->phys;
    result->setRssi(sx->getRSSI());
    result->setSnr(sx->getSNR());
    result->setFrequencyError(sx->getFrequencyError(false));
  } else if (isSX1276()) {
    SX1276 *sx = (SX1276 *)this->phys;
    result->setRssi(sx->getRSSI());
    result->setSnr(sx->getSNR());
    result->setFrequencyError(sx->getFrequencyError(false));
  }
  log_i("frame received: %d bytes RSSI: %f SNR: %f", result->getDataLength(), result->getRssi(), result->getSnr());
  return result;
}

void LoRaModule::stopRx() {
  log_i("RX stopped");
  int16_t status = ERR_NONE;
  if (isSX1278()) {
    SX1278 *sx = (SX1278 *)this->phys;
    status = sx->sleep();
  } else if (isSX1276()) {
    SX1276 *sx = (SX1276 *)this->phys;
    status = sx->sleep();
  }
  if (status != ERR_NONE) {
    log_e("unable to put module back to sleep: %d", status);
  }
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
  if (this->phys == NULL) {
    return -1;
  }
  if (isSX1278()) {
    SX1278 *sx = (SX1278 *)this->phys;
    *value = sx->getTempRaw();
    return 0;
  } else if (isSX1276()) {
    SX1276 *sx = (SX1276 *)this->phys;
    *value = sx->getTempRaw();
    return 0;
  }
  return -1;
}

int16_t LoRaModule::loraTx(uint8_t *data, size_t dataLength, LoraState *request) {
  if (loraInitialized) {
    request->gain = lora.gain;
  }
  int16_t status = syncLoraModemState(request);
  if (status != ERR_NONE) {
    log_e("unable to init tx lora: %d", status);
    return status;
  }
  return transmit(data, dataLength);
}

int16_t LoRaModule::fskTx(uint8_t *data, size_t dataLength, FskState *request) {
  if (fskInitialized) {
    request->gain = fsk.gain;
  }
  int16_t status = syncFskModemState(request);
  if (status != ERR_NONE) {
    log_e("unable to init tx fsk: %d", status);
    return status;
  }
  return transmit(data, dataLength);
}

int16_t LoRaModule::transmit(uint8_t *data, size_t len) {
  int16_t status;
  if (isSX1278()) {
    SX1278 *sx = (SX1278 *)this->phys;
    status = sx->transmit(data, len);
  } else if (isSX1276()) {
    SX1276 *sx = (SX1276 *)this->phys;
    status = sx->transmit(data, len);
  } else {
    status = ERR_CHIP_NOT_FOUND;
  }
  if (status != ERR_NONE) {
    log_e("unable to tx: %d", status);
  }
  return status;
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
  int16_t status = ERR_NONE;
  if (isSX1278()) {
    SX1278 *sx = (SX1278 *)this->phys;
    switch (ldro) {
      case LDRO_AUTO:
        status = sx->autoLDRO();
        break;
      case LDRO_ON:
        status = sx->forceLDRO(true);
        break;
      case LDRO_OFF:
        status = sx->forceLDRO(false);
        break;
      default:
        break;
    }
  } else if (isSX1276()) {
    SX1276 *sx = (SX1276 *)phys;
    switch (ldro) {
      case LDRO_AUTO:
        status = sx->autoLDRO();
        break;
      case LDRO_ON:
        status = sx->forceLDRO(true);
        break;
      case LDRO_OFF:
        status = sx->forceLDRO(false);
        break;
      default:
        break;
    }
  } else {
    status = ERR_CHIP_NOT_FOUND;
  }
  return status;
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