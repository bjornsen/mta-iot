/* Things to try:
 * 
 * - make the LED blink faster and slower
 * - vary the brightness using analogWrite() (01-pwmLED)
 *
 */

const int LEDPIN = 27;

void setup()
{
	pinMode(LEDPIN, OUTPUT);
}

void loop()
{
	digitalWrite(LEDPIN, HIGH);
	delay(2000);
	digitalWrite(LEDPIN, LOW);
	delay(2000);
}
