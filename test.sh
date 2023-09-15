#!/bin/bash

# set -x

confPath=/etc/curve/chunkserver.conf

function ping_mds() {
    if=$1
    targets=$2

    for target in ${targets}; do
        packet_loss=$(ping -c 3 -w 2 ${target} -I $if | grep "packet loss" | awk '{print $6}' | cut -d% -f1)
        if [ $packet_loss -ne 0 ]; then
            echo "ping ${target} through ${if} encounter packet loss ${packet_loss}%"
            return 1
        fi
    done

    return 0
}

function precheck_network() {
    if1=$1
    if2=$1

    mds_addrs=$(cat ${confPath} | grep "mds.listen.addr" | awk -F'=' '{ print $2 }' | awk -F "," '{for(i=1;i<=NF;i++) print $i}' | awk -F ":" '{print $1}' | sort -u)
    ping_mds $if1 "$mds_addrs"
    if [ $? -ne 0 ]; then
        echo "network check failed, please check network configuration"
        exit 1
    fi

    ping_mds $if2 "$mds_addrs"
    if [ $? -ne 0 ]; then
        echo "network check failed, please check network configuration"
        exit 1
    fi
}

echo "precheck networking ..."
precheck_network 10.187.0.92 10.182.2.254
echo "precheck networking done"
