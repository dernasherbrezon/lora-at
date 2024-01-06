import pytest
from AtRestClient import AtRestClient

lora_rx = {
    "freq": 437200000,
    "bw": 125000,
    "sf": 9,
    "cr": 5,
    "syncWord": 18,
    "preambleLength": 8,
    "ldo": 0,
    "useCrc": 1,
    "useExplicitHeader": 1,
    "length": 0,
    "gain": 6
}

lora_tx = {
    "freq": 437200000,
    "bw": 125000,
    "sf": 9,
    "cr": 5,
    "syncWord": 18,
    "preambleLength": 8,
    "ldo": 0,
    "useCrc": 1,
    "useExplicitHeader": 1,
    "length": 0,
    "power": 4,
    "ocp": 240,
    "pin": 1,
    "data": "cafe"
}

fsk_rx = {
    "freq": 437200000,
    "bitrate": 4800,
    "freqDeviation": 5000,
    "preamble": 8,
    "syncword": "12AD",
    "encoding": 0,
    "dataShaping": 2,
    "crc": 1,
    "rxBandwidth": 5000,
    "rxAfcBandwidth": 10000
}

fsk_tx = {
    "freq": 437200000,
    "bitrate": 4800,
    "freqDeviation": 5000,
    "preamble": 8,
    "syncword": "12AD",
    "encoding": 0,
    "dataShaping": 2,
    "crc": 1,
    "power": 10,
    "ocp": 240,
    "pin": 1,
    "data": "cafe"
}

expected_message = {
    "status": "SUCCESS",
    "frames": [
        {
            "data": "CAFE",
            "rssi": -12,
            "snr": 12,
            "frequencyError": 1234,
            "timestamp": 1234567788
        }
    ]
}

expected_status = {
    "status": "SUCCESS",
    "minFreq": 25000000,
    "maxFreq": 1700000000
}


def test_lora_rx_tx() -> None:
    client0 = AtRestClient('lora-at-0.local', 'r2lora', 'password')
    status0 = client0.getStatus()
    assert status0.status_code == 200
    assert compare_objects(expected_status, status0.json())
    client1 = AtRestClient('lora-at-1.local', 'r2lora', 'password')
    status1 = client1.getStatus()
    assert status1.status_code == 200
    assert compare_objects(expected_status, status1.json())

    status0 = client0.startLoRaRx(lora_rx)
    assert status0.status_code == 200

    status1 = client1.loRaTx(lora_tx)
    assert status1.status_code == 200

    status0 = client0.stopRx()
    assert status0.status_code == 200
    assert compare_objects(expected_message, status0.json(), ignore_fields=["rssi", "snr", "frequencyError", "timestamp"])

def test_fsk_rx_tx() -> None:
    client0 = AtRestClient('lora-at-0.local', 'r2lora', 'password')
    status0 = client0.getStatus()
    assert status0.status_code == 200
    assert compare_objects(expected_status, status0.json())
    client1 = AtRestClient('lora-at-1.local', 'r2lora', 'password')
    status1 = client1.getStatus()
    assert status1.status_code == 200
    assert compare_objects(expected_status, status1.json())

    status0 = client0.startFskRx(fsk_rx)
    assert status0.status_code == 200

    status1 = client1.fskTx(fsk_tx)
    assert status1.status_code == 200

    status0 = client0.stopRx()
    assert status0.status_code == 200
    assert compare_objects(expected_message, status0.json(), ignore_fields=["rssi", "snr", "frequencyError", "timestamp"])


def compare_objects(obj1, obj2, ignore_fields=[]):
    """
    Compare two dictionaries while recursively ignoring specified fields.

    Args:
    - obj1 (dict or list): First dictionary or list.
    - obj2 (dict or list): Second dictionary or list.
    - ignore_fields (list): List of field names to be ignored during the comparison.

    Returns:
    - bool: True if the objects are equal, False otherwise.
    """

    if isinstance(obj1, dict) and isinstance(obj2, dict):
        for field in ignore_fields:
            obj1.pop(field, None)
            obj2.pop(field, None)

        if set(obj1.keys()) != set(obj2.keys()):
            return False

        for key in obj1:
            if obj1[key] != obj2[key]:
                if isinstance(obj1[key], (dict, list)) and isinstance(obj2[key], (dict, list)):
                    # Recursive call for nested dictionaries or lists
                    if not compare_objects(obj1[key], obj2[key], ignore_fields):
                        return False
                else:
                    return False

        return True

    elif isinstance(obj1, list) and isinstance(obj2, list):
        if len(obj1) != len(obj2):
            return False

        for item1, item2 in zip(obj1, obj2):
            if isinstance(item1, (dict, list)) and isinstance(item2, (dict, list)):
                # Recursive call for items that are dictionaries or lists
                if not compare_objects(item1, item2, ignore_fields):
                    return False
            elif item1 != item2:
                return False

        return True

    else:
        return obj1 == obj2
