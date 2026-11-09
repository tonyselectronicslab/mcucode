// two stage booster 5v-15v 15v-40v 

const int pwm1=11,pwm2=12;  // pwm pin 11 stage 1, pin 12 stage 2 
const int voltage1=A0,voltage2=A1;

int ADC1avr=0,ADCcount1=0,ADC2avr=0,ADCcount2=0;

void setup()
{
  pinMode(pwm1,OUTPUT); // pwm signal stage 1
  pinMode(voltage1,INPUT); // voltage output stage 1

  pinMode(pwm2,OUTPUT);
  pinMode(voltage2,INPUT);

  // Configure TCCR1A:
  // COM1A1 = 1, COM1B1 = 1 (Non-inverting PWM on Pins 11 & 12)
  // WGM10 = 1 (8-bit Phase-Correct PWM mode, ~31.37 kHz)
  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM10);

  // Configure TCCR1B:
  // CS10 = 1 (Prescaler = 1, no clock division)
  TCCR1B = _BV(CS10);
  
  OCR1A = (int)180; // PWM duty cycle 1 10 to 200 typically pin 11
  OCR1B = (int)55; // pin 12

  Serial.begin(9600);
}

void loop()
{
  int rawADC = analogRead(voltage1);
  float stage1voltage=(rawADC/41.6);

  if(ADCcount1<10)
  { 
    ADC1avr+=rawADC;
    ADCcount1++;
  }
  else
  {
    ADC1avr=0; ADCcount1=0;
  }

  Serial.print("Voltage out stage 1 ADC ");
  Serial.print(rawADC);  
  Serial.print(" V out=");
  Serial.print(stage1voltage);
  if(ADCcount1==10)
  {
    Serial.print(" AVR1 = ");
    Serial.print(ADC1avr/10);
  }
  Serial.println("");

  delay(100);
  
}