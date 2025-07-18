#include "UARTEStream.h"
#include <Adafruit_TinyUSB.h>
#include <cstdio>
#include <stdlib.h> // for dtostrf

nrfx_uarte_t UARTEStream::_uarte = NRFX_UARTE_INSTANCE(1);
uint8_t UARTEStream::_rx_buffer[64];
volatile size_t UARTEStream::_rx_length = 0;
volatile bool UARTEStream::_rx_ready = false;
volatile size_t UARTEStream::_rx_position = 0;

UARTEStream::UARTEStream()
    : _ring_head(0), _ring_tail(0), _rx_chunk_size(1) {}

bool UARTEStream::begin(uint32_t tx_pin, uint32_t rx_pin, uint32_t baudrate, size_t rx_chunk_size) {
    _rx_position = 0;
    if(rx_chunk_size == 0 || rx_chunk_size > sizeof(_rx_buffer)){
        return false;
    }
    _rx_chunk_size = rx_chunk_size;

    nrf_uarte_baudrate_t actual_baudrate;
    switch(baudrate){
      case 1200: actual_baudrate = NRF_UARTE_BAUDRATE_1200; break;
      case 2400: actual_baudrate = NRF_UARTE_BAUDRATE_2400; break;
      case 4800: actual_baudrate = NRF_UARTE_BAUDRATE_4800; break;
      case 9600: actual_baudrate = NRF_UARTE_BAUDRATE_9600; break;
      case 14400: actual_baudrate = NRF_UARTE_BAUDRATE_14400; break;
      case 19200: actual_baudrate = NRF_UARTE_BAUDRATE_19200; break;
      case 28800: actual_baudrate = NRF_UARTE_BAUDRATE_28800; break;
      case 31250: actual_baudrate = NRF_UARTE_BAUDRATE_31250; break;
      case 38400: actual_baudrate = NRF_UARTE_BAUDRATE_38400; break;
      case 57600: actual_baudrate = NRF_UARTE_BAUDRATE_57600; break;
      case 76800: actual_baudrate = NRF_UARTE_BAUDRATE_76800; break;
      case 115200: actual_baudrate = NRF_UARTE_BAUDRATE_115200; break;
      case 230400: actual_baudrate = NRF_UARTE_BAUDRATE_230400; break;
      case 250000: actual_baudrate = NRF_UARTE_BAUDRATE_250000; break;
      case 460800: actual_baudrate = NRF_UARTE_BAUDRATE_460800; break;
      case 921600: actual_baudrate = NRF_UARTE_BAUDRATE_921600; break;
      case 1000000: actual_baudrate = NRF_UARTE_BAUDRATE_1000000; break;
      default:  actual_baudrate = NRF_UARTE_BAUDRATE_115200;break;
    }

    nrfx_uarte_config_t config = {
        .pseltxd            = tx_pin,
        .pselrxd            = rx_pin,
        .pselcts            = NRF_UARTE_PSEL_DISCONNECTED,
        .pselrts            = NRF_UARTE_PSEL_DISCONNECTED,
        .p_context          = this,
        .baudrate           = actual_baudrate,
        .interrupt_priority = NRFX_UARTE_DEFAULT_CONFIG_IRQ_PRIORITY,
        .hal_cfg = {
            .hwfc   = NRF_UARTE_HWFC_DISABLED,
            .parity = NRF_UARTE_PARITY_EXCLUDED,
        },
    };

    if (nrfx_uarte_init(&_uarte, &config, _event_handler) != NRFX_SUCCESS) {
        return false;
    }

    _start_rx();
    return true;
}

void UARTEStream::end() {
    nrfx_uarte_uninit(&_uarte);
    _rx_ready = false;
    _rx_length = 0;
    _ring_head = 0;
    _ring_tail = 0;
}

void UARTEStream::_start_rx() {
    _rx_ready = false;
    nrfx_uarte_rx(&_uarte, _rx_buffer, _rx_chunk_size);
}

void UARTEStream::_move_to_ring_buffer(size_t length) {
    for (size_t i = 0; i < length; ++i) {
        size_t next_head = (_ring_head + 1) % _ring_buffer_size;
        
        if(next_head == _ring_tail){
          _ring_tail = (_ring_tail + 1) % _ring_buffer_size;
        }

          _ring_buffer[_ring_head] = _rx_buffer[i];
          _ring_head = next_head;
    }
    _rx_ready = false;
    _rx_length = 0;
    _rx_position = 0;
    _start_rx();
}

int UARTEStream::available() {
    if(_ring_head >= _ring_tail){
      return _ring_head - _ring_tail;
    } else {
      return _ring_buffer_size - _ring_tail + _ring_head;
    }
}

int UARTEStream::read() {
    if (available() == 0) return -1;

    uint8_t data = _ring_buffer[_ring_tail];
    _ring_tail = (_ring_tail + 1) % _ring_buffer_size;

    return data;
}

size_t UARTEStream::readBytes(char *buffer, size_t length) {
    size_t count = 0;
    while (count < length && available()) {
        buffer[count++] = read();
    }
    return count;
}

String UARTEStream::readString() {
    String s;
    while (available()) {
        s += (char)read();
    }
    return s;
}

String UARTEStream::readStringUntil(char terminator){
    String result;
    unsigned long start = millis();
    while ((millis() - start) < _timeout){
        if (available()){
            char c = (char)read();
            result += c;
            if (c == terminator) break;
        } else {
            delay(1);
        }
    }
    return result;
}

int UARTEStream::peek(){
  if(available() == 0) return -1;
  return _ring_buffer[_ring_tail];
}

void UARTEStream::flush() {
    size_t amount = nrf_uarte_rx_amount_get(_uarte.p_reg);
    for (size_t i = _rx_position; i < amount; ++i) {
        _ring_buffer[_ring_head] = _rx_buffer[i];
        _ring_head = (_ring_head + 1) % _ring_buffer_size;
    }
    _rx_position = 0;
    _rx_ready = false;
    _start_rx();
    _ring_head = _ring_tail = 0;
}

size_t UARTEStream::write(uint8_t data) {
    nrfx_err_t err;
    do {
        err = nrfx_uarte_tx(&_uarte, &data, 1);
    } while (err == NRFX_ERROR_BUSY);
    return 1;
}

size_t UARTEStream::print(const String &s) {
    return write((const uint8_t *)s.c_str(), s.length());
}

size_t UARTEStream::println(const String &s) {
    size_t n = print(s);
    n += write('\r');
    n += write('\n');
    return n;
}

size_t UARTEStream::print(const char str[]) {
    return write((const uint8_t *)str, strlen(str));
}

size_t UARTEStream::print(char c) {
    return write(c);
}

size_t UARTEStream::print(unsigned char b, int base) {
    return print((unsigned long)b, base);
}

size_t UARTEStream::print(int n, int base) {
    return print((long)n, base);
}

size_t UARTEStream::print(unsigned int n, int base) {
    return print((unsigned long)n, base);
}

size_t UARTEStream::print(long n, int base) {
    char buf[8 * sizeof(long) + 1];
    if (base == 10) {
        sprintf(buf, "%ld", n);
    } else if (base == 16) {
        sprintf(buf, "%lx", n);
    } else {
        ultoa(n, buf, base);
    }
    return print(buf);
}

size_t UARTEStream::print(unsigned long n, int base) {
    char buf[8 * sizeof(unsigned long) + 1];
    if (base == 10) {
        sprintf(buf, "%lu", n);
    } else if (base == 16) {
        sprintf(buf, "%lx", n);
    } else {
        ultoa(n, buf, base);
    }
    return print(buf);
}

size_t UARTEStream::print(double n, int digits) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", digits, n);
    return print(buf);
}

size_t UARTEStream::println(void) {
    return write('\r') + write('\n');
}

size_t UARTEStream::println(const char c[]) {
    return print(c) + println();
}

size_t UARTEStream::println(char c) {
    return print(c) + println();
}

size_t UARTEStream::println(unsigned char b, int base) {
    return print(b, base) + println();
}

size_t UARTEStream::println(int num, int base) {
    return print(num, base) + println();
}

size_t UARTEStream::println(unsigned int num, int base) {
    return print(num, base) + println();
}

size_t UARTEStream::println(long num, int base) {
    return print(num, base) + println();
}

size_t UARTEStream::println(unsigned long num, int base) {
    return print(num, base) + println();
}

size_t UARTEStream::println(double num, int digits) {
    return print(num, digits) + println();
}

void UARTEStream::_event_handler(nrfx_uarte_event_t const *p_event, void *p_context) {
  UARTEStream* self = static_cast<UARTEStream*>(p_context);
  
    if (p_event->type == NRFX_UARTE_EVT_RX_DONE) {
        size_t length = p_event->data.rxtx.bytes;
        if(length > 0){
          self->_move_to_ring_buffer(length);
        }

    if(self->_rx_chunk_size == 1){
      nrfx_uarte_rx(&_uarte, _rx_buffer, self->_rx_chunk_size);
    }
    }
}

