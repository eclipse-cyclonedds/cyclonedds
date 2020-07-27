exitcode=0
# RSS/samples/roundtrip numbers are based on experimentation on Travis
bin/ddsperf -L -D20 -n10 -Qminmatch:2 -Qrss:4 -Qsamples:300000 -Qroundtrips:3000 sub ping & ddsperf_pids=$!
bin/ddsperf -L -D20 -n10 -Qminmatch:2 -Qrss:4 pub 100Hz burst 1000 & ddsperf_pids="$ddsperf_pids $!"
sleep 21
for pid in $ddsperf_pids ; do
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
