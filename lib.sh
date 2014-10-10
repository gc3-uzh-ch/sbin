#! /bin/sh

# contains STRING SUBSTRING
#
# Return success if SUBSTRING is contained into STRING.
#
contains () {
    case "$1" in
        *"$2"*) return 0 ;;
        *)      return 1 ;;
    esac
}


# is_range SPEC
#
# Return success if SPEC is a valid range specification,
# i.e., has the form START-END.
#
is_range () {
    contains "$1" '-'
}


# expand_range START-END
#
# Output all numbers from START to END (inclusive),
# one per line
#
expand_range () {
    local start=$(echo "$1" | cut -d- -f1)
    local end=$(echo "$1" | cut -d- -f2)
    seq "$start" "$end" | tr ' ' '\n'
}


# expand_set SET
#
# Output all numbers in SET, one per line.  SET is a
# comma-separated list of ranges START-END (inclusive).
#
expand_set () {
    _expand_set_impl "$1" | tr -s '\n'; echo
}
_expand_set_impl () {
    local set="$1"
    if contains "$set" ','; then
        local first=$(echo "$set" | cut -d, -f1)
        local rest=$(echo "$set" | cut -d, -f2)
        echo $(expand_set "$first")
        echo $(expand_set "$rest")
    else
        if is_range "$set"; then
            expand_range "$set"
        else
            # single number
            echo "$set"
        fi
    fi
}


# for_each_online_cpu CMD ARG [ARG ...]
#
# Run the given command line, once per each online CPU.
# The literal string '{}' is substituted with the CPU
# number.
#
for_each_online_cpu () {
    expand_set $(cat /sys/devices/system/cpu/online) \
        | xargs --replace -n1 "$@"
}

# tests
expand_set '0'
expand_set '0-7'
expand_set '0-4,7'
expand_set '0-4,6-7'

for_each_online_cpu echo CPU '#{}'
