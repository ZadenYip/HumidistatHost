/*=====================================================================================
 * Description:
 *   The Modbus parameter structures used to define Modbus instances that
 *   can be addressed by Modbus protocol. Define these structures per your needs in
 *   your application. Below is just an example of possible parameters.
 *====================================================================================*/
#ifndef _DEVICE_PARAMS
#define _DEVICE_PARAMS

// This file defines structure of modbus parameters which reflect correspond modbus address space
// for each modbus register type (coils, discreet inputs, holding registers, input registers)
#include <stdint.h>

#pragma pack(push, 1)
typedef struct
{
    float humidity;
    float temperature;
} input_reg_params_t;
#pragma pack(pop)

extern input_reg_params_t input_reg_params;

#endif // !defined(_DEVICE_PARAMS)
