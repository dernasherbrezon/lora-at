
import json
import logging
from datetime import datetime

class LoraFrame:

    def __init__(self):
        self.frequencyError = 0.0
        self.rssi = 0.0
        self.snr = 0.0
        self.timestamp = 0
        self.data = []

class ObservationRequest:

    def __init__(self, json):
        self.startTimeMillis = json["startTimeMillis"]
        self.endTimeMillis = json["endTimeMillis"]
        # currentTimeMillis
        self.freq = json["freq"]
        self.bw = json["bw"]
        self.sf = json["sf"]
        self.cr = json["cr"]
        self.syncWord = json["syncWord"]
        self.power = json["power"]
        self.preambleLength = json["preambleLength"]
        self.gain = json["gain"]
        self.ldro = json["ldro"]

    def __str__(self):
        return str(self.__dict__)

class ClientConfiguration:

    def __init__(self, btaddress, minFrequency, maxFrequency, schedule):
        self.btaddress = btaddress
        self.minFrequency = minFrequency
        self.maxFrequency = maxFrequency
        self.schedule = schedule
        self.batteryLevel = None
        self.frames = []

    @classmethod
    def fromJson(cls, json):
        return cls(json["btaddress"], json["minFrequency"], json["maxFrequency"], json["schedule"])

    def setSchedule(self, schedule):
        logging.info('new schedule for ' + self.btaddress)
        self.schedule = sorted(schedule, key=lambda x: x.startTimeMillis)
    
    def getFrames(self):
        return self.frames

    def setFrames(self, frames):
        self.frames = frames

    def findNextObservation(self):
        if self.schedule == None:
            return None
        currentTime = int(datetime.utcnow().timestamp() * 1000)
        # requests are sorted startTimeMillis asc
        # return first found
        for cur in self.schedule:
            if cur["startTimeMillis"] > currentTime:
                cur["currentTimeMillis"] = currentTime
                return cur

        return None

    def addFrame(self, frame):
        self.frame.append(frame)

    def setBatteryLevel(self, level):
        self.batteryLevel = level

    def __str__(self):
        return self.btaddress


class Configuration:

    def __init__(self, filename):
        f = open(filename)
        data = json.load(f)
        f.close()

        self.hostname = data["hostname"]
        self.port = data["port"]
        self.btname = data["btname"]
        self.clients = {}
        if "clients" in data:
            for i in data["clients"]:
                self.clients[i] = ClientConfiguration.fromJson(data["clients"][i])

    def getHostname(self):
        return self.hostname

    def getPort(self):
        return self.port

    def getBtName(self):
        return self.btname

    def hasClient(self, btClientAddress):
        return btClientAddress in self.clients

    def setSchedule(self, btaddress, schedule):
        client = self.clients[btaddress]
        if client == None:
            logging.info('unknown client')
            return
        client.setSchedule(schedule)

    def getClients(self):
        return self.clients

    def addClient(self, btaddress, minFreq, maxFreq):
        self.clients[btaddress] = ClientConfiguration(btaddress, minFreq, maxFreq, [])
