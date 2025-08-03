import asyncio
import logging
import re
import numpy as np
from asyncio import CancelledError
from bleak import BleakClient
from bleak import BleakScanner
from bleak import BleakGATTCharacteristic
from bleak.exc import BleakDeviceNotFoundError


logger = logging.getLogger(__name__)
address = "48:87:2D:73:E1:46" # BT24 MAC
SERVICE_UUID = "0000FFE0-0000-1000-8000-00805F9B34FB"
NOTIFY_WRITE_UUID = "0000FFE1-0000-1000-8000-00805F9B34FB"
WRITE_UUID = "0000FFE2-0000-1000-8000-00805F9B34FB"
MODEL_NBR_UUID = "2A24"

last_sent_msg = bytearray(0)

def get_checksum(data: bytearray, need_line_end: bool = True) -> np.uint8:
    checksum = np.uint8(0)
    for byte in data:
        checksum += byte
    if need_line_end:
        checksum += ord("\r")  # 添加回车符的校验
        checksum += ord("\n")  # 添加换行符的
    return ~checksum

def encode_msg(expr: str) -> bytearray:
    """
    解析形如 0x00 0x06 0x01 "HyFran1" 的字符串：
    - 十六进制（0x开头）部分：转为单字节
    - 引号部分：转为ASCII字节序列
    返回: bytearray
    """
    out = bytearray()
    parts = expr.strip().split(" ")

    for part in parts:
        if part.startswith("0x"):
            try:
                value = int(part, 16)
                out.append(value)
            except ValueError:
                logger.error(f"Invalid hex value: {part}")
        elif re.match(r'"[^"]*"', part):
            trim = part[1:-1]  # 去除引号
            out.extend(trim.encode())
        else:
            logger.warning(f"Unrecognized token: {part}")

    return out

async def discover_devices():
    devices = await BleakScanner.discover(10)
    
    for device in devices:
        print(f"Device Name: {device.name}, Address: {device.address}")
        
async def main():
    logger.info("Starting...")
    await discover_devices()
    device = await BleakScanner.find_device_by_address(address, 10)
    if device is None:
        logger.error(f"Device with address {address} not found.")
        return
    else:
        logger.info(f"Device found: {device}")
        
        
    def disconnected_callback(client):
        logger.info("Disconnected from device.")
    async with BleakClient(device, disconnected_callback) as client:
        async def handle_notification(sender: BleakGATTCharacteristic, data: bytearray):
            logger.info(f"Notification from {sender}: {data}")
            msg = data.decode()
            print(f"Received message: {msg}")
            if msg == "NAK\r\n":
                await client.write_gatt_char(WRITE_UUID, last_sent_msg)
                
        def notify_callback(sender, data):
            asyncio.create_task(handle_notification(sender, data))
            
            
        await client.start_notify(NOTIFY_WRITE_UUID, notify_callback)
        loop = asyncio.get_event_loop()
    
        while True:
            try:
                user_input = str(await loop.run_in_executor(None, input, "Enter command: \r\n"))
                encoded = encode_msg(user_input)
                checksum_value = get_checksum(encoded)
                
                # 0xNN 0xNN 0xNN "string"
                # 012345
                tail = (" " + user_input[5:]) if len(user_input) > 6 else None
                user_input = user_input[:4] + " " + hex(checksum_value)
                if tail != None:
                    user_input += tail
                encoded = encode_msg(user_input)
                last_sent_msg = encoded + b"\r\n"
                await client.write_gatt_char(WRITE_UUID, last_sent_msg)
            except (asyncio.CancelledError, KeyboardInterrupt):
                logger.info("Disconnecting...")
                break

async def stay_connected():
    client = BleakClient(address)
    await client.connect()

asyncio.run(main())