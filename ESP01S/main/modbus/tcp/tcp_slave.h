#ifndef MODBUS_TCP_SLAVE_H
#define MODBUS_TCP_SLAVE_H
void modbus_deinit(void);
void modbus_init(void);
void modbus_update_temp_and_humi(float temperature, float humidity);

#endif // MODBUS_TCP_SLAVE_H