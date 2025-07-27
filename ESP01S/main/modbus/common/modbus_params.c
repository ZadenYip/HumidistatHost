/*=====================================================================================
 * Description:
 *   C file to define parameter storage instances
 *====================================================================================*/
#include <stdint.h>
#include "modbus_params.h"

// Here are the user defined instances for device parameters packed by 1 byte
// These are keep the values that can be accessed from Modbus master
holding_reg_params_t holding_reg_params = { 0 };
