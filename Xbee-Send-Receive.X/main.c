/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
    may accompany Microchip software.
    
    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER 
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY 
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS 
    FOR A PARTICULAR PURPOSE.
    
    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP 
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO 
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL 
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT 
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS 
    SOFTWARE.
*/
#define F_CPU 10000000

#include "mcc_generated_files/mcc.h"
#include "libavrxbee/xbee.h"
#include <util/delay.h>

struct xbee_tx_request r;
struct xbee_frame *f;

uint8_t buffer[128];
uint64_t coord_addresss;

void modem_open(uint64_t coordinator){
    coord_addresss = coordinator;
    // Reset the modem ~{RESET} pin...
    XBEE_RESET_SetLow();
    XBEE_RESET_SetHigh();    
}

uint64_t modem_get_coord_addr(){
    return coord_addresss;
}

void modem_close(void){
    
}


void modem_send_message(unsigned char* node_message, uint8_t data_length){
    printf("modem_send_message: BEGIN \n");
    
    // The message stream is actually a node message stream. This needs to be
    // converted to an XBee message stream and then written to the XBee using
    // the USART0_Write. This is not like the TW0 API that uses a finite state
    // machine to set a buffer and then write the content - which could be 
    // simpler
    
    r.addr = modem_get_coord_addr();
    r.network = 0xFFFE;
    r.radius = 0;
    r.opts = 0;
    r.data = node_message;
    r.len =  data_length;

    f = xbee_create_tx_request_frame(0x01, &r);

    unsigned int size;
    unsigned char *bytes;
    bytes = xbee_frame_to_bytes(f, &size);
    printf("Sending: length = %04x, ", size);
    for(int idx = 0; idx<size; idx++){
        printf("%02X ", bytes[idx]);
    }
    printf("\n");
    uint8_t tx_idx = 0;
    while(USART0_IsTxReady()){
        USART0_Write(bytes[tx_idx++]);
        if(tx_idx >= size){
            break;
        }
    }
    printf("modem_send_message: END\n");
}




/*
    Main application
*/
#define DATA_LENGTH 73 // Upper limit for the payload
// xbee buffer size is 14 + DATA_LENGTH
int main(void)
{
    /* Initialises MCU, drivers and middleware */
    SYSTEM_Initialize();
    
    printf("Start \n");

    XBEE_RESET_SetHigh();    
    modem_open(XBEE_ADDR_BROADCAST);
    _delay_ms(1000);
    
    unsigned char msg[DATA_LENGTH] ={0};

    for(uint8_t i=0; i<DATA_LENGTH; i++){
        msg[i] = i+1;
    }
    modem_send_message(msg, DATA_LENGTH);
    printf("Entering ctrl loop \n");
    while (1){
        _delay_ms(30000);
    }
}
/**
    End of File
*/