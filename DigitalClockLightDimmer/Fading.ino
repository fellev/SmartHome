#include <TimerOne.h>
#include <movingAvg.h>                  // https://github.com/JChristensen/movingAvg

int ledpin = TIMER1_A_PIN; //6; //LED_BUILTIN;
int ldrpin = 0;
int value1 = 0;
int value2 = 0;
int value3 = 0;
int avg = 0;
//int read_delay=1000;
//int loop_cnt = 1000;

#define SERIAL_BAUD          115200

#define D_MAX_INTENSITY      10000
#define D_THRESHOLD_PERCENT  10
#define D_LED_POWER_OUTPUT_MIN 1
#define D_LED_POWER_OUTPUT_MAX 10000

#define M_LOW_THRESHOLD(val)      (val * 100 / (100 + D_THRESHOLD_PERCENT))
#define M_MAX_THRESHOLD(val)      (val * 100 / (100 - D_THRESHOLD_PERCENT))

//RunningAverage myRA(50);
movingAvg avgL (50);                  // define the moving average object

typedef struct TAG_S_INTENSITY_RANGE
{
  int min;
  int max;
  int val;
} S_INTENSITY_RANGE, *S_INTENSITY_RANGE_PTR;

S_INTENSITY_RANGE g_intensity_rages[] =
  {
    { 0, 1, 1 },
    { 2, D_MAX_INTENSITY / 4,
    D_MAX_INTENSITY / 5 },
    { D_MAX_INTENSITY / 4, D_MAX_INTENSITY / 3,
    D_MAX_INTENSITY / 4 },
    { D_MAX_INTENSITY / 3, D_MAX_INTENSITY / 2,
    D_MAX_INTENSITY / 3 },
    { D_MAX_INTENSITY / 2, D_MAX_INTENSITY / 1,
    D_MAX_INTENSITY } };

typedef void (*iFunction) (void);

iFunction f_intensity_control;

void f_analog_intensity (void)
{
//	if (loop_cnt == 0)
//	{
  avg = avgL.reading (value1);
  //    avg = 35;
  value2 = constrain(avg, 0, 70);
  value3 = map (value2, 0, 70, D_LED_POWER_OUTPUT_MIN, D_LED_POWER_OUTPUT_MAX);
  analogWrite (ledpin, value3);
  delay (100);
//		read_delay = 100;
  //	Serial.println(String("PWM"));
//	}
}

void f_digital_intensity (void)
{
  digitalWrite (ledpin, 1);
//	delayMicroseconds(2);
  digitalWrite (ledpin, 0);
  delay (10);
//	read_delay = 1000;
//	avgL.reset();
}

void setup ()
{
  Serial.begin (SERIAL_BAUD);
  Serial.println (String ("Start"));
  Timer1.initialize (10000);  // 40 ms = 100 Hz
  avgL.begin ();
  f_intensity_control = f_digital_intensity;
}

void loop ()
{
  value1 = analogRead (ldrpin);
  f_analog_intensity ();
//	if ( loop_cnt++ > read_delay )
//	{
//		value1 = analogRead(ldrpin);
//		loop_cnt = 0;
//	}

//    for (i = 0; i < (sizeof(g_intensity_rages) / sizeof(S_INTENSITY_RANGE)) ; i++ )
//    {
//         if ((g_intensity_rages[i].min < value3) && (g_intensity_rages[i].max >= value3))
//         {
//              value3 = g_intensity_rages[i].val;
//         }
//    }

//	if (value1 < 2)
//	{
//		f_intensity_control = f_digital_intensity;
//	}
//	else
//	{
//		f_intensity_control = f_analog_intensity;
//	}

//	f_intensity_control();
//	digitalWrite(ledpin, 1);
//	digitalWrite(ledpin, 0);
//	delay(10);
//	else
//	{
////	    avg = avgL.reading(value1);
//	//    avg = 35;
//	    value2 = constrain(avg,0,70);
//	    value3 = map(value2,0,70,D_LED_POWER_OUTPUT_MIN,D_LED_POWER_OUTPUT_MAX);
//		analogWrite(ledpin, value3);
//		delay(100);
//		Serial.println(String("PWM"));
//	}

//	Serial.print(value1);
//	Serial.print(',');
//	Serial.print(avg);
//	Serial.print(',');
//	Serial.print(value2);
//	Serial.print(',');
//	Serial.print(value3);
//	Serial.println(',');

}
