from zeroconf import ServiceBrowser, ServiceListener, Zeroconf, IPVersion
from pymodbus.pdu import ModbusPDU
from pymodbus.client import ModbusTcpClient
import time
esp01s_address = None

def read_input_registers(client: ModbusTcpClient, address: int, count: int) -> list[int]:
    try:
        response: ModbusPDU = client.read_input_registers(address, count=count)
        if response.isError():
            print(f"Error reading input registers: {response}")
            return [None, None]
        else:
            print(f"Input Registers at {address}: {response.registers}")
            return response.registers
    except Exception as e:
        print(f"Exception occurred: {e}")
        return [None, None]

def main() -> None:
    while True:
        client = ModbusTcpClient(esp01s_address)
        client.connect()
        temperature = float()
        humidity = float()
        temperature, humidity = read_input_registers(client, 0, 2)
        print(f"Temperature: {temperature}, Humidity: {humidity}")
        client.close()
        time.sleep(2)
    

class MyListener(ServiceListener):

    def update_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        #print(f"Service {name} updated")
        return

    def remove_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        #print(f"Service {name} removed")
        return

    def add_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        info = zc.get_service_info(type_, name)
        # 获取esp8266的类型和名称
        if name == "humidistat-esp8266._modbus._tcp.local.":
            global esp01s_address
            esp01s_address = info.parsed_addresses(IPVersion.V4Only)[0]



zeroconf = Zeroconf()
listener = MyListener()
browser = ServiceBrowser(zeroconf, "_modbus._tcp.local.", listener)
try:
    while esp01s_address is None:
        time.sleep(1)
    main()
finally:
    zeroconf.close()