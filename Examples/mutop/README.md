## mutop Python example muwerk process monitor via MQTT

When using `munet`s mqtt stack, statistical information of the muwerk scheduler can be exported via mqtt.

The script `mutop.py` implements a simple python mqtt client that displays information about muwerk tasks
in a top-like manner:

![mutop](https://github.com/muwerk/muwerk/blob/master/Resources/mutop.jpg?raw=true)

Dependencies
------------

`mutop` requires [`paho-mqtt`](https://pypi.org/project/paho-mqtt/)

install with:

```
pip install paho-mqtt
```

Usage
-----

```
$ python mutop.py -h

usage: mutop.py [-h] [--domain DOMAIN] [--sampletime SAMPLETIME]
                mqtt_hostname device_hostname

positional arguments:
  mqtt_hostname         Hostname of mqtt server
  device_hostname       Hostname of muwerk-device

optional arguments:
  -h, --help            show this help message and exit
  --domain DOMAIN, -d DOMAIN
                        outgoing domain prefix used by device, default is 'omu'
  --sampletime SAMPLETIME, -s SAMPLETIME
                        Sampling time in seconds, should be larger than the largest
                        task schedule time.
```
