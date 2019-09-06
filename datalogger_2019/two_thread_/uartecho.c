/*
 * Copyright (c) 2015-2019, Texas Instruments Incorporated
 * All rights reserved.
 *  ======== uartecho.c ========
 */
//----- FOR SeaScan Time Base
/* For usleep() */

#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART.h>
/* Example/Board Header files */
#include "Board.h"
/* POSIX Header files */
#include <pthread.h>
/***** GLOBAL VARIABLES *****/
extern pthread_mutex_t hold;

UART_Handle uart;
int seascan_command;

#include <xdc/runtime/Types.h>
#include <xdc/runtime/Timestamp.h>

#define software_UART_tx_PIN Board_P3_7_SEASER
#define software_UART_rx_PIN Board_P3_7_SEASER

void baudrate_2400_delay();
void baudrate_half_delay();
void software_UART_write(unsigned char *buf,unsigned char len);
int  software_UART_read_char(unsigned char *buf);

double timestamp_GPS;
double timestamp_Seascan;

char RS232_Out[100];

int drift_ms = 0;
unsigned char Seascan_SN[]="#TB38 ";
int send_at = 0;


int GPS_checksum(char *rx_buf,int no);

//  Callback function for the GPIO interrupt on Board_GPIO_BUTTON0.
void gpioButtonFxn0(uint_least8_t index)
{
     timestamp_Seascan = Timestamp_get32();
     /* Clear the GPIO interrupt and toggle an LED */
    GPIO_toggle(Board_GPIO_LED0);
}
void gpioButtonFxn1(uint_least8_t index)
{
    timestamp_GPS =  Timestamp_get32();
    GPIO_toggle(Board_GPIO_LED1);
    if(seascan_command == 7){
        seascan_command = 0;
        send_at = 1;
    }
}

// *  ======== mainThread ========
void *mainThread(void *arg0)
{
    char        input;
    const char  echoPrompt[] = "Seascan Rx-Tx- :\r\n";

    UART_Params uartParams;
    /* Call driver init functions */
    GPIO_init();
    UART_init();

    /* Configure the LED and button pins */
     GPIO_setConfig(Board_GPIO_LED0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
     GPIO_setConfig(Board_GPIO_LED1, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

     GPIO_setConfig(Board_P5_0_GPSPPS, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_RISING);  //--
     GPIO_setConfig(Board_P3_6_SEAPPS, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING); //--

     /* install Button callback */
     GPIO_setCallback(Board_P3_6_SEAPPS, gpioButtonFxn0);
     GPIO_setCallback(Board_P5_0_GPSPPS, gpioButtonFxn1);



    /* Create a UART with data processing off. */
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.baudRate = 115200;

    uart = UART_open(Board_UART0, &uartParams);

    if (uart == NULL) {
        /* UART_open() failed */
        while (1);
    }

    UART_write(uart, echoPrompt, sizeof(echoPrompt));

    GPIO_enableInt(Board_P3_6_SEAPPS);
    GPIO_enableInt(Board_P5_0_GPSPPS);

    /* Loop forever echoing */
    while (1) {

       // pthread_mutex_lock(&hold);
        UART_read(uart, &input, 1);
       // pthread_mutex_unlock(&hold);
        if(input == '1')seascan_command = 1;
        if(input == '2')seascan_command = 2;
        if(input == '3')seascan_command = 3;
        if(input == '6')seascan_command = 6;
        if(input == '7')seascan_command = 7;
        if(input == '8')seascan_command = 8;
        if(input == '9')seascan_command = 9;

        if(input == '4')seascan_command = 4;

        if(input == '5'){

            sprintf(RS232_Out, "Timestamp1 is %f ms\n\r", ((double)(timestamp_Seascan - timestamp_GPS)/48000.0) );
            UART_write(uart, &RS232_Out, strlen(RS232_Out));
            drift_ms = (timestamp_Seascan - timestamp_GPS)/4800;




        }
        UART_write(uart, &input, 1);




    }
}


//------------------------------------------------------------------

void software_UART_write(unsigned char *buf,unsigned char len){

    GPIO_setConfig(software_UART_tx_PIN, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_HIGH ); // set output
    unsigned char n_bit,data;  //-- 2400,8,n,1
    while(len--){
        n_bit = 8;
        GPIO_write(software_UART_tx_PIN, 0); //- start
        baudrate_2400_delay();

        data = *buf++;

        while(n_bit--){
            GPIO_write(software_UART_tx_PIN, (data&0x01) );
            data>>=1;
            baudrate_2400_delay();
        }
        GPIO_write(software_UART_tx_PIN, 1); //- stop
        baudrate_2400_delay();

    }
}

int software_UART_read_char(unsigned char *buf){
    GPIO_setConfig(software_UART_rx_PIN, GPIO_CFG_IN_PU);  //- set input
    UInt32 timeout_Ticks,timeout;
    timeout = 48000000; //- timeout = 1s
    timeout_Ticks = Timestamp_get32()+timeout;
    unsigned char n_bit,data;  //-- 2400,8,n,1

    while(GPIO_read(software_UART_rx_PIN)){
            if(Timestamp_get32() > timeout_Ticks){
                return(0); //- timeout
            }
    }
    baudrate_half_delay();
    baudrate_2400_delay();
    n_bit = 8;
    data = 0;
     for(n_bit = 0;n_bit<8;n_bit++){
            data>>=1;
            if(GPIO_read(software_UART_rx_PIN)){
                 data = data| 0x80;
            }
            baudrate_2400_delay();
    }
    *buf = (data) ;
            return(1);
}
//------------------------------------------------------------------

void *softwareUART_Thread(void *arg0)
{
    /* Call driver init functions */
    GPIO_init();
    // I2C_init();    // SPI_init();    // UART_init();    // Watchdog_init();
    GPIO_setConfig(software_UART_tx_PIN, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_HIGH );
    GPIO_setConfig(Board_GPIO_BUTTON0, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);
    GPIO_setConfig(Board_GPIO_BUTTON1, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

    /* install Button callback */
    GPIO_setCallback(Board_GPIO_BUTTON0, gpioButtonFxn0);
    GPIO_setCallback(Board_GPIO_BUTTON1, gpioButtonFxn1);

    /* Enable interrupts */
    GPIO_enableInt(Board_GPIO_BUTTON0);
    GPIO_enableInt(Board_GPIO_BUTTON1);

   unsigned char input_char;
   unsigned char seascan_command_0x0D = 0x0d;

   UART_Handle uart2;
   UART_Params uartParams2;
   UART_Params_init(&uartParams2);
   uartParams2.writeDataMode = UART_DATA_BINARY;
   uartParams2.readDataMode = UART_DATA_BINARY;
   uartParams2.readReturnMode = UART_RETURN_FULL;
   uartParams2.readEcho = UART_ECHO_OFF;
   uartParams2.baudRate = 9600;
   uartParams2.readTimeout = 10;

   uart2 = UART_open(Board_UART1, &uartParams2);//  Board_UART1 = P3.2 P3.3

   if (uart2 == NULL) {
       /* UART_open() failed */
       while (1);
   }

   seascan_command = 0;
   int y,nodata;

   char GPS_input;
   static char gps_str_buf[150];
   static uint8_t uart_rx_buf_counter2 = 0;
   char Gps_Date[6];    char Gps_Time[6];    char Gps_Staus[3];
   char GPS_Time_String[]  = "2000/01/01 00:00:00.000000";
   while (1) {
    //   pthread_mutex_lock(&hold);


       GPS_input = 0;
       if(UART_read(uart2, &GPS_input, 1)>0){
           gps_str_buf[uart_rx_buf_counter2] = GPS_input;            uart_rx_buf_counter2++;

           if(GPS_input == '\n'){
                       if(strncmp("$GPRMC",gps_str_buf,6)==0){

                           UART_write(uart, &gps_str_buf, uart_rx_buf_counter2);

                           if(GPS_checksum(gps_str_buf,uart_rx_buf_counter2)==1){
                                   sscanf(gps_str_buf,"$GPRMC,%[^,],%[^,],%*[^,],%*[^,],%*[^,],%*[^,],%*[^,],%*[^,],%[^,]",Gps_Time,Gps_Staus,Gps_Date);//--  %[^,] 可用來分割,內的字元
                               if(Gps_Staus[0]=='A'){
                                   Gps_Staus[0]='N';
                                       GPS_Time_String[2] = Gps_Date[4];       GPS_Time_String[3] = Gps_Date[5];       GPS_Time_String[5] = Gps_Date[2];       GPS_Time_String[6] = Gps_Date[3];
                                       GPS_Time_String[8] = Gps_Date[0];     GPS_Time_String[9] = Gps_Date[1];     GPS_Time_String[11] = Gps_Time[0];    GPS_Time_String[12] = Gps_Time[1];
                                       GPS_Time_String[14] = Gps_Time[2];   GPS_Time_String[15] = Gps_Time[3];    GPS_Time_String[17] = Gps_Time[4];   GPS_Time_String[18] = Gps_Time[5];

                               /*     hp_gps_time = ms_timestr2hptime(GPS_Time_String);

                                       if(check_GPS){
                                           gps_right_counter++;
                                       }
                                       if(gps_right_counter==10 ){
                                                       Logger_status = Logger_Start_GpsSync_LED_R_G;
                                                       check_GPS = 0;
                                                       gps_right_counter++;
                                       }
                               */
                               }
                             /*  else{
                                    gps_right_counter = 0;;
                               }
                             */
                           }
                       }
                       memset(gps_str_buf,0,uart_rx_buf_counter2+1);
                       uart_rx_buf_counter2 = 0;
                    }
                    else if(uart_rx_buf_counter2>149){
                            memset(gps_str_buf,0,uart_rx_buf_counter2+1);
                            uart_rx_buf_counter2 = 0;
                    }


       }
           if(seascan_command == 1){
               seascan_command=0;
               software_UART_write(Seascan_SN,6);
               for(y=0;y<5;y++){
               if(software_UART_read_char(&input_char) == 1){
                   if(input_char==':'){
                       UART_write(uart, &input_char, 1);
                       break;
                   }
               }
               }
           }
           if(seascan_command == 2){
               seascan_command=0;               nodata = 0;
               software_UART_write("H",1);
               software_UART_write(&seascan_command_0x0D,1);

               for(y=0;y<500;y++){
                   if(software_UART_read_char(&input_char) == 1){
                        UART_write(uart, &input_char, 1);
                   }
                   else{
                       UART_write(uart, "N", 1);
                       nodata++;
                       if(nodata>1)break;
                   }
               }
           }
           if(seascan_command == 3){ //-- 取得Seascan時間
               seascan_command=0;
               software_UART_write(Seascan_SN,6);
               for(y=0;y<6;y++){
                   if(software_UART_read_char(&input_char) == 1){
                        UART_write(uart, &input_char, 1);
                        //if(input_char==':')break;
                   }
                   else{
                       break;
                   }
               }

               software_UART_write("?T",2);     software_UART_write(&seascan_command_0x0D,1);

               for(y=0;y<100;y++){
                   if(software_UART_read_char(&input_char) == 1){
                        UART_write(uart, &input_char, 1);
                            if(input_char==0x0d)break;
                   }
                   else{
                       break;
                   }
               }
           }

           if(seascan_command == 6){
               //-- Send !U
                          seascan_command=0;               nodata = 0;
                          software_UART_write(Seascan_SN,6);
                                    for(y=0;y<6;y++){
                                        if(software_UART_read_char(&input_char) == 1){
                                             UART_write(uart, &input_char, 1);
                                           //  if(input_char==0x3a)break;
                                        }
                                    }
                                    software_UART_write("!U",2);
                                    software_UART_write(&seascan_command_0x0D,1);

                                    for(y=0;y<100;y++){
                                        if(software_UART_read_char(&input_char) == 1){
                                             UART_write(uart, &input_char, 1);
                                                // if(input_char==0x0d)break;

                                        }
                                        else{
                                            UART_write(uart, "N", 1);
                                                break;
                                        }
                                    }


                                   software_UART_write("!T 121:11:00:00 ",16);
                                   seascan_command = 7;

            }
           if(send_at == 1){
               send_at = 0;
               //-- Send 2400,7,E,1 @ ~
               unsigned char seascan_command_char = 0xC0;
               software_UART_write(&seascan_command_char,1);
            }

           if(seascan_command == 8){
                        //-- Send !U !A  修正時間
                 seascan_command=0;               nodata = 0;
                                   software_UART_write(Seascan_SN,6);
                                             for(y=0;y<6;y++){
                                                 if(software_UART_read_char(&input_char) == 1){
                                                      UART_write(uart, &input_char, 1);
                                                 }
                                             }
                                             software_UART_write("!U",2);
                                             software_UART_write(&seascan_command_0x0D,1);

                                             for(y=0;y<100;y++){
                                                 if(software_UART_read_char(&input_char) == 1){
                                                      UART_write(uart, &input_char, 1);
                                                 }
                                                 else{
                                                         break;

                                                 }
                                             }

                                             if(drift_ms>0){
                                                 sprintf(RS232_Out, "!A +00%04d", drift_ms);
                                             }
                                             else{
                                                 sprintf(RS232_Out, "!A -00%04d", drift_ms*-1);
                                             }

                                             //software_UART_write()
                                             UART_write(uart, &RS232_Out, strlen(RS232_Out));
                                             software_UART_write((unsigned char *)RS232_Out,10);
                                             software_UART_write(&seascan_command_0x0D,1);
            }

    //   pthread_mutex_unlock(&hold);

    }
}

void baudrate_2400_delay(){
    // -- https://e2e.ti.com/support/legacy_forums/embedded/tirtos/f/355/t/464329
    UInt32 sTicks, eTicks;
    sTicks = eTicks = Timestamp_get32();
    eTicks = eTicks + 19680;  //-- 410 * 48     //eTicks = eTicks + 4800;  //-- 100 * 48 --9600    //eTicks = eTicks + 190;  //-- 8 * 48 --115200
    while(sTicks < eTicks)sTicks = Timestamp_get32();
}

void baudrate_half_delay(){
    UInt32 sTicks, eTicks;
    sTicks = eTicks = Timestamp_get32();
    eTicks = eTicks + 19680/2;  //48Mhz-- 1us
    while(sTicks < eTicks)sTicks = Timestamp_get32();
}



//unsigned char ahex2bin (unsigned char MSB, unsigned char LSB)
char ahex2bin ( char MSB,  char LSB)
{
    if (MSB > '9') MSB += 9;   // Convert MSB value to a contiguous range (0x30..0x?F)
    if (LSB > '9') LSB += 9;   // Convert LSB value to a contiguous range (0x30..0x?F)
    return (MSB <<4) | (LSB & 0x0F);   // Make a result byte using only low nibbles of MSB and LSB
}
int GPS_checksum(char *rx_buf,int no){
    int ck = 0;  char checksum = 0;

    for(ck=1;ck<no-5;ck++){
        checksum ^= rx_buf[ck];
    }
    if(checksum == ahex2bin(rx_buf[no-4],rx_buf[no-3])){
        return(1);
    }
    else{
        return(0);
    }

}


