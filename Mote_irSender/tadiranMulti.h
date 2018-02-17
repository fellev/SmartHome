// **********************************************************************************************************
// Author: Felix Laevsky, Lina Laevsky Faingold
// IR sender for TadiranMulti Air Conditioner
// Allow to control Air Conditioner different modes and power
// **********************************************************************************************************
// Air Conditioner modes are received from mote_gateway and IR codes sent to the Air Conditioner
//

#ifndef TADIRAN_MULTI_H_
#define TADIRAN_MULTI_H_
/*****************************************************************************
 * Includes
 * **************************************************************************/


/*****************************************************************************
 * Defines
 * **************************************************************************/


/*****************************************************************************
 * Macros
 * **************************************************************************/


/*****************************************************************************
 * Types
 * **************************************************************************/
// typedef struct tadiranMultiCodeSec1_t
// {
//     byte mode[3];
//     byte power;
//     byte fan_speed[2];
//     byte sweep;
//     byte nu1;
//     byte temperature[4];
//     byte nu2[8];
//     byte light;
//     byte power;
//     byte nu3[11];
// } tadiranMultiCodeSec1_ts;

typedef struct tadiranMultiCodeSec1_t
{
    byte mode           : 3;
    byte power1         : 1;
    byte fan_speed      : 2;
    byte sweep          : 1;
    byte nu1            : 1;
    byte temperature    : 4;
    unsigned int nu2    : 9;
    byte light          : 1;
    byte power2         : 1;
    unsigned int nu3    : 12;
} tadiranMultiCodeSec1_ts;

typedef struct tadiranMultiCodeSec2_t
{
    byte air_flow_angle : 4;
    unsigned long nu1   : 24;
    byte crc            : 4;
} tadiranMultiCodeSec2_ts;

typedef union tadiranMultiCode1_t
{
    tadiranMultiCodeSec1_ts code;
    unsigned long long      raw_data;
}tadiranMultiCode1_tu;

typedef union tadiranMultiCode2_t
{
    tadiranMultiCodeSec2_ts code;
    unsigned long           raw_data;
}tadiranMultiCode2_tu;



    
class tadiranMulti
{

    
    public:
        enum Power
        {
            OFF,
            ON
        };

        enum Mode
        {
            MODE_AUTO,
            MODE_COOL,
            MODE_DRY,
            MODE_FAN,
            MODE_HEAT
        };  

        enum FanAngle
        {
            ANGLE_AUTO          = 0,
            ANGLE_SWEEP         = 1,
            ANGLE_POS1          = 2,
            ANGLE_POS2          = 3,
            ANGLE_POS3          = 4,
            ANGLE_POS4          = 5,
            ANGLE_POS5          = 6,
            ANGLE_SWEEP_DOWN    = 7,
            ANGLE_SWEEP_CENTRAL = 9, 
            ANGLE_SWEEP_UP      = 11
        };        
        
        Power                  _power;        // On, Off
        unsigned char          _fan;          // 0-Auto, 1-3 Speed   
        unsigned char          _temperature;  // Temperature range 16-30 degrees
        Mode                   _mode;         // Select the mode
        FanAngle               _fan_angle;    // Fan angle
        unsigned char          _light;        // Led light
        unsigned char          _crc;          // CRC to send
        tadiranMulti(){
            _crc = 16;
        };
        tadiranMulti(Power         power,
                     unsigned int  fan,
                     unsigned int  temperature,
                     Mode          mode,
                     FanAngle      fan_angle,
                     unsigned char light
                   )        
                     : _power(power),
                       _fan(fan),
                       _temperature(temperature),
                       _mode(mode),
                       _fan_angle(fan_angle),
                       _light(light) {};
        
        void send(int crc);
        void startToSend();
        void worker();       
};


/*****************************************************************************
 * Exported Global Variables
 * **************************************************************************/

/*****************************************************************************
 * Methods prototypes
 * **************************************************************************/

#endif

