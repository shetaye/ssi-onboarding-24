/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
/// \tag::uart_advanced[]

#define UART_ID uart0
#define BAUD_RATE 115200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define MAX_PACKET_LEN 4069
#define ACK_BYTE '!'
#define SYN_RETRIES 3
#define SYN_BYTE '$'
#define SYN_COUNT 3

static int chars_rxed = 0;

queue_t uart_queue;
absolute_time_t last_byte_receive_time;

uint8_t packet_buf[MAX_PACKET_LEN];

// RX interrupt handler
void on_uart_rx()
{
    while (uart_is_readable(UART_ID))
    {
        uint8_t ch = uart_getc(UART_ID);
        queue_try_add(&uart_queue, &ch);
        last_byte_receive_time = get_absolute_time();
    }
}

typedef struct
{
    uint16_t length;   // NOTE: Little endian
    uint16_t seq_num;  // NOTE: Little endian
    uint32_t checksum; // NOTE: Little endian
} packet_header_t;

// From https://gist.github.com/xobs/91a84d29152161e973d717b9be84c4d0
// (not using fast version because we want small binary size)
unsigned int crc32(const uint8_t *message, uint16_t len)
{
    int i, j;
    unsigned int byte, crc, mask;

    i = 0;
    crc = 0xFFFFFFFF;
    while (i < len)
    {
        byte = message[i]; // Get next byte.
        crc = crc ^ byte;
        for (j = 7; j >= 0; j--)
        { // Do eight times.
            mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
        i = i + 1;
    }
    return ~crc;
}

packet_header_t compute_packet_header(const uint8_t *packet, uint16_t len, uint16_t seq_num)
{
    packet_header_t header = {
        len,
        seq_num,
        crc32(packet, len)};

    return header;
}

/**
 * Receive up to num_bytes bytes into the buffer specified by dest.
 *
 * @return Number of bytes received, may be less than desired
 */
uint16_t receive_into(void *dest, uint16_t num_bytes, uint16_t timeout_ms)
{
    uint8_t *dest_ptr; // Convert to char* for arithmetic
    uint16_t bytes_received = 0;
    for (int i = 0; i < timeout_ms; i++)
    {
        // Drain the queue
        while (queue_try_remove(&uart_queue, dest_ptr + bytes_received))
        {
            bytes_received++;
            if (bytes_received == num_bytes)
            {
                return num_bytes;
            }
        }

        sleep_ms(1);
    }

    return bytes_received;
}

bool receive_ack()
{
    // Receive a single byte
    uint8_t received_byte;
    uint16_t received = receive_into(&received_byte, 1, 1000);

    return received && received_byte == ACK_BYTE;
}

bool receive_syn()
{
    // Receive multiple sync bytes
    uint8_t count = 0;

    while (true)
    {
        uint8_t received_byte;
        uint16_t received = receive_into(&received_byte, 1, 1000);

        if (!received)
            return false;

        if (received_byte == SYN_BYTE)
        {
            count++;
        }

        if (count >= SYN_COUNT)
        {
            return true;
        }
    }
}

void send_ack()
{
    uart_putc_raw(UART_ID, ACK_BYTE);
}

void send_syn()
{
    for (int i = 0; i < SYN_COUNT; i++)
    {
        uart_putc_raw(UART_ID, SYN_BYTE);
    }
}

bool packet_handler_write(const uint8_t *packet, uint16_t len, uint16_t seq_num)
{
    // Check packet length
    if (len > MAX_PACKET_LEN)
    {
        printf("Packet is too long!\n");
        return false;
    }

    // Write sync packet and wait for ack
    bool syn_acknowledged = false;
    for (int i = 0; i < SYN_RETRIES; i++)
    {
        for (int j = 0; j < SYN_COUNT; j++)
        {
            uart_putc_raw(UART_ID, SYN_BYTE);
        }

        if (receive_ack())
        {
            syn_acknowledged = true;
            break;
        }

        sleep_ms(100);
    }

    if (!syn_acknowledged)
    {
        printf("Payload did not respond to sync!\n");
        return false;
    }

    // Calculate the header
    packet_header_t header = compute_packet_header(packet, len, seq_num);

    // Send header and receive ACK
    uart_write_blocking(UART_ID, (uint8_t *)&header, sizeof(packet_header_t));

    if (!receive_ack())
    {
        printf("Header was not acknowledged!\n");
        return false;
    }

    // Send actual packet
    uart_write_blocking(UART_ID, packet, len);

    // Wait for ACK
    return receive_ack();
}

bool packet_handler_read(uint8_t *packet)
{
    uint16_t bytes_received;

    // Wait for sync
    if (!receive_syn())
    {
        printf("Syn was not received!\n");
        return false;
    }
    send_ack();

    // Receive header
    packet_header_t header;
    bytes_received = receive_into(&header, sizeof(packet_header_t), 1000);

    if (bytes_received < sizeof(packet_header_t))
    {
        printf("Header was not received!\n");
        return false;
    }
    send_ack();

    // Check header
    if (header.length > MAX_PACKET_LEN)
    {
        printf("Packet is too long!\n");
        return false;
    }

    // Read actual packet
    bytes_received = receive_into(packet, header.length, 1000);

    if (bytes_received < header.length)
    {
        printf("Packet was not fully received!\n");
        return false;
    }

    // Verify checksum
    if (crc32(packet, header.length) != header.checksum)
    {
        printf("Invalid checksum!\n");
        return false;
    }
    send_ack();
}

int main()
{
    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));

    // Actually, we want a different speed
    // The call will return the actual baud rate selected, which will be as close as
    // possible to that requested
    uart_init(UART_ID, BAUD_RATE);

    // Set UART flow control CTS/RTS, we don't want these, so turn them off
    uart_set_hw_flow(UART_ID, false, false);

    // Set our data format
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

    // Turn off FIFO's - we want to do this character by character
    uart_set_fifo_enabled(UART_ID, false);

    // Set up a RX interrupt
    // We need to set up the handler first
    // Select correct interrupt for the UART we are using
    int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

    queue_init(&uart_queue, 1, 256);

    // And set up and enable the interrupt handlers
    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(UART_ID, true, false);

    // OK, all set up.
    // Lets send a basic string out, and then run a loop and wait for RX interrupts
    // The handler will count them, but also reflect the incoming data back with a slight change!
    const uint8_t msg[] = "THIS IS A TEST PACKET";
    const uint16_t len = sizeof(msg) - 1;

    while (1)
    {
        packet_handler_write(msg, len, 999);
        sleep_ms(1000);
    }
}

/// \end:uart_advanced[]