import pytest
from pytest_embedded_serial import SerialDut
from typing import Tuple

# @pytest.mark.supported_targets
@pytest.mark.parametrize('count', [
    2,
], indirect=True)
def test_common_at_commands(dut: Tuple[SerialDut, SerialDut]) -> None:
    dut_tx = dut[0]
    dut_rx = dut[1]

    dut_rx.expect('lora-at initialized', timeout=3)
    dut_rx.write('AT')
    dut_rx.expect('OK', timeout=3)
    dut_rx.write('AT+GMR')
    dut_rx.expect('2.0', timeout=3)
    dut_rx.expect('OK', timeout=3)
    dut_rx.write('AT+DISPLAY=1')
    dut_rx.expect('OK', timeout=3)
    dut_rx.write('AT+DISPLAY?')
    dut_rx.expect('1', timeout=3)
    dut_rx.expect('OK', timeout=3)
    dut_rx.write('AT+TIME=1628233187224')
    dut_rx.expect('OK', timeout=3)
    dut_rx.write('AT+TIME?')
    # skip actual time as it will tick as soon as it set
    # just make sure time query works fine
    dut_rx.expect('OK', timeout=3)
    # default frequencies. taken from sdkconfig.defaults
    dut_rx.write('AT+MINFREQ?')
    dut_rx.expect('25000000', timeout=3)
    dut_rx.expect('OK', timeout=3)
    dut_rx.write('AT+MAXFREQ?')
    dut_rx.expect('1700000000', timeout=3)
    dut_rx.expect('OK', timeout=3)
    # set empty bluetooth (disable it)
    dut_rx.write('AT+BLUETOOTH=')
    dut_rx.expect('OK', timeout=3)
    dut_rx.write('AT+BLUETOOTH?')
    # it will contain actual address after the pattern
    dut_rx.expect('LoraAt', timeout=3)
    dut_rx.expect('OK', timeout=3)
    dut_rx.write('AT+BLUETOOTH=00:00:00:00:00:00')
    dut_rx.expect('ERROR', timeout=40)
    dut_rx.write('AT+BLUETOOTH=00')
    dut_rx.expect('ERROR', timeout=10)
    # test rx
    dut_rx.write('AT+LORARX=436703003,250000,10,5,18,10,8,4,1,1,1,0')
    dut_rx.expect('OK', timeout=3)
    dut_tx.write('AT+LORATX=CAFE,436703003,250000,10,5,18,10,8,4,1,1,1,0')
    dut_tx.expect('OK', timeout=3)
    dut_rx.expect('received frame', timeout=3)
    dut_rx.write('AT+PULL')
    dut_rx.expect('CAFE', timeout=3)
    dut_rx.expect('OK', timeout=3)
    # test frames returned on stop
    dut_tx.write('AT+LORATX=CAFE,436703003,250000,10,5,18,10,8,4,1,1,1,0')
    dut_tx.expect('OK', timeout=3)
    dut_rx.write('AT+STOPRX')
    dut_rx.expect('CAFE', timeout=3)
    dut_rx.expect('OK', timeout=3)
    # test FSK mode
    dut_rx.write('AT+FSKRX=437200012,4800,5000,4,12AD,0,2,1,4,5000,20000')
    dut_rx.expect('OK', timeout=3)
    dut_tx.write('AT+FSKTX=10CAFE,437200012,4800,5000,4,12AD,0,2,1,4,0,0')
    dut_tx.expect('OK', timeout=3)
    dut_rx.expect('received frame', timeout=3)
    dut_rx.write('AT+STOPRX')
    dut_rx.expect('10CAFE', timeout=3)
    dut_rx.expect('OK', timeout=3)







