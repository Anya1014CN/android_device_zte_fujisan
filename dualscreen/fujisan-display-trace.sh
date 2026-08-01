#!/usr/bin/env bash
# Capture Fujisan P0 display evidence without changing any system partition.
# Usage:
#   fujisan-display-trace.sh start OUT_DIR
#   fujisan-display-trace.sh stop OUT_DIR
#   fujisan-display-trace.sh capture OUT_DIR SECONDS
set -euo pipefail

adb_cmd=${ADB:-adb}
trace_root=/sys/kernel/debug/tracing
events=(
    mdss/fujisan_display_event
    mdss/mdp_cmd_kickoff
    mdss/mdp_cmd_pingpong_done
)

usage() {
    printf 'usage: %s {start|stop|capture} OUT_DIR [seconds]\n' "$0" >&2
    exit 2
}

prepare_device() {
    "$adb_cmd" root >/dev/null
    "$adb_cmd" wait-for-device
    "$adb_cmd" shell "test -d $trace_root"
}

set_events() {
    local value=$1
    local event
    for event in "${events[@]}"; do
        "$adb_cmd" shell "echo $value > $trace_root/events/$event/enable"
    done
}

capture_state() {
    local out_dir=$1
    "$adb_cmd" shell 'getprop; cat /sys/module/ah1898/parameters/hall_status; cat /sys/module/zte_touch_expand/parameters/separate_inputs' \
        >"$out_dir/properties-and-posture.txt"
    "$adb_cmd" shell 'cat /proc/bus/input/devices; dumpsys input; dumpsys display; dumpsys SurfaceFlinger' \
        >"$out_dir/input-display-sf.txt"
}

start_trace() {
    local out_dir=$1
    mkdir -p "$out_dir"
    prepare_device
    "$adb_cmd" shell "echo 0 > $trace_root/tracing_on; echo > $trace_root/trace"
    set_events 1
    capture_state "$out_dir"
    "$adb_cmd" shell "echo 1 > $trace_root/tracing_on"
    printf 'trace started: %s\n' "$out_dir"
}

stop_trace() {
    local out_dir=$1
    test -d "$out_dir" || {
        printf 'trace output directory does not exist: %s\n' "$out_dir" >&2
        exit 1
    }
    prepare_device
    "$adb_cmd" shell "echo 0 > $trace_root/tracing_on"
    "$adb_cmd" exec-out cat "$trace_root/trace" >"$out_dir/ftrace.txt"
    "$adb_cmd" shell dmesg >"$out_dir/dmesg.txt"
    capture_state "$out_dir"
    set_events 0
    printf 'trace saved: %s\n' "$out_dir"
}

case ${1:-} in
    start)
        test $# -eq 2 || usage
        start_trace "$2"
        ;;
    stop)
        test $# -eq 2 || usage
        stop_trace "$2"
        ;;
    capture)
        test $# -eq 3 || usage
        start_trace "$2"
        sleep "$3"
        stop_trace "$2"
        ;;
    *)
        usage
        ;;
esac
