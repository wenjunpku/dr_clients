import os
import argparse

CALL_STACK = 0


def get_files(path):
    s = set()
    list_file = os.listdir(path)
    for item in list_file:
        s.add(item)
    return s


def filter_modname(log, valid_modname):
    lines = log.split("\n")
    (inst, _, from_addr, _, from_offset, _, from_modname) = lines[0].split()
    (_, to_addr, _, to_offset, _, to_modname) = lines[1].split()
    if not (from_modname in valid_modname or to_modname in valid_modname):
        return False
    return True


def add_stack(log):
    global CALL_STACK
    lines = log.split("\n")
    (inst, _, from_addr, _, from_offset, _, from_modname) = lines[0].split()
    nowstack = CALL_STACK
    if inst == "CALL" or inst == "INDCALL":
        CALL_STACK += 1
        nowstack = CALL_STACK
    else:
        CALL_STACK -= 1

    mylog = str(nowstack).rjust(3) + " " + log
    mylog = mylog.replace("\n", "\n    ")
    return mylog


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-l", "--log_path", type=str, help="input log path")
    parser.add_argument("-d", "--dir_path", type=str, help="program path")

    args = parser.parse_args()
    log_path = args.log_path
    dir_path = args.dir_path
    dir_path = os.path.dirname(dir_path)
    valid_set = get_files(dir_path)
    fin = open(log_path, "r")
    log = ""
    logs = []
    for line in fin.readlines():
        if line == "\n":
            logs.append(log)
            log = ""
        else:
            log += line

    fout = open(log_path + ".filtered", "w")
    for log in logs:
        is_valid = filter_modname(log, valid_set)
        if is_valid:
            fout.write("%s" % (add_stack(log)))
            fout.write("\n")

