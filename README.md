[![Build Status](https://travis-ci.org/jollheef/rootkiticide.svg?branch=master)](https://travis-ci.org/jollheef/rootkiticide)

# rootkiticide

rootkiticide is a project for dynamic revealing linux rootkits.

Currently is a proof of concept (prototype) rather than ready for usage software.

## Usage

    localhost $ git clone git://github.com/jollheef/rootkiticide
    localhost $ cd rootkiticide
    localhost $ make KERNEL=/path/to/kernel/headers
    localhost $ scp {rkcd.ko,rkcdcli} compromisedhost:
    localhost $ ssh compromisedhost
    compromisedhost $ sudo insmod ./rkcd.ko

Wait some time for collect data and run user-space cli util

    compromisedhost $ ./rkcdcli
