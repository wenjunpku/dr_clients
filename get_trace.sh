#!/bin/bash
if [ $# != 2 ]
then
    echo "Usage: ./get_trace <.exe file path> <log id>"
    exit
fi
path=$1
id=$2
file_name=`basename $path`

echo -e "drrun -c build64/RelWithDebInfo/call_trace.dll $id -- $path\n"
drrun -c build64/RelWithDebInfo/call_trace.dll $id -- $path
echo -e "python trace_handle.py -l logs/call_trace.$file_name.$id.0000.log -d $path\n"
python trace_handle.py -l logs/call_trace.$file_name.$id.0000.log -d $path

echo -e "log file stores in logs/call_trace.$file_name.$id.0000.log"
echo -e "filtered log file stores in ./logs/call_trace.$file_name.$id.0000.log.filtered"
