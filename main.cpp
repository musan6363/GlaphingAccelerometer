#include "ICM_20948.h"
#include "mbed.h"
#include "SerialStream.h"

void printScaledAGMT( ICM_20948_I2C *sensor, FILE** fpp );
BufferedSerial pc(USBTX, USBRX);
SerialStream<BufferedSerial> debug_ss(pc);

LocalFileSystem local("local");  // mbed自身からmbedのストレージにアクセスする．

// main() runs in its own thread in the OS
int main()
{
    DigitalInOut led1(LED1);  // led1にLED1(=P1_18)ピンを割り当てる．
    led1 = 1;
    
    /** Create an I2C Master interface, connected to the specified pins
     *
     *  @param sda I2C data line pin
     *  @param scl I2C clock line pin
     */
    I2C i2c(I2C_SDA1, I2C_SCL1);  // I2Cの通信を行うピンの指定．data line(p9)とclock line(p10)
    i2c.frequency(400*1000);  // I2Cインターフェイスの周波数を400kHzに設定

    ICM_20948_I2C imu;
    imu.enableDebugging(debug_ss);
    do{
        imu.begin(i2c);
        printf("%s\n", imu.statusString());
        if( imu.status != ICM_20948_Stat_Ok ){
            printf( "Trying again...\n" );
            wait_us(500*1000);
        }else{
            break;
        }
    }while(1);

    // Here we are doing a SW reset to make sure the device starts in a known state
    imu.swReset( );
    if( imu.status != ICM_20948_Stat_Ok){
        printf("Software Reset returned: %s\n", imu.statusString());
    }
    wait_us(250*1000);

      // Now wake the sensor up
    imu.sleep( false );
    imu.lowPower( false );

    imu.setSampleMode( (ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), ICM_20948_Sample_Mode_Continuous );
    if( imu.status != ICM_20948_Stat_Ok){
        printf("setSampleMode returned: %s\n", imu.statusString());
    }

    // Set full scale ranges for both acc and gyr
    ICM_20948_fss_t myFSS;  // This uses a "Full Scale Settings" structure that can contain values for all configurable sensors

    myFSS.a = gpm16;         // (ICM_20948_ACCEL_CONFIG_FS_SEL_e)
                            // gpm2
                            // gpm4
                            // gpm8
                            // gpm16

    myFSS.g = dps2000;       // (ICM_20948_GYRO_CONFIG_1_FS_SEL_e)
                            // dps250
                            // dps500
                            // dps1000
                            // dps2000

    imu.setFullScale( (ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), myFSS );
    if( imu.status != ICM_20948_Stat_Ok){
        printf("setFullScale returned: %s\n", imu.statusString());
    }

    // Set up Digital Low-Pass Filter configuration
    ICM_20948_dlpcfg_t myDLPcfg;            // Similar to FSS, this uses a configuration structure for the desired sensors
    myDLPcfg.a = acc_d473bw_n499bw;         // (ICM_20948_ACCEL_CONFIG_DLPCFG_e)
                                            // acc_d246bw_n265bw      - means 3db bandwidth is 246 hz and nyquist bandwidth is 265 hz
                                            // acc_d111bw4_n136bw
                                            // acc_d50bw4_n68bw8
                                            // acc_d23bw9_n34bw4
                                            // acc_d11bw5_n17bw
                                            // acc_d5bw7_n8bw3        - means 3 db bandwidth is 5.7 hz and nyquist bandwidth is 8.3 hz
                                            // acc_d473bw_n499bw

    myDLPcfg.g = gyr_d361bw4_n376bw5;       // (ICM_20948_GYRO_CONFIG_1_DLPCFG_e)
                                            // gyr_d196bw6_n229bw8
                                            // gyr_d151bw8_n187bw6
                                            // gyr_d119bw5_n154bw3
                                            // gyr_d51bw2_n73bw3
                                            // gyr_d23bw9_n35bw9
                                            // gyr_d11bw6_n17bw8
                                            // gyr_d5bw7_n8bw9
                                            // gyr_d361bw4_n376bw5

    imu.setDLPFcfg( (ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), myDLPcfg );
    if( imu.status != ICM_20948_Stat_Ok){
        printf("setDLPcfg returned: %s\n", imu.statusString());
    }

    // Choose whether or not to use DLPF
    // Here we're also showing another way to access the status values, and that it is OK to supply individual sensor masks to these functions
    ICM_20948_Status_e accDLPEnableStat = imu.enableDLPF( ICM_20948_Internal_Acc, false );
    ICM_20948_Status_e gyrDLPEnableStat = imu.enableDLPF( ICM_20948_Internal_Gyr, false );
    printf("Enable DLPF for Accelerometer returned: %s\n", imu.statusString(accDLPEnableStat));
    printf("Enable DLPF for Gyroscope returned: %s\n", imu.statusString(gyrDLPEnableStat));

    // Choose whether or not to start the magnetometer
    imu.startupMagnetometer();
    if( imu.status != ICM_20948_Stat_Ok){
        printf("startupMagnetometer returned: %s\n", imu.statusString());
    }

    FILE *fp;
    if ( NULL == (fp = fopen( "/local/output.csv", "w" )) ){
        error( "" );
    }
    fprintf( fp, "Acc (mg) , , , ,"\
                 "Gyr (DPS), , , ,"\
                 "Mag (uT) , , , ,"\
                 "Tmp (C)\n"
    );
    fprintf( fp, "accX, accY, accZ,  ,"\
    "gyrX, gyrY, gyrZ,  ,"\
    "magX, magY, magZ,  ,"\
    "temp\n"
    );

    Timer t;
    t.start();
    int s = 0,e = 0;
    for (int i = 0; i < 500; i++){
        if( imu.dataReady() ){
            // imu.getAGMT(true);  // 1350 ns interval
            imu.getAGMT(false);  // 786 ns interval
            // imu.getAG(false); // 526 ns interval
            // なんか、色々ロードしてる, rangeとか
            e = t.elapsed_time().count();  //  for one sampling ,  to cut loading settings
            printf("%d ::: %d ns\n", i, e-s);
            s = t.elapsed_time().count();
            // printRawAGMT( myICM.agmt );     // Uncomment this to see the raw values, taken directly from the agmt structure
            printScaledAGMT( &imu, &fp );   // This function takes into account the scale settings from when the measurement was made to calculate the values with units
        }else{
           printf("Waiting for data\n");
           wait_us(500*1000);
        }
    }
    fclose(fp);
}

void printScaledAGMT( ICM_20948_I2C *sensor, FILE** fpp ){
    printf("Scaled. Acc (mg) [ %.2f, %.2f, %.2f ], "\
    "Gyr (DPS) [ %.2f, %.2f, %.2f ], "\
    "Mag (uT) [ %.2f, %.2f, %.2f  ], "\
    "Tmp (C) [ %.2f ]\n",
    sensor->accX(), sensor->accY(), sensor->accZ(),
    sensor->gyrX(), sensor->gyrY(), sensor->gyrZ(), 
    sensor->magX(), sensor->magY(), sensor->magZ(), 
    sensor->temp()
    );
    fprintf( *fpp, "%.2f, %.2f, %.2f,  ,"\
    "%.2f, %.2f, %.2f,  ,"\
    "%.2f, %.2f, %.2f,  ,"\
    "%.2f\n",
    sensor->accX(), sensor->accY(), sensor->accZ(),
    sensor->gyrX(), sensor->gyrY(), sensor->gyrZ(), 
    sensor->magX(), sensor->magY(), sensor->magZ(), 
    sensor->temp()
    );
}
