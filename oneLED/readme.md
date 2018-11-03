# Blink an LED

![fritzing diagram](oneLED_fritzing.png)

![schematic](oneLED_schematic.png)

```cpp
const int LEDPIN = 27;

void setup()
{
    pinMode(LEDPIN, OUTPUT);
}

void loop()
{
    digitalWrite(LEDPIN, HIGH);
    delay(500);
    digitalWrite(LEDPIN, LOW);
    delay(500);
}
```
