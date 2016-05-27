#!/bin/bash

print_help() {
    echo -e "\tstart-container.sh -d <Plugin Directory> [ -p <port number> ]\n"
}

while getopts "d:p:" opt; do
    case $opt in
	d)
	    pluginDir="$(realpath $OPTARG)"
	    ;;
	p)
	    portNum=$OPTARG
	    ;;
	*)
	    print_help
	    exit 0
	    ;;
    esac
done


if [ -z ${pluginDir+x} ]; then
    echo -e "Missing plugin directory path."    
    print_help
    exit 1
fi

#set default port if none given
if [ -z ${portNum+x} ]; then
    portNum=5000
fi

docker run -d  --name "smartreflect" -v "$pluginDir":/SmartReflectServer/PluginDaemon/web/Plugins \
       -p $portNum:5000 smartreflect:latest
