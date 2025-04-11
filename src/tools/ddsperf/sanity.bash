exitcode=0
# RSS/samples/roundtrip numbers are based on experimentation on Travis
d=bin
[ -n "${BUILD_TYPE}" -a -d bin/${BUILD_TYPE} ] && d=bin/${BUILD_TYPE}
dur=30
declare -a pids

# Only check RSS if address sanitizer not used, the check here is a bit
# sloppy, but probably good enough
qrss="-Qrss:10"
if expr "${SANITIZER}" : '.*address' ; then
    echo "address sanitizer enabled in build, not checking RSS"
    qrss=""
fi

#CYCLONEDDS_URI="$CYCLONEDDS_URI,<Int><Test><Xmit>10</>"
#MallocStackLogging=1 ...
$d/ddsperf -L -D$dur -n10 -Qminmatch:2 -Qlivemem:1 $qrss -Qsamples:100000 -Qroundtrips:3000 sub ping & \
    pids[${#pids}]=$!
$d/ddsperf -L -D$dur -n10 -Qminmatch:2 -Qlivemem:1 $qrss pub 100Hz burst 1000 & \
    pids[${#pids}]=$!

sleep $((dur + 1))
#sleep 5
#malloc_history ${pids[0]} -allByCount > xx1.txt
#sleep $((dur - 6))
#malloc_history ${pids[0]} -allByCount > xx2.txt
#sleep 1

for pid in ${pids[@]} ; do
    if kill -0 $pid 2>/dev/null ; then
        echo "killing process $pid"
        kill -9 $pid
        exitcode=2
    fi
    wait $pid
    x=$?
    if [[ $x -gt $exitcode ]] ; then
        exitcode=$x
    fi
done
if [[ $exitcode -gt 0 ]] ; then
    echo "** FAILED **"
else
    echo "** OK **"
fi
exit $exitcode
