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

usage: mutop [-h] [--domain DOMAIN] [--sampletime SAMPLETIME] [--username USERNAME]
             [--password PASSWORD]
             mqtt_hostname device_hostname

positional arguments:
  mqtt_hostname         Hostname of mqtt server
  device_hostname       Hostname of muwerk-device

optional arguments:
  -h, --help            show this help message and exit
  --domain DOMAIN, -d DOMAIN
                        outgoing domain prefix used by device, default is "omu", use
                        "" if no outgoing domain prefix is used.
  --sampletime SAMPLETIME, -s SAMPLETIME
                        Sampling time in seconds, should be larger than the largest
                        task schedule time.
  --username USERNAME, -u USERNAME
                        username for authorization if the mqtt server requires one.
  --password PASSWORD, -p PASSWORD
                        password for authorization if the mqtt server requires one.
```

If you use mutop frequently, consider making it executable and creating a symbolic link
in your path:

```bash

chmod +x mutop.py
ln -s <fullpath>/mutop.py /usr/local/bin/mutop
```
