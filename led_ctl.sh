#!/bin/bash
led=$1
echo $led > /sys/class/leds/sys-led
