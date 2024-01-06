import requests
from requests.auth import HTTPBasicAuth


class AtRestClient:

    def __init__(self, baseurl, user, password):
        self.baseurl = baseurl
        self.user = user
        self.password = password

    def getStatus(self):
        return requests.get('http://' + self.baseurl + '/api/v2/status', auth=HTTPBasicAuth(self.user, self.password))

    def startLoRaRx(self, payload):
        return requests.post('http://' + self.baseurl + '/api/v2/lora/rx/start', json = payload, auth=HTTPBasicAuth(self.user, self.password))

    def loRaTx(self, payload):
        return requests.post('http://' + self.baseurl + '/api/v2/lora/tx', json = payload, auth=HTTPBasicAuth(self.user, self.password))

    def startFskRx(self, payload):
        return requests.post('http://' + self.baseurl + '/api/v2/fsk/rx/start', json = payload, auth=HTTPBasicAuth(self.user, self.password))

    def fskTx(self, payload):
        return requests.post('http://' + self.baseurl + '/api/v2/fsk/tx', json = payload, auth=HTTPBasicAuth(self.user, self.password))

    def stopRx(self):
        payload = {
            ## empty
        }
        return requests.post('http://' + self.baseurl + '/api/v2/rx/stop', json = payload, auth=HTTPBasicAuth(self.user, self.password))