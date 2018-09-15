import sys
from sys import argv
import json

def green(s):
    res = "\033[0;32m"
    res+= s
    res +="\033[0m"
    return res

def red(s):
    res = "\033[0;31m"
    res+= s
    res +="\033[0m"
    return res

def yellow(s):
    res = "\033[0;33m"
    res+= s
    res +="\033[0m"
    return res

def tab_func(s):
    res = s + "\t"
    return res

def fail_tab(s):
    res = tab_func(s)
    int_val = int(s)
    if int_val != 0:
        res = red(res)
    return res

printtab = [
    ["client", "cli", tab_func],
    ["server", "serv", tab_func],
    ["retry", "fail", fail_tab],
    ["wait_token", "wait", tab_func],
    ["out_standing", "out", tab_func],
    ["net_send", "snd", tab_func],
    ["net_rcv", "rcv", tab_func],
    ["wait_disk", "disk", tab_func],
    ["complete", "done", tab_func],
    ["total_pending", "pend", tab_func],
    ["t_usec", "task_usec", tab_func],
    ["t_n", "all", tab_func],
]


def header():
    s = "id\t"
    for item in printtab:
        s += tab_func(item[1])
    return s + '\n'

i = 0
def line(cs_item):
    global i
    s = str(i)+"\t"
    i = i+1
    for print_item in printtab:
        s += print_item[2](str(cs_item[print_item[0]]))
    s += "\n"
    return s


def lines(cs_info):
    s = ""
    for item in cs_info:
        s += line(item)
    return s


def j2t(arg):
    info = json.loads(arg[0])
    csinfo = info["all_info"]["csinfo"]
    # tasks = info["all_info"]["tasks"]
    # servers = info["all_info"]["servers"]
    # clients = info["all_info"]["clients"]
    # s = json.dumps(csinfo)
    return 0, header() + lines(csinfo)


cmd_table = {"j2t": j2t,
             }


def help(arg):
    print "-------------------------help-----------------------"
    print "j2t <json>                 ;from json to table format"


if __name__ == '__main__':
    if (len(argv) < 2): help(argv);exit(1)
    err, res = cmd_table.get(argv[1], help)(argv[2:])
    if (err): print >> sys.stderr, res;exit(err)
    print res
