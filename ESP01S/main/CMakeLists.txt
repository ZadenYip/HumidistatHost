idf_component_register(SRCS "app_main.c" 
                            "app_uart.c"
                            "wifi/wifi_module.c"
                            "mdns/mdns_service.c"
                            "modbus/common/modbus_params.c"
                            "modbus/tcp/tcp_slave.c"
                    INCLUDE_DIRS "." "wifi" "mdns" "modbus"
                    )