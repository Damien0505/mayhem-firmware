#ifndef __FPROTO_FINEOFFSET_WH2_H__
#define __FPROTO_FINEOFFSET_WH2_H__

#include "weatherbase.hpp"

typedef enum {
    FineOffsetWH2DecoderStepReset = 0,
    FineOffsetWH2DecoderStepSaveDuration,
    FineOffsetWH2DecoderStepCheckDuration,
} FineOffsetWH2DecoderStep;

class FProtoWeatherFineOffsetWH2 : public FProtoWeatherBase {
   public:
    FProtoWeatherFineOffsetWH2() {
        sensorType = FPW_FineOffsetWH2;
    }

    void feed(bool level, uint32_t duration) override {
        switch (parser_step) {
            case FineOffsetWH2DecoderStepReset:
                // Sync: long LOW gap ~9000us (18 x te_short)
                if ((!level) && (DURATION_DIFF(duration, te_short * 18) < te_delta * 8)) {
                    parser_step = FineOffsetWH2DecoderStepSaveDuration;
                    decode_data = 0;
                    decode_count_bit = 0;
                }
                break;

            case FineOffsetWH2DecoderStepSaveDuration:
                if (level) {
                    te_last = duration;
                    parser_step = FineOffsetWH2DecoderStepCheckDuration;
                } else {
                    parser_step = FineOffsetWH2DecoderStepReset;
                }
                break;

            case FineOffsetWH2DecoderStepCheckDuration:
                if (!level) {
                    // Sync postfix — end of packet
                    if (DURATION_DIFF(duration, te_short * 18) < te_delta * 8) {
                        if ((decode_count_bit == min_count_bit_for_found) &&
                            fineoffset_wh2_check()) {
                            if (callback) callback(this);
                        }
                        decode_data = 0;
                        decode_count_bit = 0;
                        parser_step = FineOffsetWH2DecoderStepSaveDuration;
                        break;
                    }
                    // Bit 0: short pulse (~500us) + short gap (~1000us)
                    if ((DURATION_DIFF(te_last, te_short) < te_delta) &&
                        (DURATION_DIFF(duration, te_short * 2) < te_delta * 2)) {
                        subghz_protocol_blocks_add_bit(0);
                        parser_step = FineOffsetWH2DecoderStepSaveDuration;
                    }
                    // Bit 1: short pulse (~500us) + long gap (~2000us)
                    else if ((DURATION_DIFF(te_last, te_short) < te_delta) &&
                             (DURATION_DIFF(duration, te_short * 4) < te_delta * 4)) {
                        subghz_protocol_blocks_add_bit(1);
                        parser_step = FineOffsetWH2DecoderStepSaveDuration;
                    } else {
                        parser_step = FineOffsetWH2DecoderStepReset;
                    }
                } else {
                    parser_step = FineOffsetWH2DecoderStepReset;
                }
                break;
        }
    }

   protected:
    // WH2/WH3/WH5: ~500us pulse, ~1000us short gap, ~2000us long gap
    uint32_t te_short = 500;
    uint32_t te_long = 2000;
    uint32_t te_delta = 150;
    uint32_t min_count_bit_for_found = 36;

    // Packet layout (36 bits):
    // [35:28] ID       8 bits  - random, changes on battery replace
    // [27]    battery  1 bit   - 1=OK, 0=LOW
    // [26:25] channel  2 bits  - 00=ch1, 01=ch2, 10=ch3
    // [24]    sign     1 bit   - 0=positive, 1=negative temp
    // [23:12] temp    12 bits  - temp * 10, offset 0 (not 400 like Kedsum)
    // [11:4]  humid    8 bits  - humidity %
    // [3:0]   CRC      4 bits  - CRC-4, poly 0x3, init 0x0
    bool fineoffset_wh2_check() {
        if (!decode_data) return false;
        uint8_t msg[4] = {
            static_cast<uint8_t>(decode_data >> 28),
            static_cast<uint8_t>(decode_data >> 20),
            static_cast<uint8_t>(decode_data >> 12),
            static_cast<uint8_t>(decode_data >> 4),
        };
        uint8_t crc = FProtoGeneral::subghz_protocol_blocks_crc4(msg, 4, 0x03, 0x00);
        return (crc == (decode_data & 0x0F));
    }
};

#endif