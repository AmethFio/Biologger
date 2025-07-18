#ifndef UARTE_STREAM_H
#define UARTE_STREAM_H

#include <Arduino.h>
#include "nrfx_uarte.h"

class UARTEStream : public Stream {
public:
    UARTEStream();
    bool begin(uint32_t tx_pin, uint32_t rx_pin, uint32_t baudrate = 115200, size_t rx_chunk_size = 1);
    void end();
    size_t _rx_chunk_size;
    static volatile size_t _rx_position;

    void setTimeout(unsigned long timeout){ _timeout = timeout; } 
    int peek() override;

    int available() override;
    int read() override;
    size_t readBytes(char *buffer, size_t length) ;
    String readString() ;
    String readStringUntil(char terminator);
    void flush() override;
    size_t write(uint8_t data) override;
    using Print::write;
    size_t print(const String &s);
    size_t println(const String &s);
    size_t print(const char[]);
    size_t print(char);
    size_t print(unsigned char, int = DEC);
    size_t print(int, int = DEC);
    size_t print(unsigned int, int = DEC);
    size_t print(long, int = DEC);
    size_t print(unsigned long, int = DEC);
    size_t print(double, int = 2);

    size_t println(void);
    size_t println(const char[]);
    size_t println(char);
    size_t println(unsigned char, int = DEC);
    size_t println(int, int = DEC);
    size_t println(unsigned int, int = DEC);
    size_t println(long, int = DEC);
    size_t println(unsigned long, int = DEC);
    size_t println(double, int = 2);


private:
    static void _event_handler(nrfx_uarte_event_t const *p_event, void *p_context);
    void _rx_done(uint8_t *data, size_t length);

    unsigned long _timeout = 1000;

    static nrfx_uarte_t _uarte;
    static uint8_t _rx_buffer[64];
    static volatile size_t _rx_length;
    static volatile bool _rx_ready;

    static const size_t _ring_buffer_size = 256;
    uint8_t _ring_buffer[_ring_buffer_size];
    volatile size_t _ring_head;
    volatile size_t _ring_tail;

    void _start_rx();
    void _move_to_ring_buffer(size_t length);
};

#endif
