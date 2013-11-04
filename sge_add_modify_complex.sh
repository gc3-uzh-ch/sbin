#!/bin/sh

me=$(basename "$0")

modify_host() {
    host="$1"

    tmp=$(mktemp -t "${me}.XXXXXX")

    env SGE_SINGLE_LINE=1 qconf -se ${host} \
        | egrep -v '^(load_values|processors)' > $tmp

    if egrep -q serial_job "$tmp"; then
        sed -r -i -e 's/serial_job=1/partition="odd"/' $tmp
    else
        sed -r -i -e 's/complex_values(\s+)(.+)/complex_values\1partition="even",\2/' $tmp
    fi

    qconf -Me $tmp

    rm -f "$tmp"
}

modify_host $1

