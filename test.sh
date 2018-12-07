#!/bin/sh -eux
echo "Check for proc entry exists"
stat /proc/rootkiticide

echo "Check for proc entry contains logs"
[ 20 -eq $(head -n 20 /proc/rootkiticide | wc -l) ]

echo "Check for proc entry contains valid logs"
python3 -c 'print('"$(head -n 1 /proc/rootkiticide)"')'
