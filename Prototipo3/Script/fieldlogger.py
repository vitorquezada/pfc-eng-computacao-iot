"""
Script responsável por sincronizar dados do FieldLogger, da bancada de termodinâmica do prédio de Engenharia Mecânica - UFG, com a plataforma TagoIO.
Para isto comunica-se utilizando protocolo MQTT.
"""

import json
import socket
import time
from paho.mqtt import client as mqtt_client

HOST = "192.168.21.26"  # The server's hostname or IP address
PORT = 502  # The port used by the server

DEVICE_TAGOIO = {
    'token': '7e2e7399-ccae-470a-9c2a-0de40bc3f208',
    'broker': 'mqtt.tago.io',
    'port': 1883,
    'user': 'Token',
    'client_id': 'Teste 2',
    'topic': "variaveis",
    'subscription_topic': 'inTopic',
}
INTERVAL_SECONDS = 1

FAN_FL_ADDRESS = 14
FAN_FL_ADDRESS_COUNT = 1
FAN_FL_RELE_ADDRESS = 30

CURRENT_FL_ADDRESS = 99
CURRENT_FL_ADDRESS_COUNT = 1
CURRENT_FL_DECIMALS = 4

TEMP_1_FL_ADDRESS = 3
TEMP_1_FL_ADDRESS_COUNT = 1
TEMP_1_FL_DECIMALS = 1

TEMP_2_FL_ADDRESS = 4
TEMP_2_FL_ADDRESS_COUNT = 1
TEMP_2_FL_DECIMALS = 1

TEMP_3_FL_ADDRESS = 5
TEMP_3_FL_ADDRESS_COUNT = 1
TEMP_3_FL_DECIMALS = 1

TEMP_4_FL_ADDRESS = 32
TEMP_4_FL_ADDRESS_COUNT = 1
TEMP_4_FL_DECIMALS = 0


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("MQTT: Connected to MQTT Broker!")
    else:
        print(f"MQTT: Failed to connect, return code {rc}", )


def on_publish(mosq, obj, mid):
    print("MQTT: mid: "+str(mid))


def on_disconnect(client, userdata, rc):
    print(f"MQTT: disonnected with rc {rc}")


def on_message(client, userdata, msg):
    topic = msg.topic
    jsonObj = msg.payload.decode()

    match jsonObj.comando:
        case 'fan':
            command_write_single_coil(FAN_FL_ADDRESS, jsonObj.valor)


def getMqttClient():
    client_id = DEVICE_TAGOIO.get('client_id')
    user = DEVICE_TAGOIO.get('user')
    pw = DEVICE_TAGOIO.get('token')
    broker = DEVICE_TAGOIO.get('broker')
    port = DEVICE_TAGOIO.get('port')
    subscription_topic = DEVICE_TAGOIO.get('subscription_topic')

    client = mqtt_client.Client(client_id)
    client.username_pw_set(user, pw)
    client.on_connect = on_connect
    client.on_publish = on_publish
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    client.connect(broker, port)

    client.subscribe(subscription_topic)

    client.loop_start()

    return client


def send_command_socket(command):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((HOST, PORT))
        s.sendall(bytes(command))
        data = s.recv(1024)
        return data


def command_read_holding_registers(address, count):
    addressBytes = address.to_bytes(2, 'big')
    addressCountBytes = count.to_bytes(2, 'big')

    command = [0x00, 0x00]  # TransactionIdentifier
    command.extend([0x00, 0x00])  # ProtocolIdentifier
    command.extend([0x00, 0x06])  # MessageLength
    command.extend([0xFF])  # DeviceAddress
    command.extend([0x03])  # Functional code
    command.extend(addressBytes)  # Address of the first byte of register
    command.extend(addressCountBytes)  # Number of registers Byte

    data = send_command_socket(command)
    responseBytes = data[8]
    return bytes([x for x in data[-1 * responseBytes:]])


def command_read_coils(address, count, commandText):
    addressBytes = address.to_bytes(2, 'big')
    addressCountBytes = count.to_bytes(2, 'big')

    command = [0x00, 0x00]  # TransactionIdentifier
    command.extend([0x00, 0x00])  # ProtocolIdentifier
    command.extend([0x00, 0x06])  # MessageLength
    command.extend([0xFF])  # DeviceAddress
    command.extend([0x01])  # Functional code
    command.extend(addressBytes)  # Address of the first byte of register
    command.extend(addressCountBytes)  # Number of registers Byte

    data = send_command_socket(command)

    byteResult = data[-1]
    binaryResult = bin(byteResult)[-1]
    value = binaryResult == '1'
    print(f'{commandText}: {value}')
    return value


def command_write_single_coil(address, onOff):
    addressBytes = address.to_bytes(2, 'big')
    valueBytes = [0xFF if onOff else 0x00, 0x00]

    command = [0x00, 0x00]  # TransactionIdentifier
    command.extend([0x00, 0x00])  # ProtocolIdentifier
    command.extend([0x00, 0x06])  # MessageLength
    command.extend([0xFF])  # DeviceAddress
    command.extend([0x05])  # Functional code
    command.extend(addressBytes)  # Address of the first byte of register
    command.extend(valueBytes)  # Number of registers Byte

    data = send_command_socket(command)
    resultHi = data[-2]
    resultLo = data[-1]
    return resultHi == 0xFF and resultLo == 0x00


def GetObject(key, value, serial, unit):
    obj = {
        'variable': key,
        'value': value,
        'serial': serial,
    }
    if unit != '':
        obj['unit'] = unit
    return obj


def main():
    client = getMqttClient()

    while not client.is_connected():
        time.sleep(0.5)

    lastSend = time.time_ns()
    while True:

        current_value_bytes = command_read_holding_registers(
            CURRENT_FL_ADDRESS, CURRENT_FL_ADDRESS_COUNT)
        current_value = int.from_bytes(
            current_value_bytes, 'big') / (10**CURRENT_FL_DECIMALS)
        current = round(current_value, 3)
        print(f'Current: {current} A')

        temp_1_value_bytes = command_read_holding_registers(
            TEMP_1_FL_ADDRESS, TEMP_1_FL_ADDRESS_COUNT)
        temp_1_value = int.from_bytes(
            temp_1_value_bytes, 'big') / (10**TEMP_1_FL_DECIMALS)
        temp_1 = round(temp_1_value, 3)
        print(f'T1: {temp_1} ºC')

        temp_2_value_bytes = command_read_holding_registers(
            TEMP_2_FL_ADDRESS, TEMP_2_FL_ADDRESS_COUNT)
        temp_2_value = int.from_bytes(
            temp_2_value_bytes, 'big') / (10**TEMP_2_FL_DECIMALS)
        temp_2 = round(temp_2_value, 3)
        print(f'T2: {temp_2} ºC')

        temp_3_value_bytes = command_read_holding_registers(
            TEMP_3_FL_ADDRESS, TEMP_3_FL_ADDRESS_COUNT)
        temp_3_value = int.from_bytes(
            temp_3_value_bytes, 'big') / (10**TEMP_3_FL_DECIMALS)
        temp_3 = round(temp_3_value, 3)
        print(f'T3: {temp_3} ºC')

        temp_4_value_bytes = command_read_holding_registers(
            TEMP_4_FL_ADDRESS, TEMP_4_FL_ADDRESS_COUNT)
        temp_4_value = int.from_bytes(
            temp_4_value_bytes, 'big') / (10**TEMP_4_FL_DECIMALS)
        temp_4 = round(temp_4_value, 3)
        print(f'T4: {temp_4} ºC')

        fan_bytes = command_read_holding_registers(
            FAN_FL_ADDRESS, FAN_FL_ADDRESS_COUNT)
        fan = fan_bytes[-1] == 1
        print(f'Fan: {fan}')

        now = time.time_ns()
        nowSeconds = int(now * 1e-9)
        duration = now - lastSend
        lastSend = now

        variables = [
            GetObject('current_rms', current, nowSeconds, 'A'),
            GetObject('resistor', 254, nowSeconds, 'Ohm'),
            GetObject('duration', int(duration * 1e-6), nowSeconds, 'ms'),
            GetObject('fan', fan, nowSeconds, None),
            GetObject('temp_1', temp_1, nowSeconds, 'C'),
            GetObject('temp_2', temp_2, nowSeconds, 'C'),
            GetObject('temp_3', temp_3, nowSeconds, 'C'),
            GetObject('temp_4', temp_4, nowSeconds, 'C'),
        ]

        topic = DEVICE_TAGOIO.get('topic')
        jsonMqtt = json.dumps(variables)
        client.publish(topic, jsonMqtt)

        time.sleep(INTERVAL_SECONDS)


if __name__ == "__main__":
    main()
