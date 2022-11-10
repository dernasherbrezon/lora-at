import dbus
import dbus.exceptions
import dbus.mainloop.glib
import dbus.service
import bluez
import configuration
import threading
import logging
import struct
import time
import traceback

try:
  from gi.repository import GObject
except ImportError:
  import gobject as GObject

mainloop = None

LORA_SERVICE_UUID = '3f5f0b4d-e311-4921-b29d-936afb8734cc'
SCHEDULE_CHARACTERISTIC_UUID = '40d6f70c-5e28-4da4-a99e-c5298d1613fe'

class Application(dbus.service.Object):
    """
    org.bluez.GattApplication1 interface implementation
    """
    def __init__(self, config, bus):
        self.path = '/'
        self.services = []
        dbus.service.Object.__init__(self, bus, self.path)
        self.add_service(LoraService(config, bus, 0))

    def get_path(self):
        return dbus.ObjectPath(self.path)

    def add_service(self, service):
        self.services.append(service)

    @dbus.service.method(bluez.DBUS_OM_IFACE, out_signature='a{oa{sa{sv}}}')
    def GetManagedObjects(self):
        response = {}

        for service in self.services:
            response[service.get_path()] = service.get_properties()
            chrcs = service.get_characteristics()
            for chrc in chrcs:
                response[chrc.get_path()] = chrc.get_properties()
                descs = chrc.get_descriptors()
                for desc in descs:
                    response[desc.get_path()] = desc.get_properties()

        return response

class LoraAdvertisement(bluez.Advertisement):

    def __init__(self, config, bus, index):
        bluez.Advertisement.__init__(self, bus, index, 'peripheral')
        self.add_service_uuid(LORA_SERVICE_UUID)
        self.add_local_name(config.getBtName())
        self.include_tx_power = False

class LoraService(bluez.Service):

    def __init__(self, config, bus, index):
        bluez.Service.__init__(self, bus, index, LORA_SERVICE_UUID, True)
        self.add_characteristic(ScheduleCharacteristic(config, bus, 0, self))


class ScheduleCharacteristic(bluez.Characteristic):

    def __init__(self, config, bus, index, service):
        self.config = config
        bluez.Characteristic.__init__(
                self, bus, index,
                SCHEDULE_CHARACTERISTIC_UUID,
                ['read'],
                service)
        self.add_descriptor(ScheduleDescriptor(bus, 0, self))

    def ReadValue(self, options):
        try:
            client = self.parseClient(options)
            if client == None:
                return []

            observationReq = client.findNextObservation()
            if observationReq == None:
                logging.info("[" + client + "] no schedule")
                return []

            logging.info("[" + client + "] observation: " + observationReq)
            # network byte order
            packed = struct.pack("!QQQffBBBbHHB", observationReq.startTimeMillis, observationReq.endTimeMillis, int(time.time() * 1000), observationReq.freq, observationReq.bw, observationReq.sf, observationReq.cr, observationReq.syncWord, observationReq.power, observationReq.preambleLength, observationReq.gain, observationReq.ldro)
            return dbus.Array(packed, signature=dbus.Signature('y'))
        except:
            logging.error(traceback.format_exc())
            return []
    
    def WriteValue(self, value, options):
        try:
            client = self.parseClient(options)
            if client == None:
                return

            headerFormat = "!fffQL"
            headerSize = struct.calcsize(headerFormat)
            frame = {}
            frame.frequencyError, frame.rssi, frame.snr, frame.timestamp, frame.dataLength = struct.unpack(headerFormat, value[:headerSize])
            frame.data = struct.unpack("%ds" % frame.dataLength, value[headerSize:])
            logging.info("[" + client + "] received frame: " + frame)
            client.addFrame(frame)
        except:
            logging.error(traceback.format_exc())
            return []
            
    def parseClient(self, options):
        lastPart = '/dev_'
        index = options['device'].rfind(lastPart)
        btaddress = options['device'][index + len(lastPart):].replace('_',':').lower()
        if self.config.hasClient(btaddress) == False:
            logging.info('client is not configured %s' % btaddress)
            return None
        return self.config.getClients()[btaddress]

class ScheduleDescriptor(bluez.Descriptor):
    
    SCHEDULE_DESCRIPTION_UUID = '5604f205-0c14-4926-9d7d-21dbab315f2e'

    def __init__(self, bus, index, characteristic):
        bluez.Descriptor.__init__(
                self, bus, index,
                self.SCHEDULE_DESCRIPTION_UUID,
                ['read'],
                characteristic)

    def ReadValue(self, options):
        return [dbus.String('Schedule for LoRa module')]

def register_app_cb():
    logging.info('GATT application registered')

def register_app_error_cb(error):
    logging.error('failed to register application: ' + str(error))
    mainloop.quit()

def register_ad_cb():
    logging.info('advertisement registered')

def register_ad_error_cb(error):
    logging.error('failed to register advertisement: ' + str(error))
    mainloop.quit()

def find_adapter(bus):
    remote_om = dbus.Interface(bus.get_object(bluez.BLUEZ_SERVICE_NAME, '/'),
                               bluez.DBUS_OM_IFACE)
    objects = remote_om.GetManagedObjects()

    for o, props in objects.items():
        if bluez.GATT_MANAGER_IFACE in props.keys():
            return o

    return None

class GattServer(threading.Thread):

    def __init__(self, config):
        global mainloop

        dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
        bus = dbus.SystemBus()

        adapter = find_adapter(bus)
        if not adapter:
            logging.error('GattManager1 interface not found')
            return

        service_manager = dbus.Interface(
                bus.get_object(bluez.BLUEZ_SERVICE_NAME, adapter),
                bluez.GATT_MANAGER_IFACE)
        self.ad_manager = dbus.Interface(bus.get_object(bluez.BLUEZ_SERVICE_NAME, adapter),
                bluez.LE_ADVERTISING_MANAGER_IFACE)

        app = Application(config, bus)
        self.advertisement = LoraAdvertisement(config, bus, 0)

        mainloop = GObject.MainLoop()

        logging.info('registering GATT application')

        service_manager.RegisterApplication(app.get_path(), {},
                                        reply_handler=register_app_cb,
                                        error_handler=register_app_error_cb)

        self.ad_manager.RegisterAdvertisement(self.advertisement.get_path(), {},
                                        reply_handler=register_ad_cb,
                                        error_handler=register_ad_error_cb)
        super(GattServer, self).__init__(daemon = True)

    def stop(self):
        mainloop.quit()
    
    def run(self):
        mainloop.run()

        self.ad_manager.UnregisterAdvertisement(self.advertisement)
        logging.info('advertisement unregistered')
        dbus.service.Object.remove_from_connection(self.advertisement)