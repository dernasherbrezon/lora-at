#!/usr/bin/env python3

import gatt
import web
import configuration
import sys
from http.server import HTTPServer


def main():

    config = configuration.Configuration(sys.argv[1])

    gattServer = gatt.GattServer(config)
    gattServer.start()

    web.ScheduleHandler.config = config
    web.ScheduleHandler.filename = sys.argv[1]
    webServer = HTTPServer((config.getHostname(), config.getPort()), web.ScheduleHandler)
    print('starting web server on %s:%s' % (config.getHostname(), config.getPort()))
    try:
        webServer.serve_forever()
    except(KeyboardInterrupt):
        gattServer.stop()

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("invalid number of arguments. Expected: main.py configuration.json")
        sys.exit(1)
    main()
