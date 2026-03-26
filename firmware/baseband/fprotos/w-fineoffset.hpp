// SPDX-License-Identifier: GPL-2.0-or-later
// Based on rtl_433 fineoffset.c by merbanan et al.
// Fine Offset Electronics WH2, WH5 and compatible
// Protocol: OOK PPM, 36 bits, 433.92 MHz

#pragma once
#include "weatherproto.hpp"

class FProtoWeatherFineOffset : public FProtoWeatherBase {
public:
    const char* name = "FineOffset";

    // OOK PPM timing (from rtl_433):
    //   Short gap (0 bit): ~1000 µs
    //   Long gap (1 bit):  ~2000 µs
    //   Pulse width:       ~500 µs
    //   Preamble gap:      ~4000 µs
    static constexpr uint16_t PULSE_SHORT_MIN = 300;
    static constexpr uint16_t PULSE_SHORT_MAX = 800;
    static constexpr uint16_t PULSE_LONG_MIN  = 900;
    static constexpr uint16_t PULSE_LONG_MAX  = 2200;
    static constexpr uint8_t  EXPECTED_BITS   = 36;

    bool decode(const uint8_t* b, uint8_t num_bytes) override {
        // Packet layout (WH2/WH5, 36 bits = 4.5 bytes):
        // AAAA AAAA  BBBB CCCC  DDDD DDDD  DDDD EEEE  EEEE FFFF
        // A: 8-bit device ID (changes on battery replace)
        // B: 4-bit flags: bit3=battery_ok, bit2=tx_mode, bit1-0=channel
        // C: 4-bit high temp nibble
        // D: 8-bit low temp byte  (temp = (C<<8|D) - 400) / 10.0 °C
        // E: 8-bit humidity %
        // F: 4-bit CRC (nibble, poly 0x31 style)

        if (num_bytes < 5) return false;

        uint8_t id        = b[0];
        uint8_t flags     = b[1] >> 4;
        bool    bat_ok    = (flags >> 3) & 1;
        uint8_t channel   = flags & 0x3;
        int16_t temp_raw  = ((b[1] & 0x0F) << 8) | b[2];
        uint8_t humidity  = b[3];
        uint8_t crc_recv  = b[4] >> 4;

        // Validate: humidity must be 0-100, temp plausible
        if (humidity > 100) return false;
        if (temp_raw > 600 || temp_raw < 0) return false;  // −40 to +60 °C

        // CRC check (simple nibble checksum — matches rtl_433 behavior)
        uint8_t crc_calc = (b[0] + b[1] + b[2] + b[3]) & 0x0F;
        if (crc_calc != crc_recv) return false;

        // Populate common fields
        this->id          = id;
        this->channel     = channel;
        this->battery_low = !bat_ok;
        this->temp        = (temp_raw - 400) / 10.0f;
        this->humidity    = humidity;

        return true;
    }
};
