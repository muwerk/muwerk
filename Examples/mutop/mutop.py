import paho.mqtt.client as mqtt
import time
import json
import sys


class MuwerkTop:
    def __init__(self, mqtt_server, muhost, sample_time):
        self.mqtt_server = mqtt_server
        self.sample_time = f"{sample_time}"
        self.muhost = muhost
        self.last_lines = 0
        self.mqttc = mqtt.Client()
        self.mqttc.on_message = self.on_message
        self.mqttc.on_connect = self.on_connect
        self.mqttc.on_publish = self.on_publish
        self.mqttc.on_subscribe = self.on_subscribe
        # Uncomment to enable debug messages
        # self.mqttc.on_log = self.on_log
        self.connection_state = False
        self.mqttc.connect(mqtt_server, 1883, 60)
        self.mqttc.loop_start()

    def on_connect(self, mqttc, obj, flags, rc):
        self.connection_state = True
        print(f"Connected to MQTT server {self.mqtt_server}.")
        # print("Connected.")

    def on_message(self, mqttc, obj, msg):
        smsg = str(msg.payload.decode('utf-8'))
        stat = json.loads(smsg)
        bars = ['█', '▉', '▊', '▋', '▌', '▍', '▎', '▏']

        up = 0

        # print(stat)
        # print(msg.topic + " " + str(msg.qos) + " " + str(msg.payload))

        print("-------------------------------------------------------------------------")
        print("ID Task name    Schedule   Cnt    task rel time  cputime/call   late/call")
        print("-------------------------------------------------------------------------")
        up += 3
        cpu_all = 0

        if 'tsks' not in stat or (len(stat['tdt']) > 0 and len(stat['tdt'][0])) != 6:
            print("Incompatible version of statistics information, received:")
            print(msg.topic + " " + str(msg.qos) + " " + str(msg.payload))
            exit(-1)
        for i in range(stat['tsks']):
            tsk = stat['tdt'][i]
            cpu = int(tsk[4])
            cpu_all += cpu
        for i in range(stat['tsks']):
            tsk = stat['tdt'][i]
            name = tsk[0]
            tid = int(tsk[1])
            sched = f"{(int(tsk[2]))}"
            if sched[-6:] == "000000":
                sched = sched[:-6]+" s"
            else:
                if sched[-3:] == "000":
                    sched = sched[:-3]+"ms"
                else:
                    sched += "µs"

            cnt = int(tsk[3])
            cpu = int(tsk[4])
            late = int(tsk[5])
            if cnt > 0:
                cpu_call = cpu/cnt
                late_call = late/cnt
            if cpu_all > 0:
                per = cpu*100/cpu_all
                bl0 = int(per/10)
                r = int((per-bl0*10)*9/10)
                bls = '█'*bl0
            else:
                per = ''
                r = 0
            if r < 0 or r > 9:
                print(f"didnt work r={r}")
                exit(-2)
            else:
                if r > 0:
                    bls += bars[8-r]

            if cnt > 0:
                print(
                    f"{tid:2} {name[:12]:12} {sched:>8} {cnt:5} {per:6.3f}% {bls:10} {cpu_call:9.2f}µs {late_call:9.2f}µs")
            else:
                print(f"{tid:2} {name[:12]:12} {sched:>8} {cnt:5} ")
            up += 1
        print("-------------------------------------------------------------------------")
        upt = stat['upt']
        h = int(upt/3600)
        rm = upt % 3600
        m = int(rm/60)
        s = rm % 60
        print(
            f"Free memory {stat['mem']:10} bytes, uptime: {h:08}:{m:02}:{s:02}")
        print(
            f"CPU: {cpu_all*100.0/float(stat['apt']):6.3f}%    | Δ: {stat['dt']}µs, OS: {stat['syt']}µs, App: {stat['apt']}µs")
        print("-------------------------------------------------------------------------")
        up += 5
        self.last_lines = up
        print(f"{chr(27)}[{up}A")

    def on_publish(self, mqttc, obj, mid):
        # print("mid: " + str(mid))
        pass

    def on_subscribe(self, mqttc, obj, mid, granted_qos):
        # print("Subscribed: " + str(mid) + " " + str(granted_qos))
        pass

    def on_log(self, mqttc, obj, level, string):
        # print(string)
        pass

    def start(self):
        self.mqttc.publish(f"{self.muhost}/$SYS/stat/get", sample_time, qos=0)

    def stop(self):
        self.mqttc.publish(f"{self.muhost}/$SYS/stat/get", "", qos=0)

    # If you want to use a specific client id, use
    # mqttc = mqtt.Client("client-id")

    # print("tuple")
    # (rc, mid) = mqttc.publish("tuple", "bar", qos=2)
    # print("class")
    # infot = mqttc.publish("class", "bar", qos=2)

    # infot.wait_for_publish()


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: mutop <mqtt-server> <muwerk-host> [sample-secs=1]")
        exit(-1)
    mqtt_server = sys.argv[1]
    muhost = sys.argv[2]
    if len(sys.argv) > 3:
        sample_time = int(sys.argv[3])*1000
    else:
        sample_time = 1000
    mt = MuwerkTop(mqtt_server, muhost, sample_time)
    while mt.connection_state is False:
        time.sleep(0.1)
    stattop = f"omu/{muhost}/$SYS/stat"
    print(f"Subscribing to {stattop}")
    mt.mqttc.subscribe(stattop)
    mt.start()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        mt.stop()
        for i in range(mt.last_lines):
            print()
        print("Unsubscribing...")
        time.sleep(3)
    except Exception as e:
        print(f"Exception: {e}")
