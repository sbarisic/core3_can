#pragma once

#if defined(__cplusplus)
extern "C"
{
#endif

    typedef struct
    {
        int RPM;
    } vehicle_data;

    bool core3_can_decode_gmlan_frame(core3_can_msg *frame, vehicle_data *veh_data);

#if defined(__cplusplus)
}
#endif