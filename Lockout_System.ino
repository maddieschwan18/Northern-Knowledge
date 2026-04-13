#define LED_PINA 1 //place holders for the buzzers + lights we are going to use
#define LED_PINB 2
#define LED_PINC 3
#define LED_PIND 4
#define BUTTON_PINA 1
#define BUTTON_PINB 2
#define BUTTON_PINC 3
#define BUTTON_PIND 4
void setup() {  //The setup code is used to turn on all LEDS as well as set up the buzzers to function
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PINA, HIGH);  //Turn all player LEDs on
  digitalWrite(LED_PINB, HIGH);
  digitalWrite(LED_PINC, HIGH);
  digitalWrite(LED_PIND, HIGH);
  pinMode(BUTTON_PIN, INPUT_PULLUP); //definiton of button (the buzzers)
  Serial.begin(9600);
}

void loop() {
  //This is the main portion of the lockout system, it checks if every button is turned on and if so if the buzzer corresponding to player A, B, C, or, D is pressed that LED stays on while all others turn off.
  //After the question timer is up (for current testing purposes it will be 10 seconds for right now) all the LEDS will turn back on.
  //NOTE: We need a reset function current idea is to do a confirm answer where the player hits their on buzzer again which skips the timer period
  while(LED_PINA == HIGH && LED_PINB == HIGH && LED_PINC == HIGH && LED_PIND == HIGH)//While every button is on to make sure you cant press when your buzzer is off
  if (digitalRead(BUTTON_PINA) == LOW) {  //Idea oh how lockout system works (If player A's buzzer is pressed the LEDS of other players shut off)
    digitalWrite(LED_PINB, LOW)
    digitalWrite(LED_PINC, LOW)
    digitalWrite(LED_PIND, LOW)
    delay(10000)                          //Delays for 10 seconds (this is the test time, the actual delay will be the legnth of the timer) Afterward all the LEDS will turn back on indicating that the next question is ready
    digitalWrite(LED_PINB, HIGH);
    digitalWrite(LED_PINC, HIGH);
    digitalWrite(LED_PIND, HIGH);
  }
  if (digitalRead(BUTTON_PINB) == LOW) {  
    digitalWrite(LED_PINA, LOW)
    digitalWrite(LED_PINC, LOW)
    digitalWrite(LED_PIND, LOW)
    delay(10000)                         
    digitalWrite(LED_PINA, HIGH);
    digitalWrite(LED_PINC, HIGH);
    digitalWrite(LED_PIND, HIGH);
  }
  if (digitalRead(BUTTON_PINc) == LOW) {  
    digitalWrite(LED_PINA, LOW)
    digitalWrite(LED_PINB, LOW)
    digitalWrite(LED_PIND, LOW)
    delay(10000)                         
    digitalWrite(LED_PINA, HIGH);
    digitalWrite(LED_PINB, HIGH);
    digitalWrite(LED_PIND, HIGH);
  }
  if (digitalRead(BUTTON_PIND) == LOW) {  
    digitalWrite(LED_PINA, LOW)
    digitalWrite(LED_PINB, LOW)
    digitalWrite(LED_PINC, LOW)
    delay(10000)                         
    digitalWrite(LED_PINA, HIGH);
    digitalWrite(LED_PINB, HIGH);
    digitalWrite(LED_PINC, HIGH);
  }
}
