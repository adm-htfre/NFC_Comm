#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "board.h"
#include "ndeft2t/ndeft2t.h"
#include "./adcconv/adcconv.h"
/*
* adcconv.h
*
*/
#ifndef ADCCONV_ADCCONV_H_
#define ADCCONV_ADCCONV_H_
#include "chip.h"
#define AN0 ADCDAC_IO_ANA0_0
#define AN1 ADCDAC_IO_ANA0_1
#define AN2 ADCDAC_IO_ANA0_2
#define AN3 ADCDAC_IO_ANA0_3
#define AN4 ADCDAC_IO_ANA0_4
#define AN5 ADCDAC_IO_ANA0_5
bool ADCconv_Init();
float get_ADCconv();
float get_ADCconv_I2D();
#endif /* ADCCONV_ADCCONV_H_ */
const uint8_t locale[] = "en"; /** TEXT record locale. */
uint8_t payloadText[30]; /** TEXT record payload. */
//Store Resistance value
volatile float resval = -1;
/** Data structure for the NDEFT2T mode */

uint8_t instanceBuffer[NDEFT2T_INSTANCE_SIZE];
/** Instance buffer shared for NDEF read and write operation */
uint8_t messageBuffer[NF_C_SHARED_MEM_BYTE_SIZE];
/** Message buffer for NDEF */
/** Variables used for Case management */
typedef enum USECASE {
USECASE_ADCVAL
} USECASE_T;

static USECASE_T currentUseCase = USECASE_ADCVAL; /** indicates the current use case */
/** Variables used for tracking interrupts. */
static volatile bool RFFieldPresent = false; /** Flag indicating presence of the RF field */
static volatile bool sMsgReady = false; /** Flag indicating a valid NDEF message in shared memory */
/** Function for driver and board initialization. */
static void AppInit(void); /** Function for NFC interrupt handling. */
static void HandleNF_C(void); /** Function to create and commit an empty NDEF message for NDEF formatting. */
static bool CreateAndCommitEmptyMessage(void); /** Function to combine all steps for NDEF message (with TEXT and MIME records) creation and commit. */
static bool CreateAndCommitADCMIMEMessage(void); /** Function to handle all steps for NDEF Message parsing from shared memory. */
static bool ParseNDEFMessage(void); /** define NDEFT2T Call back **/
/** Call back from NDEFT2T MOD. Refer #NDEFT2T_FIELD_STATUS_CB. */

void NDEFT2T_FieldStatus_Cb(bool status)
{
RFFieldPresent = status; /** Call back from NDEFT2T MOD. Refer #NDEFT2T_MSG_AVAILABLE_CB. */
}

void NDEFT2T_MsgAvailable_Cb(void)
{
sMsgReady = true;
}

/** Main **/
int main(void)
{
    AppInit(); 
    
    ADCconv_Init();  //Initiate ADC
    //Get resistance
    // resval = get_ADCconv_I2D();
    while (true) 
        {
        HandleNF_C();
        }
    return 0;
}
/*****************************************************************************
* Private functions
****************************************************************************/
/**
* Function for driver and board initialization.
*/
void AppInit(void)
{
    /** Set system clock to 4MHz. needed to support prints to terminal. */
    Chip_Clock_System_SetClockFreq(4000000);
    /** Board initialization. */
    Board_Init();
    /** Initialize NFC HW block. */
    Chip_NF_C_Init(NSS_NF_C);

    NDEFT2T_Init();
    /** to enable external readers to recognize the tag, NDEF formatting of the tag is required at start-up. */
    if (false == CreateAndCommitEmptyMessage()) { }
} 

/** Function for NFC interrupt handling.*/

static void HandleNF_C(void)
{
    bool status = false;
    if (RFFieldPresent) { /** RF field present */
        if (sMsgReady) {
            sMsgReady = false;
            status = ParseNDEFMessage();
            if (status == false) {}
        }
        if(currentUseCase == USECASE_ADCVAL){
            // Put Resistance Value into payload Text
            resval = get_ADCconv();
            sprintf((char *)payloadText, "Current resistance: %4.2f", (float)(resval));
            if (false == CreateAndCommitADCMIMEMessage()) {}
        }
    }
}
/**
* Function to create and commit an empty NDEF message for NDEF formatting.
* @return Propagates the return value of the NDEFT2T_Commit Message () from @ref MODS_NSS_NDEFT2T "NDEDT2T mod".
* @note Refer to @ref MODS_NSS_NDEFT2T.NDEFT2T_CommitMessage for additional tag information.
*/
static bool CreateAndCommitEmptyMessage(void)
{
/** creates an NDEF message */ // What is a an NDEF message: https://developer.android.com/reference/android/nfc/NdefMessage 
    NDEFT2T_CreateMessage((void*)instanceBuffer,messageBuffer, NF_C_SHARED_MEM_BYTE_SIZE, true);
/** Finish the NDEF processing and commit generated message to shared memory. */
    return NDEFT2T_CommitMessage((void*)instanceBuffer);
}
/**
* Function to combine all steps for NDEF message (with TEXT and MIME records) creation and commit.
* @return Propagates the return value of the NDEFT2T_Commit Message () from @ref MODS_NSS_NDEFT2T "NDEDT2T mod".
* @note Refer to @ref MODS_NSS_NDEFT2T.NDEFT2T_CommitMessage for additional tag information.
*/
static bool CreateAndCommitADCMIMEMessage(void)
{
    NDEFT2T_CREATE_RECORD_TAGINFO_T recordTagInfo;
    bool status;
    (void) status;
    /* suppresses unused warning in release build (due to ASSERT statements which are removed) */
    /** creates an NDEF message */
    NDEFT2T_CreateMessage((void*)instanceBuffer, messageBuffer, NF_C_SHARED_MEM_BYTE_SIZE, true);
    /** create a Text record */
    recordTagInfo.shortRecord = true; /** Enable Short Record. */
    recordTagInfo.pString = (uint8_t *)locale; /** Assign language code. */
    status = NDEFT2T_CreateTextRecord((void*)instanceBuffer, &recordTagInfo);
    ASSERT(status);
    /** NULL terminator excluded for size parameter */
    status = NDEFT2T_WriteRecordPayload((void*)instanceBuffer, (void *)payloadText, (sizeof(payloadText) - 1));
    ASSERT(status);
    NDEFT2T_CommitRecord((void*)instanceBuffer);
    /** Finish the NDEF processing and commit generated message to shared memory. */
    return NDEFT2T_CommitMessage((void*)instanceBuffer);
}
/**
* Function to handle all steps for NDEF Message parsing from shared memory.
* @return Propagates the return value of the NDEFT2T_GetMessage() from @ref MODS_NSS_NDEFT2T "NDEDT2T mod".
*/
static bool ParseNDEFMessage(void)
{
    NDEFT2T_PARSE_RECORD_TAGINFO_T recordTagInfo;
    bool status = false;
    int payloadLength = 0;
    uint8_t *pPayloadStart = NULL;
    
    /** Get Message to buffer. */
    status = NDEFT2T_GetMessage((void*)instanceBuffer, messageBuffer, NF_C_SHARED_MEM_BYTE_SIZE);
    if (status == true) {
    /** Get record type TagInfo. This step is mandatory for read operation. Continue till end of message. */
    while (NDEFT2T_GetNextRecord((void*)instanceBuffer, &recordTagInfo) != false) 
        {
            pPayloadStart = (uint8_t *)NDEFT2T_GetRecordPayload((void*)instanceBuffer, &payloadLength);

            switch (recordTagInfo.type) {
            case NDEFT2T_RECORD_TYPE_EXT: /** Print NFC Forum External type record to terminal. */
            break;
            case NDEFT2T_RECORD_TYPE_MIME: /** Print MIME record as hex to terminal. */
            break;
            case NDEFT2T_RECORD_TYPE_TEXT: /** Print TEXT record to terminal (restricted to a length of 32). */
            break;
            case NDEFT2T_RECORD_TYPE_URI: /** Print URI record to terminal. */

            break;
            case NDEFT2T_RECORD_TYPE_EMPTY: /** Print EMPTY record. */
            break;
            default: /** do nothing. */
            break; 
            }
        }
    }
    return status;
}
/*
* adcconv.c
/* Set pins function */
*/
#include <string.h>
#include <stdlib.h>
#include "board.h"
#include "adcconv.h"
static void ADC_Init();
static void I2D_Init();
bool isReady = false;
float adcInput;

static void ADC_Init(){
    //ADC Single-shot
    Chip_IOCON_SetPinConfig(NSS_IOCON, IOCON_ANA0_3, IOCON_FUNC_1);
    /* Set pin function to analogue */
    Chip_ADCDAC_Init(NSS_ADCDAC0);
    Chip_ADCDAC_SetMuxADC(NSS_ADCDAC0, AN3);
    Chip_ADCDAC_SetInputRangeADC(NSS_ADCDAC0, ADCDAC_INPUTRANGE_WIDE);
    Chip_ADCDAC_SetModeADC(NSS_ADCDAC0, ADCDAC_SINGLE_SHOT);
}

static void ADC_REF_Init(){
    //ADC Single-shot
    Chip_IOCON_SetPinConfig(NSS_IOCON, IOCON_ANA0_2, IOCON_FUNC_1);
    /* Set pin function to analogue */
    Chip_ADCDAC_Init(NSS_ADCDAC0);
    Chip_ADCDAC_SetMuxADC(NSS_ADCDAC0, AN2);
    Chip_ADCDAC_SetInputRangeADC(NSS_ADCDAC0, ADCDAC_INPUTRANGE_WIDE);
    Chip_ADCDAC_SetModeADC(NSS_ADCDAC0, ADCDAC_SINGLE_SHOT);
}
static void I2D_Init(){
    //Chip_IOCON_SetPinConfig(NSS_IOCON, IOCON_ANA0_3, IOCON_FUNC_1); /* Set pin function to analogue */
    Chip_I2D_Init(NSS_I2D);
    Chip_I2D_Setup(NSS_I2D, I2D_SINGLE_SHOT, I2D_SCALER_GAIN_100_1, I2D_CONVERTER_GAIN_LOW , 100);
    Chip_I2D_SetMuxInput(NSS_I2D, I2D_INPUT_ANA0_3);
}

float get_ADCconv(){
float res;
float vss_input = 3300;

//Input voltage
/** output in μV = ((native value - native offset) * internal operating voltage / steps per μV) + offset
* native offset = 2048
* internal voltage = 1.2V
* gain (steps per μV) = 2730
* offset = 0.9V **/

float r_0_kohm = 10000; //Pull-up Resistor
int ref_r_0 = 10000;
int ref_r_1 = 10000;
// calibrate Voltage reference
ADC_REF_Init();
Chip_ADCDAC_StartADC(NSS_ADCDAC0);
while (!(Chip_ADCDAC_ReadStatus(NSS_ADCDAC0) & ADCDAC_STATUS_ADC_DONE)) {
; /* Wait until measurement completes. For single-shot mode only! */
}
adcInput = (((float)((Chip_ADCDAC_GetValueADC(NSS_ADCDAC0) - 2048))*1.2) / 2730 + 0.9);
vss_input = ((float)(ref_r_1 + ref_r_0)/(float)(ref_r_1)) * adcInput;
ADC_Init();
Chip_ADCDAC_StartADC(NSS_ADCDAC0);
while (!(Chip_ADCDAC_ReadStatus(NSS_ADCDAC0) & ADCDAC_STATUS_ADC_DONE)) {
; /* Wait until measurement completes. For single-shot mode only! */
}
adcInput = (((float)((Chip_ADCDAC_GetValueADC(NSS_ADCDAC0) - 2048))*1.2) / 2730 + 0.9);
res = ((float)r_0_kohm * adcInput)/ ((float)vss_input - adcInput);
return res;
}
float get_ADCconv_I2D(){
int i2dValue;
int i2dNativeValue;
float res;
// Chip_ADCDAC_StartDAC(NSS_ADCDAC0);
Chip_ADCDAC_StartADC(NSS_ADCDAC0);
while (!(Chip_ADCDAC_ReadStatus(NSS_ADCDAC0) & ADCDAC_STATUS_ADC_DONE)) {
; /* Wait until measurement completes. For single-shot mode only! */
}
adcInput = ((float)Chip_ADCDAC_GetValueADC(NSS_ADCDAC0) * 1600) / 4096;
Chip_ADCDAC_DeInit(NSS_ADCDAC0);
Chip_I2D_Start(NSS_I2D);
while (!(Chip_I2D_ReadStatus(NSS_I2D) & I2D_STATUS_CONVERSION_DONE)) {
; /* wait */
}
i2dNativeValue = Chip_I2D_GetValue(NSS_I2D);
i2dValue = Chip_I2D_NativeToPicoAmpere(i2dNativeValue, I2D_SCALER_GAIN_100_1, I2D_CONVERTER_GAIN_LOW, 100);
// Chip_ADCDAC_StopDAC(NSS_ADCDAC0);
// Chip_ADCDAC_DeInit(NSS_ADCDAC0);
// Chip_I2D_DeInit(NSS_I2D);
res = (adcInput / (float)i2dValue) * 1000000000;
return res;
}