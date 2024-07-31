#include <core3.h>
#include <core3_can.h>
#include <ecumaster.h>
#include <core3_gmlan.h>

#include "driver/gpio.h"
#include "driver/twai.h"

bool core3_can_decode_gmlan_frame(core3_can_msg *frame, vehicle_data* veh_data)
{
    if (veh_data == NULL || frame == NULL)
        return false;



    return false;
}
