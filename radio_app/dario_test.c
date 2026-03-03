#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "hw.h"
#include "radio.h"
#include "stm32l4xx_hal.h"
#include "stm32l4xx_hal_gpio.h"
#include "sx1280.h"

/*!
 * \brief Used to display firmware version UART flow
 */
#define FIRMWARE_VERSION    ( ( char* )"Firmware Version: 5709de2d" )

/*!const char *, ...
 * Select mode of operation for the Ping Ping application
 */
#define MODE_LORA
//#define MODE_GFSK
//#define MODE_FLRC

#define RF_BL_ADV_CHANNEL_38             			2426000000 // Hz
#define RF_BL_ADV_CHANNEL_0                     	2404000000 // Hz

/*!
 * \brief Defines the nominal frequency
 */
#define RF_FREQUENCY                                RF_BL_ADV_CHANNEL_0 // Hz

/*!
 * \brief Defines the output power in dBm
 *
 * \remark The range of the output power is [-18..+13] dBm
 */
#define TX_OUTPUT_POWER                             13

/*!
 * \brief Defines the buffer size, i.e. the payload size
 */
#define BUFFER_SIZE                                 20

/*!
 * \brief Number of tick size steps for tx timeout
 */
#define TX_TIMEOUT_VALUE                            10000 // ms

/*!
 * \brief Number of tick size steps for rx timeout
 */
#define RX_TIMEOUT_VALUE                            1000 // ms

/*!
 * \brief Size of ticks (used for Tx and Rx timeout)
 */
#define RX_TIMEOUT_TICK_SIZE                        RADIO_TICK_SIZE_1000_US

/*!
 * \brief Defines the size of the token defining message type in the payload
 */
#define PINGPONGSIZE                                4


/*!
 * \brief Defines the states of the application
 */
typedef enum
{
    APP_LOWPOWER,
    APP_RX,
    APP_RX_TIMEOUT,
    APP_RX_ERROR,
    APP_TX,
    APP_TX_TIMEOUT,
}AppStates_t;

volatile uint32_t txStartTime = 0;
volatile uint32_t txEndTime = 0;

/*!
 * \brief Function to be executed on Radio Tx Done event
 */
void OnTxDone( void );

/*!
 * \brief Function to be executed on Radio Rx Done event
 */
void OnRxDone( void );

/*!
 * \brief Function executed on Radio Tx Timeout event
 */
void OnTxTimeout( void );

/*!
 * \brief Function executed on Radio Rx Timeout event
 */
void OnRxTimeout( void );

/*!
 * \brief Function executed on Radio Rx Error event
 */
void OnRxError( IrqErrorCode_t );

/*!
 * \brief All the callbacks are stored in a structure
 */
RadioCallbacks_t Callbacks =
{
    &OnTxDone,        // txDone
    &OnRxDone,        // rxDone
    NULL,             // syncWordDone
    NULL,             // headerDone
    &OnTxTimeout,     // txTimeout
    &OnRxTimeout,     // rxTimeout
    &OnRxError,       // rxError
    NULL,             // rangingDone
    NULL,             // cadDone
};

/*!
 * \brief The size of the buffer
 */
uint8_t BufferSize = BUFFER_SIZE;

/*!
 * \brief The buffer
 */
uint8_t Buffer[BUFFER_SIZE];
uint8_t prevBuffer[BUFFER_SIZE];

/*!
 * \brief Mask of IRQs to listen to in rx mode
 */
uint16_t RxIrqMask = IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT;

/*!
 * \brief Mask of IRQs to listen to in tx mode
 */
uint16_t TxIrqMask = IRQ_TX_DONE | IRQ_RX_TX_TIMEOUT;

/*!
 * \brief The State of the application
 */
AppStates_t AppState = APP_LOWPOWER;

PacketParams_t packetParams;

PacketStatus_t packetStatus;

int dario_main( void )
{
    typedef enum {
        MODE_RECEIVER,
        MODE_SENDER
    } device_mode_t;

    device_mode_t mode;

    if (HAL_GPIO_ReadPin(IN_SENDER_GPIO_Port, IN_SENDER_Pin) == GPIO_PIN_SET) {
        mode = MODE_SENDER;
        printf("Starting as Sender!\r\n");
    } else {
        mode = MODE_RECEIVER;
        printf("Starting as Receiver!\r\n");
    }

    ModulationParams_t modulationParams;

    HwInit( );
    HAL_Delay( 500 );                   // let DC/DC power ramp up

    Radio.Init( &Callbacks );
    Radio.SetRegulatorMode( USE_DCDC ); // Can also be set in LDO mode but consume more power
    memset( &Buffer, 0x00, BufferSize );

    printf( "\n\n\r     SX1280 Ping Pong Demo Application. %s\n\n\r", FIRMWARE_VERSION );
    printf( "\n\n\r     Radio firmware version 0x%x\n\n\r", Radio.GetFirmwareVersion( ) );

#if defined( MODE_LORA )

    printf( "\nPing Pong running in LORA mode\n\r" );
    modulationParams.PacketType = PACKET_TYPE_LORA;
    modulationParams.Params.LoRa.SpreadingFactor = LORA_SF5;
    modulationParams.Params.LoRa.Bandwidth = LORA_BW_1600;
    modulationParams.Params.LoRa.CodingRate = LORA_CR_LI_4_7;

    packetParams.PacketType = PACKET_TYPE_LORA;
    packetParams.Params.LoRa.PreambleLength = 12;
    packetParams.Params.LoRa.HeaderType = LORA_PACKET_VARIABLE_LENGTH;
    packetParams.Params.LoRa.PayloadLength = BUFFER_SIZE;
    packetParams.Params.LoRa.CrcMode = LORA_CRC_ON;
    packetParams.Params.LoRa.InvertIQ = LORA_IQ_NORMAL;

    char *modeString = "Mode: LORA ";


#elif defined( MODE_FLRC )

    printf( "\nPing Pong running in FLRC mode\n\r" );
    modulationParams.PacketType = PACKET_TYPE_FLRC;
    modulationParams.Params.Flrc.BitrateBandwidth = FLRC_BR_0_260_BW_0_3;
    modulationParams.Params.Flrc.CodingRate = FLRC_CR_1_2;
    modulationParams.Params.Flrc.ModulationShaping = RADIO_MOD_SHAPING_BT_1_0;

    packetParams.PacketType = PACKET_TYPE_FLRC;
    packetParams.Params.Flrc.PreambleLength = PREAMBLE_LENGTH_32_BITS;
    packetParams.Params.Flrc.SyncWordLength = FLRC_SYNCWORD_LENGTH_4_BYTE;
    packetParams.Params.Flrc.SyncWordMatch = RADIO_RX_MATCH_SYNCWORD_1;
    packetParams.Params.Flrc.HeaderType = RADIO_PACKET_VARIABLE_LENGTH;
    packetParams.Params.Flrc.PayloadLength = BUFFER_SIZE;
    packetParams.Params.Flrc.CrcLength = RADIO_CRC_3_BYTES;
    packetParams.Params.Flrc.Whitening = RADIO_WHITENING_OFF;

#endif

    Radio.SetStandby( STDBY_RC );
    Radio.SetPacketType( modulationParams.PacketType );
    Radio.SetModulationParams( &modulationParams );
    Radio.SetPacketParams( &packetParams );
    Radio.SetRfFrequency( RF_FREQUENCY );
    Radio.SetBufferBaseAddresses( 0x00, 0x00 );
    Radio.SetTxParams( TX_OUTPUT_POWER, RADIO_RAMP_02_US );
    
    AppState = APP_LOWPOWER;

    if (mode == MODE_SENDER) {

        Radio.SetDioIrqParams( TxIrqMask, TxIrqMask, IRQ_RADIO_NONE, IRQ_RADIO_NONE );

        uint32_t txCounter = 0;

        // Prepare first packet
        memset(Buffer, 0, BufferSize);
        memcpy(Buffer, &txCounter, sizeof(txCounter));
        txCounter++;

        // Record start time
        txStartTime = HAL_GetTick();

        Radio.SendPayload(Buffer, BufferSize, (TickTime_t){ TX_TIMEOUT_VALUE });

        while (1)
        {
            SX1280ProcessIrqs();

            printf("AppState: %i\r\n", AppState);

            if(AppState == APP_TX)
            {
                // TX finished, measure time
                AppState = APP_LOWPOWER;
                txEndTime = HAL_GetTick();

                printf("TX time: %lu ms\r\n", txEndTime - txStartTime);

                // Prepare next packet
                memset(Buffer, 0, BufferSize);
                memcpy(Buffer, &txCounter, sizeof(txCounter));
                if (txCounter == UINT32_MAX)
                    txCounter = 0;
                else
                    txCounter++;

                // Record start time for next TX
                txStartTime = HAL_GetTick();

                // Send next packet
                Radio.SendPayload(Buffer, BufferSize, (TickTime_t){ TX_TIMEOUT_VALUE }); 
            } 
            else if(AppState == APP_TX_TIMEOUT)
            {
                AppState = APP_LOWPOWER;
                printf("Sender Timeout occurred!\r\n");

                txStartTime = HAL_GetTick();
                Radio.SendPayload(Buffer, BufferSize, (TickTime_t){ TX_TIMEOUT_VALUE });
            }
            HAL_Delay(100);
        }

    } else if (mode == MODE_RECEIVER) {
        
        Radio.SetDioIrqParams( RxIrqMask, RxIrqMask, IRQ_RADIO_NONE, IRQ_RADIO_NONE );
        Radio.SetRx( (TickTime_t){ RX_TIMEOUT_TICK_SIZE, RX_TIMEOUT_VALUE } );

        int8_t rssi = 0;
        

    while(1)
    {
        SX1280ProcessIrqs();

        switch(AppState)
        {
            case APP_RX:
                Radio.GetPayload(Buffer, &BufferSize, BUFFER_SIZE);
                rssi = Radio.GetRssiInst();
                
                printf("DARIO: Received buffer: ");
                for (uint8_t i = 0; i < BufferSize; i++) {
                    printf("%02X ", Buffer[i]);
                }
                printf("With RSSI: %i", rssi);
                
                printf("\r\n");
                
                Radio.SetRx((TickTime_t){ RX_TIMEOUT_TICK_SIZE, RX_TIMEOUT_VALUE });
                AppState = APP_LOWPOWER;
                break;

            case APP_RX_TIMEOUT:
                printf("RX Timeout occurred!\n");
                Radio.SetRx((TickTime_t){ RX_TIMEOUT_TICK_SIZE, RX_TIMEOUT_VALUE });
                AppState = APP_LOWPOWER;
                break;

            case APP_RX_ERROR:
                printf("RX Error occurred!\n");
                Radio.SetRx((TickTime_t){ RX_TIMEOUT_TICK_SIZE, RX_TIMEOUT_VALUE });
                AppState = APP_LOWPOWER;
                break;

            default:
                break;
        }

        HAL_Delay(10);
        }
    } 
}

void OnTxDone( void )
{
    AppState = APP_TX;
}

void OnRxDone(void)
{
    AppState = APP_RX;
}

void OnTxTimeout( void )
{
    AppState = APP_TX_TIMEOUT;
    printf( "<>>>>>>>>TXE\n\r" ); 
    // Radio.SetTx( ( TickTime_t ) { TX_TIMEOUT_VALUE } );
}

void OnRxTimeout( void )
{
    AppState = APP_RX_TIMEOUT;
    // Radio.SetRx((TickTime_t){ RX_TIMEOUT_TICK_SIZE, RX_TIMEOUT_VALUE });
}

void OnRxError( IrqErrorCode_t errorCode )
{
    AppState = APP_RX_ERROR;
    // Radio.SetRx((TickTime_t){ RX_TIMEOUT_TICK_SIZE, RX_TIMEOUT_VALUE });

    printf( "RXE<>>>>>>>>\n\r" ); 
}

void OnRangingDone( IrqRangingCode_t val )
{
}

void OnCadDone( bool channelActivityDetected )
{
}
