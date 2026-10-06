#ifndef NFC_MANAGER_H
#define NFC_MANAGER_H

#include <Arduino.h>
#include "HardwareConfig.h"

#if !ENABLE_NFC_MFRC522
	#error "NFC_MANAGER requires ENABLE_NFC_MFRC522=1 in HardwareConfig.h"
#endif

#include <SPI.h>
#include <MFRC522.h>

// Pin assignments for MFRC522 NFC module (configured in HardwareConfig.h)
#define SS_PIN  NFC_SS_PIN   // CS (SDA) pin of MFRC522
#define RST_PIN NFC_RST_PIN  // RST pin of MFRC522

void nfc_init();
void nfc_update();
bool nfc_has_new_tag();
String nfc_get_last_tag();

#endif
