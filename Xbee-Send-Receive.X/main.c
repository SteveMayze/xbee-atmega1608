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

void modem_open(uint64_t coordinator) {
    coord_addresss = coordinator;
    // Reset the modem ~{RESET} pin...
    XBEE_RESET_SetLow();
    XBEE_RESET_SetHigh();
    _delay_ms(1000);
}

uint64_t modem_get_coord_addr() {
    return coord_addresss;
}

void modem_close(void) {

}

void modem_send_message(unsigned char* node_message, uint8_t data_length) {
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
    r.len = data_length;

    f = xbee_create_tx_request_frame(0x01, &r);

    unsigned int size;
    unsigned char *bytes;
    bytes = xbee_frame_to_bytes(f, &size);
    printf("Sending: length = %04x, Data: ", size);
    for (int idx = 0; idx < size; idx++) {
        printf("%02X ", bytes[idx]);
    }
    printf("\n");
    uint8_t tx_idx = 0;
    while (USART0_IsTxReady()) {
        USART0_Write(bytes[tx_idx++]);
        if (tx_idx >= size) {
            break;
        }
    }
    printf("modem_send_message: END\n");
}


#define HEADER_GROUP 0x10
#define OPERATION_GROUP 0x20
#define PROPERTY_GROUP 0x30
#define METADATA_DOMAIN_GROUP  0x40
#define METADATA_CLASS_GROUP  0x50

typedef enum token_e {
    NODE_TOKEN_VOID = 0x00,
    NODE_TOKEN_HEADER_OPERATION = HEADER_GROUP | 0x01,
    NODE_TOKEN_HEADER_SERIAL_ID = HEADER_GROUP | 0x02,
    NODE_TOKEN_HEADER_DOMAIN = HEADER_GROUP | 0x03,
    NODE_TOKEN_HEADER_CLASS = HEADER_GROUP | 0x04,

    // OPERATIONS
    NODE_TOKEN_READY = OPERATION_GROUP | 0x01,
    NODE_TOKEN_DATAREQ = OPERATION_GROUP | 0x02,
    NODE_TOKEN_DATA = OPERATION_GROUP | 0x03,
    NODE_TOKEN_DATAACK = OPERATION_GROUP | 0x04,
    NODE_TOKEN_NODEINTROREQ = OPERATION_GROUP | 0x05,
    NODE_TOKEN_NODEINTRO = OPERATION_GROUP | 0x06,
    NODE_TOKEN_NODEINTROACK = OPERATION_GROUP | 0x07,

    NODE_TOKEN_PROPERTY = PROPERTY_GROUP | 0x00,
    NODE_TOKEN_PROPERTY_BUS_VOLTAGE = PROPERTY_GROUP | 0x01,
    NODE_TOKEN_PROPERTY_SHUNT_VOLTAGE = PROPERTY_GROUP | 0x02,
    NODE_TOKEN_PROPERTY_LOAD_CURRENT = PROPERTY_GROUP | 0x03,

    // METADATA
    NODE_METADATA_DOMAIN_POWER = METADATA_DOMAIN_GROUP | 0x01,
    NODE_METADATA_DOMAIN_CAPACITY = METADATA_DOMAIN_GROUP | 0x02,
    NODE_METADATA_DOMAIN_RATEY = METADATA_DOMAIN_GROUP | 0x03,

    NODE_METADATA_CLASS_SENSOR = METADATA_CLASS_GROUP | 0x03,
    NODE_METADATA_CLASS_ACTUATOR = METADATA_CLASS_GROUP | 0x03,

} Token_t;

typedef struct response_message {
    Token_t operation;
    uint8_t data_length;
    uint8_t *data;

} ModemResponse_t;


uint8_t rx_buffer[USART0_RX_BUFFER_SIZE];
struct xbee_rx_packet p;
uint8_t rx_pkt[90];

static ModemResponse_t response;
struct xbee_tx_status s;

ModemResponse_t* modem_receive_message(void) {
    printf("modem_receive_message: BEGIN\n");

    // Receive XBee message
    uint8_t buffer_ptr = 0;
    uint8_t d;
    while (USART0_IsRxReady()) {
        d = USART0_Read();
        if(buffer_ptr<USART0_RX_BUFFER_SIZE)
            rx_buffer[buffer_ptr++] = d;
    }

    printf("Received: size: %d, data: ", buffer_ptr);
    for (int idx = 0; idx < buffer_ptr; idx++) {
        printf("%02X ", rx_buffer[idx]);
    }
    printf("\n");

    switch (rx_buffer[3]) {
        case 0x8B:
            printf("Modem Status \n");
            xbee_frame_to_tx_status(rx_buffer, &s);
            response.operation = NODE_TOKEN_VOID;
            break;
        case 0x90:
            printf("Modem Rx Request \n");
            p.data = rx_pkt;
            xbee_frame_to_rx_packet(rx_buffer, &p);
            coord_addresss = p.addr;
            response.operation = p.data[1];
            response.data_length = p.len;
            response.data = p.data;
            printf("Received  length: %d \n", p.len);
            break;
        default:
            response.operation = NODE_TOKEN_VOID;
            break;
    }
    printf("modem_receive_message: END\n");
    return &response;
}


/*
    Main application
 */
#define DATA_LENGTH 73 // Upper limit for the payload
// xbee buffer size is 14 + DATA_LENGTH
ModemResponse_t* resp;

int main(void) {
    /* Initialises MCU, drivers and middleware */
    SYSTEM_Initialize();

    printf("\n\nStart \n");

    XBEE_RESET_SetHigh();
    modem_open(XBEE_ADDR_BROADCAST);

    unsigned char msg[DATA_LENGTH] = {0};

    for (uint8_t i = 0; i < DATA_LENGTH; i++) {
        msg[i] = i + 1;
    }
    modem_send_message(msg, DATA_LENGTH);
    printf("Entering ctrl loop \n");
    while (1) {

        if (USART0_IsRxReady()) {
                resp = modem_receive_message();
                printf("response size: %d \n", resp->data_length);
            }
        _delay_ms(5000);
    }
}
/**
    End of File
 */