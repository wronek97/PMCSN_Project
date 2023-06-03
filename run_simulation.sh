#!/bin/bash

# check all input flags
# flag t and m requires parameter (indicated with : at the end)
while getopts "hm:t:" FLAG; do
    case $FLAG in
        m) 
            mode=${OPTARG}
            ;;
        t) 
            topology=${OPTARG}
            ;;
        h)
            echo "run_simulation - execute finite/infinite horizon simulation on a specific microservices app topology"
            echo " "
            echo "syntax: $0 [ -h | -m mode | -t topology ]"
            echo " "
            echo "options:"
            echo "-h,              show brief help"
            echo "-m mode,         specify mode to use [ FINITE | INFINITE ]"
            echo "-t topology,     specify topology to use [ BASE | RESIZED | IMPROVED ]"
            exit 0
            ;;
        ?) 
            echo "script usage: $0 [ -m <FINITE|INFINITE> ] [ -t <BASE|RESIZED|IMPROVED> ]" >&2
            exit 1
            ;;
    esac
done

shift "$(( OPTIND - 1 ))"

# check presence of mode and topology flags
if [ -z "$mode" ] || [ -z "$topology" ] ; then
        echo "script usage: $0 [ -m <FINITE|INFINITE> ] [ -t <BASE|RESIZED|IMPROVED> ]" >&2
        exit 1
fi

# check mode flag
if [ $mode != "FINITE" ] && [ $mode != "INFINITE" ]; then
        echo "script usage: $0 [ -m <FINITE|INFINITE> ] [ -t <BASE|RESIZED|IMPROVED> ]" >&2
        exit 1
fi

# check topology flag
if [ $topology != "BASE" ] && [ $topology != "RESIZED" ] && [ $topology != "IMPROVED" ]; then
        echo "script usage: $0 [ -m <FINITE|INFINITE> ] [ -t <BASE|RESIZED|IMPROVED> ]" >&2
        exit 1
fi

# compile file 
cd source/
make

# start the correct simulation using lowercase flag
cd ..
clear
./bin/simulation_${topology,,} $mode 