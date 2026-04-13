#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

//Create radio object (CE, CSN pins)
RF24 radio(9, 10);

//Communication address (needs to match receiver)
const byte address[6] = "00001";

//Unique ID for each button
char PLAYER_ID = 'A';

//Pin definitions
#define BUTTON_PIN 2
#define LED_PIN 3

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Button (LOW when pressed)
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, HIGH); // LED ON = ready

  radio.begin();
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_LOW);
  radio.stopListening(); // Set as transmitter
}

void loop() {
  //Check if button is pressed
  if (digitalRead(BUTTON_PIN) == LOW) {

    //Send player ID wirelessly
    radio.write(&PLAYER_ID, sizeof(PLAYER_ID));

    //Turn off LED to indicate lockout
    digitalWrite(LED_PIN, LOW);

    delay(300); // debounce
  }
}
