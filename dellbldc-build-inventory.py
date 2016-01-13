#!/usr/bin/env python
# -*- coding: utf-8 -*-#
#
#
# Copyright (C) 2015, S3IT, University of Zurich. All rights reserved.
#
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

__docformat__ = 'reStructuredText'
__author__ = 'Antonio Messina <antonio.s.messina@gmail.com>'


import sys
import os
import argparse
import re
import ipaddress

from s3it.inventory import dump_hosts
from collections import defaultdict


def parse_getmacaddress(fd):
    # Parse the output fo racadm getmacaddress
    # Assume hosts start with ^Server-[0-9]+
    #
    # Typical output:
    #
    # <Name>         <Type>              <Presence>    <Active WWN/MAC>          <Partition Status>  <Assignment Type>
    # CMC            N/A                 Present       44:A8:42:3B:B8:3F         N/A                 Factory
    # Server-1-A     IDRAC-Controller    Present       1C:40:24:34:80:A0         N/A                 FlexAddress
    #                10 GbE KR           Present       1C:40:24:34:80:A1         Enabled             FlexAddress
    #                10 GbE KR           Present       1C:40:24:34:80:A3         Enabled             FlexAddress
    #                FCoE-FIP            Present       1C:40:24:34:80:A2         Disabled            FlexAddress
    #                FCoE-FIP            Present       1C:40:24:34:80:A4         Disabled            FlexAddress
    #                FCoE-WWN            Present       20:01:1C:40:24:34:80:A2   Disabled            FlexAddress
    #                FCoE-WWN            Present       20:01:1C:40:24:34:80:A4   Disabled            FlexAddress
    # Server-1-B     Gigabit Ethernet    Not Installed                           Not Installed       Not Installed
    # Server-1-C     Gigabit Ethernet    Present       1C:40:24:34:80:A9         Unknown             FlexAddress
    #                Gigabit Ethernet    Present       1C:40:24:34:80:AB         Unknown             FlexAddress
    #                Gigabit Ethernet    Present       1C:40:24:34:80:AA         Unknown             FlexAddress
    #                Gigabit Ethernet    Present       1C:40:24:34:80:AC         Unknown             FlexAddress

    hosts = defaultdict(dict)
    server_re = re.compile(r'Server-(?P<slot>[0-9]+)-(?P<switch>[A-C])\s+.(?P<type>IDRAC-Controller|Gigabit Ethernet)\s+Present\s+(?P<mac>[^\s]+)\s')

    for line in fd:
        match = server_re.search(line)
        if match:
            myid = '%02d' % int(match.group('slot'))
            switch = match.group('switch')
            if switch == 'A':
                hosts[myid]['sp-mac'] = match.group('mac').lower()
            elif switch == 'C':
                hosts[myid]['eth0-mac'] = match.group('mac').lower()
        # else:
        #     print("Line '%s' not matching regexp '%s'" % (line.strip(), server_re.pattern))
    return hosts

def build_inventory_hosts(inventory_path, chassis, start_v840, start_v841, start_v618, hosts):
    # Build proper inventory hosts to be saved with s3it.inventory.dump_hosts
    inventory = {}
    v840ip = ipaddress.ip_address(unicode(start_v840))
    v841ip = ipaddress.ip_address(unicode(start_v841))
    v618ip = ipaddress.ip_address(unicode(start_v618))
    for slot in sorted(hosts.keys()):
        rawhost = hosts[slot]
        # Actually build two hosts, one for the sp and one for the host
        hostname = 'node-%s-%s' % (chassis, slot)
        sphostname = 'sp-' + hostname

        host = {
            'fqdn': hostname,
            'interfaces': [
                {'aliases': [hostname, hostname+'.int', hostname+'.int.s3it.uzh.ch'],
                 'broadcast': '192.168.163.255',
                 'gateway': '192.168.160.1',
                 'iface': 'eth0',
                 'ip': str(v840ip),
                 'mac': rawhost['eth0-mac'],
                 'mode': 'static',
                 'name': hostname,
                 'netmask': '255.255.252.0',
                 'network': '192.168.160.0'},
                {'aliases': [hostname+'.os', hostname+'.os.s3it.uzh.ch'],
                 'auto': False,
                 'broadcast': '10.129.31.255',
                 'iface': 'vlan618',
                 'iface_extra_conf': ['ovs_options tag=618', 'mtu 9000'],
                 'ip': str(v618ip),
                 'mode': 'static',
                 'netmask': '255.255.240.0',
                 'network': '10.129.16.0',
                 'ovs_bridge': 'br-vlan',
                 'ovs_type': 'OVSIntPort'},
                {'iface': 'bond0',
                 'iface_extra_conf': ['bond-mode 802.3ad',
                                      'mtu 9000',
                                      'bond-miimon 100',
                                      'bond-lacp-rate 1',
                                      'bond-slaves eth4 eth5'],
                 'mode': 'manual'},
                {'iface': 'eth4',
                 'iface_extra_conf': ['bond-master bond0'],
                 'mode': 'manual'},
                {'iface': 'eth5',
                 'iface_extra_conf': ['bond-master bond0'],
                 'mode': 'manual'},
                {'auto': False,
                 'iface': 'br-vlan',
                 'iface_extra_conf': ['ovs_ports vlan618',
                                      'post-up ovs-vsctl add-port br-vlan bond0'],
                 'mode': 'manual',
                 'ovs_type': 'OVSBridge'}]}

        sp = {'fqdn': sphostname,
              'interfaces': [{'aliases': [sphostname+'.mngt', sphostname],
                              'ip': str(v841ip),
                              'mac': rawhost['sp-mac'],
                              'name': sphostname,
                              'netmask': '255.255.252.0'}]}
        inventory[os.path.join(inventory_path, hostname+'.yaml')] = host
        inventory[os.path.join(inventory_path, sphostname+'.yaml')] = sp
        v840ip += 1
        v841ip += 1
        v618ip += 1

    return inventory

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-l', '--location', required=True, help='Location, e.g. l5-21')
    parser.add_argument('-d', '--inventory-path', default='/etc/s3it/inventory', help='Path to inventory directory. Default: %(default)s')
    parser.add_argument('--start-v840', required=True, help='Starting IP in vlan 840')
    parser.add_argument('--start-v841', required=True, help='Starting IP in vlan 841')
    parser.add_argument('--start-v618', required=True, help='Starting IP in vlan 618')
    parser.add_argument('file', nargs='?', type=argparse.FileType('r'), default=sys.stdin,
                        help='Output of command "racadm getmacaddress -c all"')

    cfg = parser.parse_args()
    hosts = parse_getmacaddress(cfg.file)
    inventory = build_inventory_hosts(cfg.inventory_path, cfg.location, cfg.start_v840, cfg.start_v841, cfg.start_v618, hosts)
    dump_hosts(inventory)
