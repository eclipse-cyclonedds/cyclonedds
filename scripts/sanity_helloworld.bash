exitcode=0
bin/HelloworldPublisher & pub_pid=$!
bin/HelloworldSubscriber & sub_pid=$!
for n in {5..0} ; do
    if ! kill -0 $pub_pid 2>/dev/null && ! kill -0 $sub_pid 2>/dev/null; then
        break
    fi
    sleep 1
done
if [[ $n -eq 0 ]] ; then
    echo "killing process $pub_pid and $sub_pid"
    kill -9 $pub_pid
    kill -9 $sub_pid
    exitcode=2
fi
if [[ $exitcode -gt 0 ]] ; then
    echo "** FAILED **"
else
    echo "** OK **"
fi
exit $exitcode
