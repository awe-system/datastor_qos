#!/bin/bash
python /opt/bin/conf_rpc/local_cmd/generate_all.py ../demo/demo.xml
cp -f ./output/qos_cli.py ./
cp -f ./output/sysqos_demo_cmds.h ../demo/
cp -f ./output/sysqos_demo_cmds.cpp ../demo/
