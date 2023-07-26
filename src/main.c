//
//  main.c
//  Walle_Control_Zero
//
//
// gcc main.c -o WALLE_Control_Zero.out -lwiringPi -lm

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <unistd.h> // for close

#include "wiringPi.h"
#include "wiringPiI2C.h"
#include "pca9685reg.h"

#define I2C_ADDRESS_1st (0x40)     // PCA9685 1st board addrss(0x41 for 2nd)
#define PWM_PULSE_WIDTH_MIN (0)    // PCA9685 内部カウンタ値(0-4095)
#define PWM_PULSE_WIDTH_MAX (4095) // PCA9685 内部カウンタ値(0-4095)

#define SV0_FORWARD (1)  // Rotate FORWARD with Servo0
#define SV1_FORWARD (-1) // Rotate FORWARD with Servo1

#define SV0_BACKWARD (-1) // Rotate BACKWARD with Servo0
#define SV1_BACKWARD (1)  // Rotate BACKWARD with Servo1

#define SV6_LOOK_UP (420)        // LOOK UP
#define SV6_LOOK_LITTLE_UP (350) // LOOK LITTLE UP
#define SV6_LOOK_AHEAD (300)     // LOOK AHEAD
#define SV6_LOOK_DOWN (270)      // LOOK DOWN

#define SV7_NECK_LEFT (180)      // HEAD TO LEFT LIMIT
#define SV7_NECK_RIGHT (450)     // HEAD TO RIGHT LIMIT
#define SV7_NECK_FRONT (380)     // HEAD TO FRONT
#define SV8_LEFT_ARM_DOWN (180)  // LEFT ARM DOWN LIMIT
#define SV8_LEFT_ARM_UP (500)    // LEFT ARM UP LIMIT
#define SV9_RIGHT_ARM_DOWN (500) // RIGHT ARM DOWN LIMIT
#define SV9_RIGHT_ARM_UP (180)   // RIGHT ARM UP LIMIT

#define EYES_LED_GPIO (23) // GPIO23 (PIN 16)

#define WAV_STARTUP "aplay ../../wav/Startup1.wav&"
#define WAV_WAKEUP "aplay ../../wav/wakeup.wav&"
#define WAV_WALLE "aplay ../../wav/walle.wav"
#define WAV_MUSIC "aplay ../../wav/Dirty_Work.wav&"
#define WAV_MUSIC2 "aplay ../../wav/Mark_Ronson.wav&"
#define WAV_MUSIC3 "aplay ../../wav/AC_DC_Thunderstruck.wav&"
#define WAV_MOVE "aplay ../../wav/move.wav&"
#define WAV_OHHH "aplay ../../wav/Ohhh.wav"
#define WAV_IBAA "aplay ../../wav/Ibaa.wav"
#define RADIO_AFN "sh ../../wav/PlayAFN360.sh"

#define WAV_SETUP1 "gpio -g mode 18 ALT5"
#define WAV_SETUP2 "gpio -g mode 13 ALT0"

/***************************************
 * GWS S35 STD ローテーションサーボ
 *
 * 周波数          50Hz (20ms/clock)
 * PWM            1.0ms <= 1.5ms <= 2.0ms
 * 4096カウンタ換算   203 <=   304 <= 407
 *     --> 実際はずれてる 1.5ms = 320
 ***************************************/
#define PWM_SERVO_FREQUENCY (50) // サーボのPWM周波数 = 50Hz

/**
 * 現在のSV6 PWM
 */
int m_currentPWM_SV6 = SV6_LOOK_AHEAD - 10;

/********************************************************************
 * pca9685 PWMWrite Function for wiringPi
 *
 * fd:         wiringPiI2CSetupでSetup済のpca9685のfd
 * channnel:   [0-15]
 * pwm_count:  ZEROスタートのPWM ONが終了する12bitのカウンタ値(0-4095)
 *             320で完全停止する (= PWM1.5ms)
 ********************************************************************/
static void outputPWM(const int fd, int channel, int pwm_count)
{

    // LED*_ON_Lのレジスタ値
    int chan = 4 * channel;
    // PWMカウンタ値
    int counter = pwm_count;

    // 範囲チェックと正規化
    if (PWM_PULSE_WIDTH_MAX < pwm_count)
    {
        counter = PWM_PULSE_WIDTH_MAX;
    }
    else if (PWM_PULSE_WIDTH_MIN > pwm_count)
    {
        counter = PWM_PULSE_WIDTH_MIN;
    }

    // LED*_ON, LED*_OFFのそれぞれ12bitレジスタに0-4095の値をセット
    // レジスタは上位4bit(LED*_ON_H), 下位8bit(LED*_ON_L)で構成されるので分離して指定

    // delay無しでカウンタ0からPWM ON
    wiringPiI2CWriteReg8(fd, LED0_ON_L + chan, 0x00);
    wiringPiI2CWriteReg8(fd, LED0_ON_H + chan, 0x00);

    // PWM OFF の開始カウンタ値を設定
    // 末尾8bitをレジスタに設定
    wiringPiI2CWriteReg8(fd, LED0_OFF_L + chan, counter & 0xFF);
    // 上位4bitをレジスタに設定
    wiringPiI2CWriteReg8(fd, LED0_OFF_H + chan, (counter >> 8) & 0xFF);
}

/********************************************************************
 * 一定速度で角度を変更する
 *
 *
 ********************************************************************/
void moveSlow(const int fd, int channel, int pwm_count)
{

    // 位置が同じならリターン
    if (pwm_count - m_currentPWM_SV6 == 0)
    {
        return;
    }

    // 回転方向判定
    int direction = 0;
    if ((pwm_count - m_currentPWM_SV6) > 0)
    {
        direction = 1;
    }
    else
    {
        direction = -1;
    }

    int outPWM = m_currentPWM_SV6;

    while (outPWM <= SV6_LOOK_UP && outPWM >= SV6_LOOK_DOWN)
    {

        if ((pwm_count == SV6_LOOK_AHEAD && outPWM == SV6_LOOK_AHEAD) ||
            (pwm_count == SV6_LOOK_LITTLE_UP && outPWM == SV6_LOOK_LITTLE_UP))
        {
            return;
        }

        m_currentPWM_SV6 = outPWM;
        outputPWM(fd, channel, outPWM);
        // printf("\n pwm[%d]", outPWM);
        // 100msec待つ
        delay(50);
        outPWM = m_currentPWM_SV6 + (5 * direction);
    }

    return;
}

/********************************************************************
 * pca9685 PWM ALL OFF
 *
 * fd:         wiringPiI2CSetupでSetup済のpca9685のfd
 ********************************************************************/
static void stopAllPWM(const int fd)
{

    printf("PWN ALL OFF.\n");

    // 0000 0000 をセット
    wiringPiI2CWriteReg8(fd, ALL_LED_OFF_L, 0x00);
    // 0001 0000 をセット 5bit目が ALL_LED_FULL_OFF=1
    wiringPiI2CWriteReg8(fd, ALL_LED_OFF_H, 0x10);
}

void eye_lights_on()
{
    digitalWrite(EYES_LED_GPIO, HIGH); // ON
}

void eye_lights_off()
{
    digitalWrite(EYES_LED_GPIO, LOW); // OFF
}

/**
 * setup Wiring PI
 */
void setup_GPIO(void)
{
    // Set up Wiring Pi
    wiringPiSetupGpio();
    // Set GPIO as Output
    pinMode(EYES_LED_GPIO, OUTPUT);
}

/**
 * Initialize sound output for Raspberry Pi Zero
 */
void setup_Sound_for_Zero(void)
{
    system(WAV_SETUP1);
    system(WAV_SETUP2);
}

/**********************************************************************
 * Setup a single pca9685 device at the specified i2c address and PWM frequency.
 *
 * i2cAddress:	The default address is 0x40
 * freq:		Frequency will be capped to range [40..1000] Hz. Set 50Hz for servo.
 *
 * return  fd:  Initialized pca9685 hundle
 **********************************************************************/
int pca9685Setup(const int i2cAddress, int freq)
{

    int fd = 0;
    int mode1 = 0;
    int prescale = 0;

    // I2C system initialize
    if ((fd = wiringPiI2CSetup(i2cAddress)) < 0)
    {
        printf("wiringPiI2CSetup Failed.\n");
        return -1;
    }

    // Reset Device
    wiringPiI2CWriteReg8(fd, MODE1, 0x00);

    /* -----------------------------------------------------------------
     [PRESCALE]レジスタ(8bit)へのPWM周波数指定
     - モード変更には、Mode1のSLEEP=1:「発振回路OFF」にした状態で行う必要があるため、これを先に行う。
     - 次にPRESCALEレジスタの値に、所望のPWMを得るためのpreacale値を計算式に従い求めた値で指定する。
     - 最後にMode1を元の値に戻し、リスタートする。
     - SLEEP=0(Normal)に戻すと、再起動するのにMAX 500usec待たなければならない。
     -----------------------------------------------------------------
     */

    // Mode1 レジスタ(8bit)の値を取得
    mode1 = wiringPiI2CReadReg8(fd, MODE1);
    // [Mode1]のレジスタ(8bit)への書き込み
    //   - [RESTART] bit (8)の無効化: mode1 & 0x7F(0111 1111)
    //   - [SLEEP] bit(5)の有効化 0x10(0001 0000)
    wiringPiI2CWriteReg8(fd, MODE1, (mode1 & 0x7F) | MODE1_SLEEP);

    /* -------------------------------------------------------------------
     出力するPWM出力の設定 preScaleの計算

     4096階調のカウンタ、内蔵クロック20MHzの発振回路を持つボードに対し、
     freq(Hz)のPWM信号を取り出したい場合、
     ICに指定するprescaleは以下の式で求めれば良い from 仕様書。
     -------------------------------------------------------------------
     prescale = (25MHz/4096*freq)の整数値部分 -1
     -------------------------------------------------------------------
     */
    prescale = (int)round(OSC_CLOCK / (PRESCALE_DIVIDER * freq)) - 1;
    wiringPiI2CWriteReg8(fd, PRE_SCALE, prescale);

    // Mode1 レジスタを元の値に戻す
    wiringPiI2CWriteReg8(fd, MODE1, mode1);
    // 500usec以上待つ (今回は5msec)
    delay(5);
    // Mode1[RESTART]レジスタにRESTART(=1)を書き込み
    wiringPiI2CWriteReg8(fd, MODE1, mode1 | MODE1_RESTART);

    return fd;
}

/**
 * Exec Command
 *
 * @param command:     command number
 * @param fd_pca9685: i2c device hundle
 */
void exec_command(int command, int fd_pca9685)
{

    int pwmDiff = 100;
    // LED ON/OFFコマンドかどうか
    int isLightCommand = 0;
    int i = 0;

    eye_lights_on();

    switch (command)
    {
    case 1: // 前に進む

        system(WAV_MOVE);
        // pca9685のチャンネル[0]にPWM[(0-4095)]を指定する。
        outputPWM(fd_pca9685, 0, 320 + (pwmDiff * SV0_FORWARD));
        // pca9685のチャンネル[1]にPWM[(0-4095)]を指定する。
        outputPWM(fd_pca9685, 1, 320 + (pwmDiff * SV1_FORWARD));

        delayMicroseconds(1523500 * 2);
        stopAllPWM(fd_pca9685);
        break;
    case 2: // 後ろに進む

        system(WAV_MOVE);
        outputPWM(fd_pca9685, 0, 320 + (pwmDiff * SV0_BACKWARD));
        outputPWM(fd_pca9685, 1, 320 + (pwmDiff * SV1_BACKWARD));
        delayMicroseconds(1523500 * 2);
        stopAllPWM(fd_pca9685);
        break;
    case 3: // 右に曲がる
        outputPWM(fd_pca9685, 0, 320 + (pwmDiff * SV0_FORWARD));
        outputPWM(fd_pca9685, 1, 320 + (pwmDiff * SV1_BACKWARD));
        delayMicroseconds(1523500);
        stopAllPWM(fd_pca9685);
        break;
    case 4: // 左に曲がる
        outputPWM(fd_pca9685, 0, 320 + (pwmDiff * SV0_BACKWARD));
        outputPWM(fd_pca9685, 1, 320 + (pwmDiff * SV1_FORWARD));
        delayMicroseconds(1523500);
        stopAllPWM(fd_pca9685);
        break;
    case 5: // 後ろを向く
        outputPWM(fd_pca9685, 0, 320 + (pwmDiff * SV0_FORWARD));
        outputPWM(fd_pca9685, 1, 320 + (pwmDiff * SV1_BACKWARD));
        delayMicroseconds(3800500);
        stopAllPWM(fd_pca9685);
        break;
    case 6: // Ohhh
        system(WAV_OHHH);
        break;
    case 7: // WALL-E
        system(WAV_WALLE);
        break;
    case 8: // dance
        outputPWM(fd_pca9685, 0, 320 + (pwmDiff * SV0_BACKWARD));
        outputPWM(fd_pca9685, 1, 320 + (pwmDiff * SV1_FORWARD));
        delayMicroseconds(923500);
        outputPWM(fd_pca9685, 0, 320 + (pwmDiff * SV0_FORWARD));
        outputPWM(fd_pca9685, 1, 320 + (pwmDiff * SV1_BACKWARD));
        delayMicroseconds(923500);
        outputPWM(fd_pca9685, 0, 320 + (pwmDiff * SV0_BACKWARD));
        outputPWM(fd_pca9685, 1, 320 + (pwmDiff * SV1_FORWARD));
        delayMicroseconds(5023500);
        stopAllPWM(fd_pca9685);
        break;
    case 10: // 両手を挙げる
        // outputPWM(fd_pca9685, 8, 500);
        outputPWM(fd_pca9685, 8, SV8_LEFT_ARM_UP);
        // outputPWM(fd_pca9685, 9, 180);
        outputPWM(fd_pca9685, 9, SV9_RIGHT_ARM_UP);
        break;
    case 11: // 両手を下げる
        outputPWM(fd_pca9685, 8, SV8_LEFT_ARM_DOWN);
        outputPWM(fd_pca9685, 9, SV9_RIGHT_ARM_DOWN);
        break;
    case 12: // 右手を挙げる
        outputPWM(fd_pca9685, 8, SV8_LEFT_ARM_UP);
        break;
    case 13: // 右手を下げる
        outputPWM(fd_pca9685, 8, SV8_LEFT_ARM_DOWN);
        break;
    case 14: // 左手を挙げる
        outputPWM(fd_pca9685, 9, SV9_RIGHT_ARM_UP);
        break;
    case 15: // 左手を下げる
        outputPWM(fd_pca9685, 9, SV9_RIGHT_ARM_DOWN);
        break;
    case 20: // 右を見る
        outputPWM(fd_pca9685, 7, SV7_NECK_LEFT);
        break;
    case 21: // 左を見る
        outputPWM(fd_pca9685, 7, SV7_NECK_RIGHT);
        break;
    case 22: // 前を見る
        outputPWM(fd_pca9685, 7, SV7_NECK_FRONT);
        break;
    case 23: // 上を見る
        moveSlow(fd_pca9685, 6, SV6_LOOK_UP);
        isLightCommand = 1;
        break;
    case 24: // 下を見る
        system(WAV_OHHH);
        moveSlow(fd_pca9685, 6, SV6_LOOK_DOWN);
        isLightCommand = 1;
        break;
    case 25: // 正面を見る
        moveSlow(fd_pca9685, 6, SV6_LOOK_AHEAD);
        for (i = 0; i < 5; i++)
        {
            eye_lights_on();
            delayMicroseconds(500000);
            eye_lights_off();
            delayMicroseconds(500000);
        }
        eye_lights_on();
        break;
    case 26: // YES
        moveSlow(fd_pca9685, 6, SV6_LOOK_LITTLE_UP);
        moveSlow(fd_pca9685, 6, SV6_LOOK_DOWN);
        moveSlow(fd_pca9685, 6, SV6_LOOK_AHEAD);
        isLightCommand = 1;
        break;
    case 27: // NO
        outputPWM(fd_pca9685, 7, SV7_NECK_LEFT);
        delay(800);
        outputPWM(fd_pca9685, 7, SV7_NECK_RIGHT);
        delay(800);
        outputPWM(fd_pca9685, 7, SV7_NECK_LEFT);
        delay(800);
        outputPWM(fd_pca9685, 7, SV7_NECK_FRONT);
        isLightCommand = 1;
        break;
    case 30: // 目のライトON
        system(WAV_IBAA);
        for (i = 0; i < 3; i++)
        {
            eye_lights_on();
            delayMicroseconds(500000);
            eye_lights_off();
            delayMicroseconds(500000);
        }
        eye_lights_on();
        isLightCommand = 1;
        break;
    case 31: // 目のライトOFF
        for (i = 0; i < 3; i++)
        {
            eye_lights_on();
            delayMicroseconds(500000);
            eye_lights_off();
            delayMicroseconds(500000);
        }
        eye_lights_off();
        isLightCommand = 1;
        break;
    case 50: // ミュージック
        eye_lights_on();
        system(WAV_MUSIC);
        break;
    case 51: // ミュージック
        eye_lights_on();
        system(WAV_MUSIC2);
        break;
    case 52: // ミュージック
        eye_lights_on();
        system(WAV_MUSIC3);
        break;
    case 60: // AFN RADIO
        eye_lights_on();
        system(RADIO_AFN);
        break;
    default:
        printf("NO COMMAND....\n");
        break;
    }

    // LED ON/OFF制御コマンド以外はここでLED OFF
    if (!isLightCommand)
    {
        eye_lights_off();
    }
}

/**********************************************************************
 * main
 **********************************************************************/
int main(int argc, char *argv[])
{

    // GPIO node of wiringPi
    int fd_pca9685 = 0;

    // Initialize sound output for Raspberry Pi Zero
    printf("Initialize Sound Output.\n");
    setup_Sound_for_Zero();

    // LED eye light off
    printf("Initialize LED.\n");
    setup_GPIO();

    printf("Initialize PCA9685.\n");
    // setup 1st pca9685 board
    fd_pca9685 = pca9685Setup(I2C_ADDRESS_1st, PWM_SERVO_FREQUENCY);

    // hands up
    exec_command(10, fd_pca9685);
    moveSlow(fd_pca9685, 6, SV6_LOOK_AHEAD);
    system(WAV_STARTUP);
    int i = 0;
    for (i = 0; i < 2; i++)
    {
        eye_lights_on();
        delayMicroseconds(300000);
        //        eye_lights_off();
        //        delayMicroseconds(600000);
    }
    exec_command(25, fd_pca9685);
    eye_lights_on();
    // hands down
    exec_command(11, fd_pca9685);

    char *menuStr = "ウォーリー　そうさメニュー \n"
                    " 1: 前に進む\n"
                    " 2: 後ろに進む\n"
                    " 3: 右に曲がる\n"
                    " 4: 左に曲がる\n"
                    " 5: 後ろを向く\n"
                    " 6: Ohhh\n"
                    " 7: WALL-E\n"
                    " 8: ダンス\n"
                    "10: 両手を挙げる\n"
                    "11: 両手を下げる\n"
                    "12: 右手を挙げる\n"
                    "13: 右手を下げる\n"
                    "14: 左手を挙げる\n"
                    "15: 左手を下げる\n"
                    "20: 右を見る\n"
                    "21: 左を見る\n"
                    "22: 前を見る\n"
                    "23: 上を見る\n"
                    "24: 下を見る\n"
                    "25: 正面を見る\n"
                    "26: YES\n"
                    "27: NO\n"
                    "30: 目のライトON\n"
                    "31: 目のライトOFF\n"
                    "50: 音楽を流す 1 (Dirty_Work)\n"
                    "51: 音楽を流す 2 (Mark_Manson)\n"
                    "52: 音楽を流す 3 (AC_DC)\n"
                    "60: ラジオを流す AFN\n"
                    " 0: 終わり\n"
                    "番号を選んでください>";

    int command;

    while (1)
    {
        printf("%s", menuStr);
        scanf("%d", &command);

        if (0 == command)
        {
            break;
        }
        exec_command(command, fd_pca9685);
    }

    printf("bye.....\n");

    stopAllPWM(fd_pca9685);

    // 一応fdはクローズ
    close(fd_pca9685);

    return 0;
}
