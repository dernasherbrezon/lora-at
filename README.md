# About

Control LoRa via serial interface using AT commands. Baud rate: 115200.

# Supported commands

All request commands must start with "AT". All responses must end with "OK" or "ERROR". If command returned "ERROR" then the message before contained the error reason.

Sometimes response might contain lines with "[I]" or "[E]". This is lora-at internal logging. These messages can be ignored.

<table>
    <thead>
        <tr>
            <th>Name</th>
            <th>Format/Example</th>
            <th>Description</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td rowspan="2">Check status</td>
            <td>AT</td>
            <td rowspan="2">Just check the status</td>
        </tr>
        <tr>
            <td>AT<br>OK</td>
        </tr>
        <tr>
            <td rowspan="2">Get firmware version</td>
            <td>AT+GMR</td>
            <td rowspan="2">The returned version is in format: major.minor</td>
        </tr>
        <tr>
            <td>AT+GMR<br>1.0<br>OK</td>
        </tr>
        <tr>
            <td rowspan="2">Get list of all supported chips</td>
            <td>AT+CHIPS?</td>
            <td rowspan="2">Each line contains chip information: &lt;index&gt;.&lt;chip name&gt;</td>
        </tr>
        <tr>
            <td>AT+CHIPS?<br>0,TTGO - 433/470Mhz<br>1,TTGO - 868/915Mhz<br>OK</td>
        </tr>
        <tr>
            <td rowspan="2">Setup chip</td>
            <td>AT+CHIP=&lt;index&gt;</td>
            <td rowspan="2">Set the chip to use. The index can be obtained using "AT+CHIPS?" command. Selected index will be saved into persistent memory and will survive the board restart</td>
        </tr>
        <tr>
            <td>AT+CHIP=0<br>OK</td>
        </tr>
        <tr>
            <td rowspan="2">Get chip</td>
            <td>AT+CHIP?</td>
            <td rowspan="2">Get information about currently selected chip.</td>
        </tr>
        <tr>
            <td>AT+CHIP?<br>TTGO - 868/915Mhz<br>LORA,863,928<br>FSK,863,928<br>OK</td>
        </tr>
        <tr>
            <td rowspan="2">Start LoRa RX</td>
            <td>AT+LORARX=freq,bw,sf,cr,syncword,power,preambleLength,gain,ldro</td>
            <td rowspan="2">Start LoRa receiver. All messages will be received asynchronously and stored in the memory. They can be retrived by a separate command AT+PULL or AT+STOPRX. ldro can be one of the following:
            <ul>
                <li>0 - LDRO_AUTO</li>
                <li>1 - LDRO_ON. Force ldro ON</li>
                <li>2 - LDRO_OFF. Force ldro OFF</li>
            </ul>
            </td>
        </tr>
        <tr>
            <td>AT+LORARX=433.0,10.4,9,6,18,10,55,0,0<br>OK</td>
        </tr>
        <tr>
            <td rowspan="2">Get received messages</td>
            <td>AT+PULL</td>
            <td rowspan="2">Get all currently received messages. Each line will contain received message in the following format: &lt;hexadecimal string of received data&gt;,&lt;RSSI&gt;,&lt;SNR&gt;,&lt;Frequency error&gt;,&lt;UNIX timestamp&gt;. After executing this command the internal in-memory storage will be cleared. This command should be executed for heavy traffic usages in order to keep memory usage within certain limits.</td>
        </tr>
        <tr>
            <td>AT+PULL<br>CAFE,-11.22,3.2,13.2,1605980902<br>OK</td>
        </tr>
        <tr>
            <td rowspan="2">Stop receiving data</td>
            <td>AT+STOPRX</td>
            <td rowspan="2">Stop receiving data and return all pending messages in the in-memory storage. The format matches AT+PULL command</td>
        </tr>
        <tr>
            <td>AT+STOPRX<br>CAFE,-11.22,3.2,13.2,1605980902<br>OK</td>
        </tr>
        <tr>
            <td rowspan="2">Send data</td>
            <td>AT+LORATX=hexadecimal string of data to send,freq,bw,sf,cr,syncword,power,preambleLength,gain,ldro</td>
            <td rowspan="2">Send the data using LoRa</td>
        </tr>
        <tr>
            <td>AT+LORATX=CAFE,433.0,10.4,9,6,18,10,55,0,0<br>OK</td>
        </tr>
        <tr>
            <td rowspan="2">Get current time</td>
            <td>AT+TIME?</td>
            <td rowspan="2">The board doesn't have access to NTP server so it needs externally provided time. AT+TIME? and AT+TIME commands can be used for that. The command will return currently configured time on board in the following format: yyyy-MM-dd HH:mm:ss</td>
        </tr>
        <tr>
            <td>AT+TIME?<br>2022-04-11 12:26:31<br>OK</td>
        </tr>
        <tr>
            <td rowspan="2">Set current time</td>
            <td>AT+TIME=unixtime</td>
            <td rowspan="2">Set current time on board. Can be called periodically to ensure time is not drifted too much</td>
        </tr>
        <tr>
            <td>AT+TIME=1649679986<br>OK</td>
        </tr>
        <tr>
            <td rowspan="2">Get display status</td>
            <td>AT+DISPLAY?</td>
            <td rowspan="2">The command will return 1 if display enabled, 0 - otherwise</td>
        </tr>
        <tr>
            <td>AT+DISPLAY?<br>1<br>OK</td>
        </tr>
        <tr>
            <td rowspan="2">Set display status</td>
            <td>AT+DISPLAY=status</td>
            <td rowspan="2">This command can be used to toggle display. status=1 - enable display, status=0 - disable display. Can be used to reduce power consumption</td>
        </tr>
        <tr>
            <td>AT+DISPLAY=1<br>OK</td>
        </tr>
    </tbody>
</table>