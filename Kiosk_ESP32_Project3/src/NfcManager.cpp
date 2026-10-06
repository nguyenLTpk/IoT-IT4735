#include "NfcManager.h"

// Local MFRC522 instance
MFRC522 mfrc522(SS_PIN, RST_PIN);  

static String nfcBuffer = "";
static bool nfcTagReady = false;

// Cooldown to avoid reading the same card repeatedly when it is kept on the reader
static unsigned long lastTimeRead = 0;
static const unsigned long READ_COOLDOWN_MS = 2000; 

// [ĐÃ GỠ] Hàm nfc_is_rewritable_uid_card() trước đây phát hiện thẻ clone bằng cách GHI toàn số 0
// vào block 0: thẻ magic CUID/Gen2 dùng khoá mặc định bị xoá UID vĩnh viễn ngay lần chạm đầu.
// Đầu đọc nay CHỈ ĐỌC UID, tuyệt đối không ghi dữ liệu lên thẻ.

void nfc_init() {
  SPI.begin();           // Initialize HW SPI bus (18, 19, 23)
  mfrc522.PCD_Init();    // Initialize MFRC522 chip
  Serial.println(F("[NFC/MFRC522] Initialized on SPI bus (CS:5, RST:32)"));
}

void nfc_update() {
  // 1. Only check card when cooldown has passed
  if (millis() - lastTimeRead < READ_COOLDOWN_MS) {
      return; 
  }

  // 2. Look for a new card (returns true if there is a card present)
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // 3. Read card data (manufacturer UID)
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // 4. If UID read is successful -> convert to hex string (e.g. 4 bytes [A1 B2 C3 01] => "A1B2C301")
  String uidString = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    // Add leading '0' if current byte is < 0x10 to keep 2‑digit formatting
    if(mfrc522.uid.uidByte[i] < 0x10) {
        uidString += "0";
    }
    uidString += String(mfrc522.uid.uidByte[i], HEX);
  }
  
  uidString.toUpperCase();

  Serial.printf("[NFC/MFRC522] Detected card with UID: %s\n", uidString.c_str());

  // 5. Store UID in buffer for main loop to consume
  nfcBuffer = uidString;
  nfcTagReady = true;

  // 6. Update last read timestamp (start cooldown window)
  lastTimeRead = millis();

  // 7. Halt communication with the current card so the reader can serve others
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

bool nfc_has_new_tag() {
  return nfcTagReady;
}

String nfc_get_last_tag() {
  String tag = nfcBuffer;
  nfcBuffer = "";
  nfcTagReady = false;
  return tag; // Return UID in uppercase, e.g. 05A2B1BC
}
