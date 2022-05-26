#ifndef LoRaModule_h
#define LoRaModule_h

#include <RadioLib.h>
#include <stdint.h>

#include <functional>

#include "Chip.h"
#include "LoRaFrame.h"

enum LdroType { LDRO_AUTO = 0,
                LDRO_ON = 1,
                LDRO_OFF = 2 };
enum ModemType { NONE = 0,
                 LORA = 1,
                 FSK = 2 };

struct LoraState {
  float freq;
  float bw;
  uint8_t sf;
  uint8_t cr;
  uint8_t syncWord;
  int8_t power;
  uint16_t preambleLength;
  uint8_t gain;
  LdroType ldro;
};

//FIXME address filtering
struct FskState {
  float freq;
  float br;
  float freqDev;
  float rxBw;
  uint16_t preambleLength;
  uint8_t sh;
  uint8_t *syncWord;
  size_t syncWordLength;
  int8_t power;
  uint8_t gain;
  bool ook;
};

class LoRaModule {
 public:
  ~LoRaModule();
  int16_t init(Chip *chip);
  // virtual functions used by Mock overrides in tests
  virtual int16_t startLoraRx(LoraState *request);
  virtual int16_t startFskRx(FskState *request);

  virtual int16_t loraTx(uint8_t *data, size_t dataLength, LoraState *request);
  virtual int16_t fskTx(uint8_t *data, size_t dataLength, FskState *request);

  virtual LoRaFrame *loop();
  virtual void stopRx();
  virtual bool isReceivingData();

  int getTempRaw(int8_t *value);
  void setOnRxStartedCallback(std::function<void()> func);
  void setOnRxStoppedCallback(std::function<void()> func);

 private:
  Module *module = NULL;
  PhysicalLayer *phys = NULL;
  ChipType type = ChipType::TYPE_SX1278;  // some default chip type
  bool receivingData = false;

  LoraState lora;
  bool loraInitialized = false;

  FskState fsk;
  bool fskInitialized = false;

  ModemType activeModem = ModemType::NONE;

  std::function<void()> rxStartedCallback = NULL;
  std::function<void()> rxStoppedCallback = NULL;

  LoRaFrame *readFrame();
  void reset();

  int16_t syncLoraModemState(LoraState *request);
  int16_t syncFskModemState(FskState *request);

  int16_t startReceive();
  int16_t transmit(uint8_t* data, size_t len);

  bool isSX1278();
  bool isSX1276();

  int16_t setLdro(uint8_t ldro);
  bool checkIfSyncwordEqual(FskState *f, FskState *s);
};

#endif