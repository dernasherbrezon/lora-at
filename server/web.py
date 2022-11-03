from http.server import BaseHTTPRequestHandler
from urllib.parse import urlparse
from urllib.parse import parse_qs
import json
import logging
from configuration import ObservationRequest


class ScheduleHandler(BaseHTTPRequestHandler):

    config = None
    filename = None

    def do_POST(self):
        parsed = urlparse(self.path)
        params = parse_qs(parsed.query)
        if parsed.path == '/api/v1/schedule':
            if 'client' not in params:
                self.sendStatus(400, "'client' parameter is missing")
                return
            if self.config.hasClient(params['client']) == False:
                self.sendStatus(400, "unknown client address")
                return

            content_len = int(self.headers.get('Content-Length'))
            parsed = json.load(self.rfile.read(content_len))
            schedule = set()
            for cur in parsed:
                schedule.add(ObservationRequest(cur))

            self.config.setSchedule(params['client'], schedule)
            self.sendStatus(200)
        elif parsed.path == '/api/v1/client':
            if 'client' not in params:
                self.sendStatus(400, "'client' parameter is missing")
                return
            if 'minFreq' not in params:
                self.sendStatus(400, "'minFreq' parameter is missing")
                return
            if 'maxFreq' not in params:
                self.sendStatus(400, "'maxFreq' parameter is missing")
                return
            self.config.addClient(params['client'], params['minFreq'], params['maxFreq'])
            with open(self.filename, "w") as outfile:
                outfile.write(json.dumps(self.config, default=lambda x: x.__dict__))
            self.sendStatus(200)
        else:
            self.sendStatus(404)
            return

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == '/api/v1/status':
            self.send_response(200)
            self.send_header("Content-type", "application/json")
            self.end_headers()
            clients = []
            for key in self.config.getClients():
                clients.append(self.config.getClients()[key].__dict__)
            self.wfile.write(bytes(json.dumps(clients), "utf-8"))
        elif parsed.path == '/api/v1/data':
            params = parse_qs(parsed.query)
            if 'client' not in params:
                self.sendStatus(400, "'client' parameter is missing")
                return
            if self.config.hasClient(params['client']) == False:
                self.sendStatus(400, "unknown client address")
                return
            frames = []
            client = self.config.getClients()[params['client']]
            if client != None and client.getFrames() != None:
                for cur in client.getFrames():
                    frames.append(cur.__dict__)
                # reset frames to empty once returned via API
                client.setFrames([])
            self.wfile.write(bytes(json.dumps(frames), "utf-8"))
        else:
            self.sendStatus(404)
            return


    def sendStatus(self, status, message = None):
        self.send_response(status)
        self.send_header("Content-type", "application/json")
        self.end_headers()
        response = {}
        response["status"] = status
        if message != None:
            logging.info(message)
            response["message"] = message
        self.wfile.write(bytes(json.dumps(response), "utf-8"))
