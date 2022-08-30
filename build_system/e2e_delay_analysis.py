#!/usr/bin/env python
# -*- coding: UTF-8 -*-

import operator


def audio_transport_delay_send_side_stats(datas, filepath):
    with open(filepath) as file_handle:
        line = file_handle.readline()
        timestamp = 0
        seq_num = 0
        send_side_send_time = 0
        while line:
            if line.find("SendPacketToNetwork:") >= 0:
                pos = line.find("SendPacketToNetwork:")
                statdata = line[(pos + len("SendPacketToNetwork:")):]
                data_items = statdata.split(",")
                timestamp = int(data_items[0])
                seq_num = int(data_items[1])
            elif line.find("SendRtp:") >= 0:
                pos = line.find("SendRtp:")
                statdata = line[(pos + len("SendRtp:")):]
                data_items = statdata.split(",")
                if int(data_items[0]) == seq_num:
                    send_side_send_time = int(data_items[1])

                if timestamp != 0 and seq_num != 0 and send_side_send_time != 0:
                    datas[timestamp] = [timestamp, seq_num, send_side_send_time]
                timestamp = 0
                seq_num = 0
                send_side_send_time = 0
            line = file_handle.readline()
    return datas


def audio_transport_delay_recv_side_stats(datas, filepath):
    with open(filepath) as file_handle:
        line = file_handle.readline()
        timestamp = 0
        recv_side_recv_time = 0
        decrption_delay = 0
        while line:
            if line.find("Packet received:") >= 0:
                pos = line.find("Packet received:")
                statdata = line[(pos + len("Packet received:")):]
                data_items = statdata.split(",")
                timestamp = int(data_items[0])
                recv_side_recv_time = int(data_items[1])
                decrption_delay = int(data_items[2])
                if operator.contains(datas, timestamp) and len(datas[timestamp]) < 5:
                    datas[timestamp].append(recv_side_recv_time)
                    datas[timestamp].append(decrption_delay)
            line = file_handle.readline()
    return datas


def generate_e2e_delay_report(datas):
    range_lt5 = 0
    range_5to10 = 0
    range_10to15 = 0
    range_15to20 = 0
    range_20to35 = 0
    range_35to60 = 0
    range_60to100 = 0
    range_gt100 = 0
    gt100items = []
    print("|%-20s|%-20s|%-20s|" % ("Delay", "Item Count", "The percentage"))
    print("|%-20s|%-20s|%-20s|" % ("-" * 20, "-" * 20, "-" * 20))

    totalCount = 0
    for (timestamp, data) in datas.items():
        if len(data) < 5:
            continue
        totalCount = totalCount + 1
    for (timestamp, data) in datas.items():
        # data[0] is timestamp
        # data[1] is sequence number
        # data[2] is send side send time
        # data[3] is recv side recv time
        # data[4] is cryption delay
        if len(data) < 5:
            continue
        if data[3] != 0 and data[2] != 0:
            e2e_delay = data[3] - data[2]
            if e2e_delay <= 5:
                range_lt5 = range_lt5 + 1
            elif e2e_delay <= 10:
                range_5to10 = range_5to10 + 1
            elif e2e_delay <= 15:
                range_10to15 = range_10to15 + 1
            elif e2e_delay <= 20:
                range_15to20 = range_15to20 + 1
            elif e2e_delay <= 35:
                range_20to35 = range_20to35 + 1
            elif e2e_delay <= 60:
                range_35to60 = range_35to60 + 1
            elif e2e_delay <= 100:
                range_60to100 = range_60to100 + 1
            else:
                range_gt100 = range_gt100 + 1
                data.append(e2e_delay)
                gt100items.append(data)

    percent = float(range_lt5) / float(totalCount)
    print("|%-20s|%-20d|%-20f|" % ("(0-5]", range_lt5, percent))

    percent = float(range_5to10) / float(totalCount)
    print("|%-20s|%-20d|%-20f|" % ("(5-10]", range_5to10, percent))

    percent = float(range_10to15) / float(totalCount)
    print("|%-20s|%-20d|%-20f|" % ("(10-15]", range_10to15, percent))

    percent = float(range_15to20) / float(totalCount)
    print("|%-20s|%-20d|%-20f|" % ("(15-20]", range_15to20, percent))

    percent = float(range_20to35) / float(totalCount)
    print("|%-20s|%-20d|%-20f|" % ("(20-35]", range_20to35, percent))

    percent = float(range_35to60) / float(totalCount)
    print("|%-20s|%-20d|%-20f|" % ("(35-60]", range_35to60, percent))

    percent = float(range_60to100) / float(totalCount)
    print("|%-20s|%-20d|%-20f|" % ("(60-100]", range_60to100, percent))

    percent = float(range_gt100) / float(totalCount)
    print("|%-20s|%-20d|%-20f|" % ("(100-...]", range_gt100, percent))

   # print("Total items =", len(datas), ", (0, 5] =", range_lt5, ", (5, 10] =", range_5to10, ", (10, 15] =", range_10to15, ", (15, +...] =", range_gt15, ", (50, +...] =", range_gt50, ", (100, +...] =", range_gt100)
    print("Total data:", totalCount)
    for data in gt100items:
        print(str(data))


def recording_playout_delay_stats(filepath, stat_key):
    datas = {}
    with open(filepath) as file_handle:
        line = file_handle.readline()
        while line:
            pos = line.find(stat_key)
            if pos >= 0:
                statdata = line[(pos + len(stat_key)):]
                delay = int(statdata)
                if operator.contains(datas, delay):
                    datas[delay] = datas[delay] + 1
                else:
                    datas[delay] = 1
            line = file_handle.readline()
    return datas


def generate_recording_delay_report(datas, max_valid_value = 35):
    keys = datas.keys()
    keys = sorted(keys)
    print("Count", str(datas))
    totalCount = 0
    for delay in keys:
        if delay < max_valid_value:
            totalCount = totalCount + datas[delay]
    print("Total count:", totalCount)

    print("|%-20s|%-20s|%-20s|" % ("Delay", "Item Count", "The percentage"))
    print("|%-20s|%-20s|%-20s|" % ("-" * 20, "-" * 20, "-" * 20))
    for delay in keys:
        if delay < max_valid_value:
            percent = float(datas[delay]) / float(totalCount)
            print("|%-20d|%-20d|%-20f|" % (delay, datas[delay], percent))


def audio_encording_delay_stats(filepath):
    datas = {}
    with open(filepath) as file_handle:
        line = file_handle.readline()
        sentPacket = True
        while line:
            pos1 = line.find("Send packet:")
            if pos1 > 0:
                sentPacket = True
                line = file_handle.readline()
                continue

            pos = line.find("Audio encoding delay:")
            if pos >= 0:
                statdata = line[(pos + len("Audio encoding delay:")):]
                delay = int(statdata)

                if not sentPacket:
                    if operator.contains(datas, delay):
                        datas[delay] = datas[delay] + 1
                    else:
                        datas[delay] = 1
                else:
                    sentPacket = False
                    if delay > 500:
                        if operator.contains(datas, delay):
                            datas[delay] = datas[delay] + 1
                        else:
                            datas[delay] = 1
            line = file_handle.readline()
    return datas


def audio_send_delay_stats(filepath):
    datas = []
    with open(filepath) as file_handle:
        line = file_handle.readline()
        timestamp = 0
        enqueue_time = 0
        seq_num = 0
        send_end_time = 0
        while line:
            if line.find("EnqueuePackets:") >= 0:
                pos = line.find("EnqueuePackets:")
                statdata = line[(pos + len("EnqueuePackets:")):]
                data_items = statdata.split(",")
                timestamp = int(data_items[0])
                enqueue_time = int(data_items[1])
            elif line.find("SendPacketToNetwork:") >= 0:
                pos = line.find("SendPacketToNetwork:")
                statdata = line[(pos + len("SendPacketToNetwork:")):]
                data_items = statdata.split(",")
                if int(data_items[0]) == timestamp:
                    seq_num = int(data_items[1])
            elif line.find("SendRtp:") >= 0:
                pos = line.find("SendRtp:")
                statdata = line[(pos + len("SendRtp:")):]
                data_items = statdata.split(",")
                if int(data_items[0]) == seq_num:
                    send_end_time = int(data_items[1])

                if timestamp != 0 and enqueue_time != 0 and seq_num != 0 and send_end_time != 0:
                    datas.append([timestamp, enqueue_time, seq_num, send_end_time])
                timestamp = 0
                enqueue_time = 0
                seq_num = 0
                send_end_time = 0
            line = file_handle.readline()
    return datas


def recv_waiting_time_stats(filepath):
    datas = {}
    with open(filepath) as file_handle:
        line = file_handle.readline()
        keystr = "Audio packet waiting time ms:"
        while line:
            pos = line.find(keystr)
            if pos >= 0:
                statdata = line[(pos + len(keystr)):]
                delay = int(statdata)
                if operator.contains(datas, delay):
                    datas[delay] = datas[delay] + 1
                else:
                    datas[delay] = 1
            line = file_handle.readline()
    return datas


def generate_send_delay_report(datas):
    data_result = {}
    for data in datas:
        delay = data[3] - data[1]
        data.append(delay)
        if operator.contains(data_result, delay):
            data_result[delay] = data_result[delay] + 1
        else:
            data_result[delay] = 1
    return data_result


if __name__ == "__main__":
    ########################################################
    # file1 = "D:\\Users\\hhhhhhhh\\AppData\\Local\\RTCTest\\RTCTest_18692.log"
    # file2 = "D:\\Users\\hhhhhhhh\\AppData\\Local\\RTCTest\\RTCTest_5136.log"
    
    # datas = {}
    
    # audio_transport_delay_send_side_stats(datas, file1)
    # audio_transport_delay_recv_side_stats(datas, file2)
    # generate_e2e_delay_report(datas)
    ########################################################

    ########################################################
    # file3 = "D:\\Users\\hhhhhhhh\\AppData\\Local\\RTCTest\\RTCTest_27392.log"
    # datas = recording_playout_delay_stats(file3, "Recording delay:")
    # generate_recording_delay_report(datas)
    ########################################################

    ########################################################
    # file4 = "D:\\Users\\hhhhhhhh\\AppData\\Local\\RTCTest\\RTCTest_27392.log"
    # datas = audio_encording_delay_stats(file4)
    # generate_recording_delay_report(datas, 9000)
    # print(str(datas))
    ########################################################

    ########################################################
    # file5 = "D:\\Users\\hhhhhhhh\\AppData\\Local\\RTCTest\\send_delay.txt"
    # datas = audio_send_delay_stats(file5)
    # generate_send_delay_report(datas)
    ########################################################

    ########################################################
    # file = "D:\\Users\\hhhhhhhh\\AppData\\Local\\RTCTest\\RTCTest_5136.log"
    
    # datas = recv_waiting_time_stats(file)
    # generate_recording_delay_report(datas, 500)
    # print(str(datas))
    ########################################################

    ########################################################
    file = "D:\\Users\\hhhhhhhh\\AppData\\Local\\RTCTest\\RTCTest_18616.log"
    
    datas = recording_playout_delay_stats(file, "Playout delay:")
    generate_recording_delay_report(datas, 500)

    datas = recording_playout_delay_stats(file, "Decoding delay:")
    generate_recording_delay_report(datas, 50000)

    datas = recording_playout_delay_stats(file, "Resampling delay:")
    generate_recording_delay_report(datas, 10000)
    ########################################################
