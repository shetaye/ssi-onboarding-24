/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include "pico/printf.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#define UART_ID uart0
#define BAUD_RATE 115200

// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define MAX_PACKET_LEN 4069
#define ACK_BYTE '!'
#define ACK_RETRIES 3
#define SYN_BYTE '$'

// static int chars_rxed = 0;

// // RX interrupt handler
// void on_uart_rx()
// {
//     while (uart_is_readable(UART_ID))
//     {
//         uint8_t ch = uart_getc(UART_ID);
//         // Can we send it back?
//         if (uart_is_writable(UART_ID))
//         {
//             // Change it slightly first!
//             ch++;
//             uart_putc(UART_ID, ch);
//         }
//         chars_rxed++;
//     }
// }

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

bool receive_ack()
{
    // Receive a single byte
    uint8_t received_byte = 0;
    uart_read_blocking(UART_ID, &received_byte, sizeof(uint8_t));

    return received_byte == ACK_BYTE;
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
    uart_putc_raw(UART_ID, SYN_BYTE);
    uart_putc_raw(UART_ID, SYN_BYTE);
    uart_putc_raw(UART_ID, SYN_BYTE);
    if (!receive_ack()) {
        return false;
    }

    // Calculate the header
    packet_header_t header = compute_packet_header(packet, len, seq_num);

    // Send header and receive ACK
    bool header_was_received = false;

    for (int i = 0; i < ACK_RETRIES; i++)
    {
        // Write header
        uart_write_blocking(UART_ID, (uint8_t *)&header, sizeof(packet_header_t));

        if (receive_ack())
        {
            header_was_received = true;
            break;
        }
        sleep_ms(500);
    }

    if (!header_was_received)
    {
        printf("Header was not acknowledged!\n");
        return false;
    }

    // Send actual packet
    uart_write_blocking(UART_ID, packet, len);

    // Wait for ACK
    return receive_ack();
}

int main()
{
    // Set up our UART with a basic baud rate.
    uart_init(UART_ID, BAUD_RATE);
    uart_set_fifo_enabled(UART_ID, false);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));

    const uint8_t msg[] = "HELLO THIS IS A PACKET";
    const uint16_t len = (sizeof(msg) / sizeof(msg[0])) - 1;

    sleep_ms(1000);
    while (1)
    {
        packet_handler_write(msg, len, 999);
        sleep_ms(1000);
    }
}

/// \end:uart_advanced[]