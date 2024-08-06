#pragma once

#if defined(__cplusplus)
extern "C"
{
#endif

    typedef enum
    {
        CORE3_CAN_MODE_NORMAL = 0,
        CORE3_CAN_MODE_NO_ACK = 1,
        CORE3_CAN_MODE_LISTEN = 2
    } core3_can_mode;

    typedef enum
    {
        CORE3_CAN_TIMING_25KBPS,
        CORE3_CAN_TIMING_33_3KBPS,
        CORE3_CAN_TIMING_50KBPS,
        CORE3_CAN_TIMING_100KBPS,
        CORE3_CAN_TIMING_125KBPS,
        CORE3_CAN_TIMING_250KBPS,
        CORE3_CAN_TIMING_500KBPS,
        CORE3_CAN_TIMING_800KBPS,
        CORE3_CAN_TIMING_1MBPS,
    } core3_can_timing;

    typedef struct
    {
        struct
        {
            // The order of these bits must match deprecated message flags for compatibility reasons
            uint32_t extd : 1;         /**< Extended Frame Format (29bit ID) */
            uint32_t rtr : 1;          /**< Message is a Remote Frame */
            uint32_t ss : 1;           /**< Transmit as a Single Shot Transmission. Unused for received. */
            uint32_t self : 1;         /**< Transmit as a Self Reception Request. Unused for received. */
            uint32_t dlc_non_comp : 1; /**< Message's Data length code is larger than 8. This will break compliance with ISO 11898-1 */
            uint32_t reserved : 27;    /**< Reserved bits */
        };

        uint32_t identifier;      /**< 11 or 29 bit identifier */
        uint8_t data_length_code; /**< Data length code */
        uint8_t data[8];          /**< Data bytes (not relevant in RTR frame) */
    } core3_can_msg;

    int core3_can_init(core3_can_timing timing, core3_can_mode mode);
    bool core3_can_receive(core3_can_msg *msg);
    bool core3_can_send(core3_can_msg *msg);

#if defined(__cplusplus)
}
#endif