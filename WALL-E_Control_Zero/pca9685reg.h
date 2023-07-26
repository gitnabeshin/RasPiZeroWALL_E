//
//  pca9685reg.h
//  I2C_PCA9685_Servo_Control
//
//  Created by 渡辺 紳一郎 on 2017/06/22.
//  Copyright © 2017年 渡辺 紳一郎. All rights reserved.
//

/**
 http://wiringpi.com/reference/i2c-library/
 -------------------------------------------------------------------------
 WiringPi includes a library which can make it easier to use the Raspberry Pi’s on-board I2C interface.
 
 Before you can use the I2C interface, you may need to use the gpio utility to load the I2C drivers into the kernel:
 
 > gpio load i2c
 
 If you need a baud rate other than the default 100Kbps, then you can supply this on the command-line:
 
 > gpio load i2c 1000
 
 will set the baud rate to 1000Kbps – ie. 1,000,000 bps. (K here is times 1000)
 To use the I2C library, you need to:
 
 > #include <wiringPiI2C.h>
 
 in your program. Programs need to be linked with -lwiringPi as usual.
 You can still use the standard system commands to check the I2C devices, and I recommend you do so
 – e.g. the i2cdetect program. Just remember that on a Rev 1 Raspberry pi it’s device 0,
 and on a Rev. 2 it’s device 1. e.g.
 
 > i2cdetect -y 0 # Rev 1
 > i2cdetect -y 1 # Rev 2
 
 Note that you can use the gpio command to run the i2cdetect command for you with the correct
 parameters for your board revision:
 
 > gpio i2cdetect
 
 ------------------------------------------------------
 > sudo raspi-config
   --> Interfacing Options --> I2C enable
 
 pi@raspberrypi:~/work/i2c_servo $ sudo i2cdetect -y 1
 0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
 00:          -- -- -- -- -- -- -- -- -- -- -- -- --
 10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 40: 40 -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 70: 70 -- -- -- -- -- -- --
 pi@raspberrypi:~/work/i2c_servo $
 
 ------------------------------------------------------
 
 is all that’s needed.
 -------------------------------------------------------------------------
 */


#ifndef pca9685reg_h

// PCA9685 Registers

#define MODE1 0x00
#define MODE2 0x01
#define SUBADR1 0x02
#define SUBADR2 0x03
#define SUBADR3 0x04
#define ALLCALLADR 0x05
#define LED0_ON_L 0x06
#define LED0_ON_H 0x07
#define LED0_OFF_L 0x08
#define LED0_OFF_H 0x09
// LED1 .. LED15 (0x0A .. 0x45)
#define ALL_LED_ON_L 0xFA
#define ALL_LED_ON_H 0xFB
#define ALL_LED_OFF_L 0xFC
#define ALL_LED_OFF_H 0xFD
#define PRE_SCALE 0xFE
#define TESTMODE 0xFF

// Bits in the MODE1 register

#define MODE1_RESTART 0x80
#define MODE1_EXTCLK 0x40
#define MODE1_AI 0x20
#define MODE1_SLEEP 0x10
#define MODE1_SUB1 0x08
#define MODE1_SUB2 0x04
#define MODE1_SUB3 0x02
#define MODE1_ALLCALL 0x01

// Bits in the MODE2 register

#define MODE2_INVRT 0x10
#define MODE2_OCH 0x08
#define MODE2_OUTDRV 0x04
#define MODE2_OUTNE_1 0x02
#define MODE2_OUTNE_0 0x01

//内蔵クロック周波数 25MHz
#define OSC_CLOCK 25000000.0f
//周波数分解能(0-4096)
#define PRESCALE_DIVIDER 4096.0f


#define pca9685reg_h

#endif /* pca9685reg_h */
