import serial
import time
import sys
import copy
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import csv

if len(sys.argv) < 2:
    # プログラム, シリアル通信のポートパスの2引数がなければ処理を終了．
    print(f"Usage: {sys.argv[0]} path_to_port capture_sec sensor_num")
    sys.exit()

port = sys.argv[1]
capture_sec = int(sys.argv[2]) if len(sys.argv) >= 3 else 60 * 1  # 1 min
sensor_num = int(sys.argv[3]) if len(sys.argv) >= 4 else 4  # デフォルトのセンサ数は4

if capture_sec <= 0:
    # It is enough to not time-limited capture.
    capture_sec = 60 * 60 * 24 * 365

sleepTime = 0.001  # second
dataLength = 30


# def read(s, num=1):
#     # num文字読み出す
#     res = b''  # バイト列リテラル
#     while len(res) < num:
#         # 1文字ずつ読み取る
#         # num - len(res)番目の文字を読み取る．
#         # resは1文字読むごとに増えていく
#         res += s.read(num - len(res))
#     return res


def readline(s):
    res = s.readline()
    return res


def drop_all_data(s):
    time.sleep(0.1)  # 10ms
    while(True):  # drop all
        _tmp = s.read(1)
        if(len(_tmp) == 0):
            break


def split_data(raw_line):
    '''
    入力データの形式が変わったときはここを修正する．
       0         1         2
       01234567890123456789012
    例)sid0:9.70,13.00,-122.31
    '''
    if not raw_line.startswith('sid'):
        print("Wait...")
        raise ValueError
    device_id = raw_line[3]
    data_line = raw_line[5:-2]  # 文末は改行コードなので1文字消す
    acc = data_line.split(',')
    if len(acc) != 3:
        raise ValueError
    return [device_id, acc[0], acc[1], acc[2]]


def set_data(data, sensor, glaph_data):
    device_id = int(data[0])
    acc_x = float(data[1])
    acc_y = float(data[2])
    acc_z = float(data[3])

    # CSVで出力用のデータ
    sensor[device_id - 1][0].append([acc_x, acc_y, acc_z])

    # グラフ表示用のデータ．指定した件数しか保持しない．
    glaph_data[device_id - 1][0].append(acc_x)
    glaph_data[device_id - 1][1].append(acc_y)
    glaph_data[device_id - 1][2].append(acc_z)
    if len(sensor[device_id - 1][0]) > dataLength:
        del glaph_data[device_id - 1][0][0]
        del glaph_data[device_id - 1][1][0]
        del glaph_data[device_id - 1][2][0]


c = ['red', 'blue', 'yellow', 'green']


def add_glaph(glaph_data):
    i = 0
    for sensor in glaph_data:
        x = np.array(sensor[0])
        y = np.array(sensor[1])
        z = np.array(sensor[2])

        ax.plot(x, y, z, color=c[i])
        i += 1
    plt.draw()
    plt.pause(sleepTime)
    plt.cla()


with serial.Serial(port, 9600, timeout=0.2) as s:
    time.sleep(0.2)

    s.write(bytes('s', 'utf-8'))
    s.flush()
    sensors = [
        [
            [] for _j in range(3)
        ] for _i in range(sensor_num)
    ]
    glaph_data = copy.deepcopy(sensors)
    flag = True
    try:
        fig = plt.figure()
        ax = Axes3D(fig)
        start_time = time.time()
        while time.time() - start_time < capture_sec:
            raw_line = readline(s)
            str_line = raw_line.decode("UTF-8")  # byte型から文字列へ型変換．byte型は文字列として処理ができない
            try:
                data = split_data(str_line)
                set_data(data, sensors, glaph_data)
                add_glaph(glaph_data)
            except ValueError:
                time.sleep(1)
                continue

    finally:
        i = 1
        for sensor in sensors:
            with open(f'result{i}.csv', 'w') as f:
                writer = csv.writer(f, lineterminator="\n")
                writer.writerow(['X', 'Y', 'Z'])
                for d in sensor:
                    writer.writerows(d)
            i += 1
        print("=======================")
