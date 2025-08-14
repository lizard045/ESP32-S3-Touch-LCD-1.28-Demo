int IRVal = 0;
int IRPin = 8;


void setup() {
  
    Serial.begin(9600);
    pinMode(IRPin,INPUT);

}

void loop() {

      IRVal = digitalRead(IRPin);
      Serial.print("IR Val = ");
      Serial.println(IRVal);
      delay(1000);
    
}
