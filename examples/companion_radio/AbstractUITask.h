#pragma once

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/MultiSerialInterface.h>
#include <Arduino.h>

#ifdef PIN_BUZZER
  #include <helpers/ui/buzzer.h>
#endif

#include "NodePrefs.h"
#include "MyMesh.h"

enum class UIEventType {
    none,
    contactMessage,
    channelMessage,
    roomMessage,
    newContactMessage,
    ack
};

class AbstractUITask : public MyMesh::Listener {
protected:
  mesh::MainBoard* _board;
  MultiSerialInterface* _interfaceManager;

  AbstractUITask(mesh::MainBoard* board, MultiSerialInterface* interfaceManager) : _board(board), _interfaceManager(interfaceManager) {
  }

public:
  bool hasConnection() const { return _interfaceManager->isConnected(); }
  virtual void setHasConnection(bool connected) { }  // state is derived; kept for the fork UI bridge
  uint16_t getBattMilliVolts() const { return _board->getBattMilliVolts(); }
  bool isBluetoothEnabled() const { return _interfaceManager->isBluetoothEnabled(); }
  virtual bool isDisplayOn() const { return false; }
  void enableBluetooth() { _interfaceManager->enableBluetooth(); }
  void disableBluetooth() { _interfaceManager->disableBluetooth(); }
  virtual void msgRead(int msgcount) = 0;
  // The phone took a message that was also kept on the device screen.
  virtual void clearTakenMsg() {}
  virtual void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount, bool group) = 0;
  virtual void notify(UIEventType t = UIEventType::none) = 0;
  virtual void loop() = 0;
};
