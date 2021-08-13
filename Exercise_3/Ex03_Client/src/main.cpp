/**
 * A BLE client example that is rich in capabilities.
 * There is a lot new capabilities implemented.
 * author unknown
 * updated by chegewara
 */
#include <Arduino.h>
#include "BLEDevice.h"
#include <base64.h>
#include "mbedtls/md.h"

// The remote service we wish to connect to.
static BLEUUID serviceUUID("f7826da6-4fa2-4e98-8024-bc5b71e0893e");
// The characteristic of the remote service we are interested in.
static BLEUUID charUUID("b9407f30-f5f8-466e-aff9-25556b57fe6d");

static bool doConnect = false;
static bool connected = false;
static bool doScan = false;
static BLERemoteCharacteristic *pRemoteCharacteristic;
static BLEAdvertisedDevice *myDevice;

static void notifyCallback(
    BLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{
  Serial.print("Notify callback for characteristic ");
  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  Serial.print(" of data length ");
  Serial.println(length);
  Serial.print("data: ");
  for (size_t i = 0; i < length; i++)
  {
    Serial.print(pData[i], HEX);
  }
  Serial.println();
  // The secret we recovered using the brute-force calculation
  uint8_t secret[] = {0x39, 0x64, 0x4F, 0x07};
  Serial.print("Flag: ");
  for (size_t i = 0; i < 4; i++)
  {
    Serial.print(pData[i] ^ secret[i], HEX);
  }
  Serial.println();
}

class MyClientCallback : public BLEClientCallbacks
{
  void onConnect(BLEClient *pclient)
  {
  }

  void onDisconnect(BLEClient *pclient)
  {
    connected = false;
    Serial.println("onDisconnect");
  }
};

bool connectToServer()
{
  Serial.print("Forming a connection to ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient *pClient = BLEDevice::createClient();
  Serial.println(" - Created client");

  pClient->setClientCallbacks(new MyClientCallback());

  // Connect to the remove BLE Server.
  pClient->connect(myDevice); // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  Serial.println(" - Connected to server");

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService *pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr)
  {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");

  // Obtain a reference to the characteristic in the service of the remote BLE server.
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr)
  {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our characteristic");

  // Read the value of the characteristic.
  if (pRemoteCharacteristic->canRead())
  {
    std::string value = pRemoteCharacteristic->readValue();
    Serial.print("The characteristic value was: ");
    for (int i = 0; i < 4; i++)
    {
      Serial.print(value[i], HEX);
    }
    Serial.println();
  }

  if (pRemoteCharacteristic->canNotify())
    pRemoteCharacteristic->registerForNotify(notifyCallback);

  connected = true;
}
/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID))
    {

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;

    } // Found our server
  }   // onResult
};    // MyAdvertisedDeviceCallbacks

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan *pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
  // Use unconnected analog pin as source of randomness for seed
  randomSeed(analogRead(A0));
} // End of setup.

// This is the Arduino main loop function.
void loop()
{

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if (doConnect == true)
  {
    if (connectToServer())
    {
      Serial.println("We are now connected to the BLE Server.");
    }
    else
    {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }

  // If we are connected to a peer BLE Server, update the characteristic each time we are reached
  // with the current time since boot.
  if (connected)
  {
    /*
    // Replay the stored mat1, mat2, nonce, h to the server. This is done to retrieve our encrypted flag.
    String newValue = "{mat1:2565427,mat2:2572741,nonce:xmZ5adIxEJCBFp/GBywx6Q==,sha256sum:lcQ3AGW++w7zuiOtC2t2JpCs6XIcsy6+28PUIf/zxo8=}";
     */

    // Construct a new payload to send to the server. This is done to verify that the secret we discovered is correct.
    String newValue = "{mat1:2565427,mat2:2572741,nonce:";
    // Generate random nonce
    uint8_t nonce[16];
    for (uint8_t i = 0; i < 16; i++)
    {
      nonce[i] = (uint8_t)random(0, 255);
    }
    // Encode nonce as a base64 string
    String encoded_nonce = base64::encode(nonce, 16);
    // Append nonce to payload
    newValue.concat(encoded_nonce);

    newValue.concat(",sha256sum:");
    // Construct payload over which sha256 will be calculated. First: matriculation numbers
    String sha_payload = "25654272572741";
    // Second: base64-encoded nonce
    sha_payload.concat(encoded_nonce);
    // Third: secret. We appended any four printable characters then replaced each one with a byte of the secret
    sha_payload.concat("aaaa");
    sha_payload.setCharAt(sha_payload.length() - 4, 0x39);
    sha_payload.setCharAt(sha_payload.length() - 3, 0x64);
    sha_payload.setCharAt(sha_payload.length() - 2, 0x4F);
    sha_payload.setCharAt(sha_payload.length() - 1, 0x07);
    // Calculate the hash
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char *)sha_payload.c_str(), sha_payload.length());
    byte shaResult[32];
    mbedtls_md_finish(&ctx, shaResult);
    mbedtls_md_free(&ctx);
    // Encode the hash as a base64 string
    String encoded_hash = base64::encode(shaResult, 32);
    // Finally, append the encoded hash to the payload
    newValue.concat(encoded_hash);
    
    newValue.concat("}");

    Serial.println("Setting new characteristic value to \"" + newValue + "\"");

    // Set the characteristic's value to be the array of bytes that is actually a string.
    pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());
  }
  else if (doScan)
  {
    BLEDevice::getScan()->start(0); // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
  }

  delay(1000); // Delay a second between loops.
} // End of loop