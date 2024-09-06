cu='<Gen><Interfaces><Netw n="lo0"/></></><Int><LivelinessM>true</></><Tr><C>trace</></>'

function start () {
    allowmc="<Gen><AllowMulti>$1</></>" ; shift
    spdp="<Disc><SPDPInt>0.5s</><SPDPMulti>$1</><LeaseD>2s</></>" ; shift
    parti="<Disc><Partici>$1</><InitialLoc>2s</><DiscoveredLoc>2s</></>" ; shift
    peers=""
    if [ $# -gt 0 ] ; then
        peers="<Disc><Peers"
        if [ -z "$1" ] ; then
            peers="$peers addlocalhost=\"true\""
            shift
        fi
        peers="$peers>"
        while [ $# -gt 0 ] ; do
            peers="$peers<Peer addr=\"$1\"/>"
            shift
        done
        peers="$peers</></>"
    fi
    set -x
    CYCLONEDDS_URI="$cu$allowmc$spdp$parti$peers<Tr><Out>cdds.log.${#pids[@]}</></>" bin/Debug/ddsperf sanity & pids[${#pids[@]}]=$!
    set +x
}

x=true

# sanity check a bunch of cases where discovery should/shouldn't happen
echo "====== I.1 ========"
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start true 239.255.0.2 default
start true 239.255.0.2 default
sleep 1
kill ${pids[@]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 || x=false
grep -q 'SPDP.*NEW' cdds.log.1 || x=false
$x || exit 1

echo "====== I.2 ========"
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start false 239.255.0.2 default localhost
start false 239.255.0.2 default localhost
sleep 1
kill ${pids[@]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 || x=false
grep -q 'SPDP.*NEW' cdds.log.1 || x=false
$x || exit 1

echo "====== I.3 ========"
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start true 0.0.0.0 default
start true 0.0.0.0 default
sleep 1
kill ${pids[@]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 && x=false
grep -q 'SPDP.*NEW' cdds.log.1 && x=false
$x || exit 1

echo "====== I.4 ========"
# no discovery happens: first one says peers defined => well-known ports, pings localhost
# second says: MC supported/allowed, no peers defined => port none
# a bit weird, but not wrong
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start true 0.0.0.0 default localhost
start true 0.0.0.0 default
sleep 1
kill ${pids[@]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 && x=false
grep -q 'SPDP.*NEW' cdds.log.1 && x=false
$x || exit 1

echo "====== I.5 ========"
# no discovery happens: first one says peers defined => well-known ports, pings localhost
# second says: peers defined => well-known ports
# a bit weird, but not wrong
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start true 0.0.0.0 default localhost
start true 0.0.0.0 default ""
sleep 1
kill ${pids[@]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 || x=false
grep -q 'SPDP.*NEW' cdds.log.1 || x=false
$x || exit 1

echo "====== I.6 ========"
# no discovery happens: first one says MC works => no localhost, random port
# second says: peers defined => no MC, well-known ports
# but that doesn't allow them to discover each other
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start true 239.255.0.2 default
start false 0.0.0.0 default localhost
sleep 1
kill ${pids[@]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 && x=false
grep -q 'SPDP.*NEW' cdds.log.1 && x=false
$x || exit 1

echo "====== I.7 ========"
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start true 239.255.0.2 auto
start false 0.0.0.0 default
sleep 1
kill ${pids[@]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 || x=false
grep -q 'SPDP.*NEW' cdds.log.1 || x=false
$x || exit 1

echo "====== I.8 ========"
# fails: second uses random port
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start false 239.255.0.2 default localhost
start true 239.255.0.1 default
sleep 1
kill ${pids[@]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 && x=false
grep -q 'SPDP.*NEW' cdds.log.1 && x=false
$x || exit 1

echo "====== I.9 ========"
# now it works: participant index set to auto
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start false 239.255.0.2 default localhost
start true 239.255.0.1 auto
sleep 1
kill ${pids[@]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 || x=false
grep -q 'SPDP.*NEW' cdds.log.1 || x=false
$x || exit 1

# pruning of useless initial locators
# prune delays are 2s, spdp interval = 0.5s

echo "====== II.1 ========"
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start true 239.255.0.1 default 168.10.10.10
start true 239.255.0.1 default
sleep 3
kill ${pids[@]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 || x=false
grep -q 'SPDP.*NEW' cdds.log.1 || x=false
grep -q 'spdp: prune loc.*168\.10\.10\.10' cdds.log.0 || x=false
$x || exit 1

echo "====== II.2 ========"
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start false 239.255.0.1 0 ""
start false 239.255.0.1 1 ""
sleep 3
kill ${pids[@]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 || x=false
grep -q 'SPDP.*NEW' cdds.log.1 || x=false
# mustn't prune the address of the existing one
grep -q 'spdp: prune loc.*127\.0\.0\.1:7412' cdds.log.0 && x=false
grep -q 'spdp: prune loc.*127\.0\.0\.1:7410' cdds.log.1 && x=false
# must prune some others
grep -q 'spdp: prune loc.*127\.0\.0\.1:7414' cdds.log.0 || x=false
grep -q 'spdp: prune loc.*127\.0\.0\.1:7414' cdds.log.1 || x=false
$x || exit 1

echo "====== II.3 ========"
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start false 239.255.0.1 0 localhost
start false 239.255.0.1 1 localhost
sleep 3
kill ${pids[@]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 || x=false
grep -q 'SPDP.*NEW' cdds.log.1 || x=false
# mustn't prune the address of the existing one
grep -q 'spdp: prune loc.*127\.0\.0\.1:7412' cdds.log.0 && x=false
grep -q 'spdp: prune loc.*127\.0\.0\.1:7410' cdds.log.1 && x=false
# must prune some others
grep -q 'spdp: prune loc.*127\.0\.0\.1:7414' cdds.log.0 || x=false
grep -q 'spdp: prune loc.*127\.0\.0\.1:7414' cdds.log.1 || x=false
$x || exit 1

echo "====== II.4 ========"
# first stops early, second discovered the address
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start false 239.255.0.1 0 localhost
start false 239.255.0.1 1
sleep 3
kill ${pids[0]}
sleep 1
kill ${pids[1]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 || x=false
grep -q 'SPDP.*NEW' cdds.log.1 || x=false
# clean termination -- verify:
grep -q 'SPDP.*ST3' cdds.log.1 || x=false
# 2s until pruning
# but only a 1s wait -> can't have pruned based on time yet
# clean termination: should've been dropped
grep -q 'spdp: drop live loc' cdds.log.1 || x=false
$x || exit 1

echo "====== II.5 ========"
# second stops early, first simply uses initial locator
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start false 239.255.0.1 0 localhost
start false 239.255.0.1 1
sleep 3
kill ${pids[1]}
sleep 1
kill ${pids[0]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 || x=false
grep -q 'SPDP.*NEW' cdds.log.1 || x=false
# clean termination -- verify:
grep -q 'SPDP.*ST3' cdds.log.0 || x=false
# enough time to clean up unused locators
grep -q 'spdp: prune loc.*127\.0\.0\.1:7414' cdds.log.0 || x=false
# locator was in initial set
# locator kept alive by existing participant
# clean termination -> may drop it
# initial prune delay passed, so dropped immediately
grep -q 'spdp: drop live loc.*127\.0\.0\.1:7412' cdds.log.0 || x=false
$x || exit 1

echo "====== II.6 ========"
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start false 239.255.0.1 0 localhost
start false 239.255.0.1 1
sleep 3
kill -9 ${pids[0]}
sleep 3
kill ${pids[1]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 || x=false
grep -q 'SPDP.*NEW' cdds.log.1 || x=false
# 2s until lease expiry
# 2s until pruning
# but only a 3s wait -> not pruned yet
grep -q 'spdp: prune loc' cdds.log.1 && x=false
$x || exit 1

echo "====== II.7 ========"
rm -rf cdds.log.0 cdds.log.1
declare -a pids
start false 239.255.0.1 0 localhost
start false 239.255.0.1 1
sleep 3
kill -9 ${pids[0]}
sleep 5
kill ${pids[1]}
wait
unset pids
grep -q 'SPDP.*NEW' cdds.log.0 || x=false
grep -q 'SPDP.*NEW' cdds.log.1 || x=false
# 2s until lease expiry
# 2s until pruning
# 5s wait -> should be pruned
grep -q 'spdp: prune loc' cdds.log.1 || x=false
$x || exit 1
