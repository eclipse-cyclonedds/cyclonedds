#!/usr/bin/env bash
set -e

CYCLONEDDS_HOME="${CYCLONEDDS_HOME:-"$(pwd)/../../"}"
[[ ! -d "$CYCLONEDDS_HOME" ]] && { echo "CycloneDDS home ($CYCLONEDDS_HOME) not a dir"; exit 1; }

DEST_DIR="${DEST_DIR:-"$(pwd)/src"}"
[[ ! -d "$DEST_DIR" ]] && { echo "Destination ($DEST_DIR) is not a dir"; exit 1; }

IDLC="${IDLC:-"idlc"}"
command -v "$IDLC" &>/dev/null || { echo "idlc not found"; exit 1; }

SRC_DIRS=( "Roundtrip" "Throughput" "HelloWorld" "DDSPerf" )

for e in "${SRC_DIRS[@]}"; do 
    echo "Copy $e example..."
    case $e in
        Roundtrip)
            src="$CYCLONEDDS_HOME/examples/roundtrip"
            sed 's/int\ main\ /int\ roundtrip_ping\ /' "$src/ping.c" > "$DEST_DIR/ping.c"
            sed 's/int\ main\ /int\ roundtrip_pong\ /' "$src/pong.c" > "$DEST_DIR/pong.c"
            $IDLC "$src/RoundTrip.idl" -o "$DEST_DIR"
            ;;
        Throughput)
            src="$CYCLONEDDS_HOME/examples/throughput"
            sed 's/int\ main\ /int\ throughput_sub\ /' "$src/subscriber.c" > "$DEST_DIR/subscriber.c"
            sed 's/int\ main\ /int\ throughput_pub\ /' "$src/publisher.c" > "$DEST_DIR/publisher.c"
            $IDLC "$src/Throughput.idl" -o "$DEST_DIR"
            ;;
        HelloWorld)
            src="$CYCLONEDDS_HOME/examples/helloworld"
            sed 's/int\ main\ /int\ helloworld_sub\ /' "$src/subscriber.c" > "$DEST_DIR/helloworld_sub.c"
            sed 's/int\ main\ /int\ helloworld_pub\ /' "$src/publisher.c" > "$DEST_DIR/helloworld_pub.c"
            $IDLC "$src/HelloWorldData.idl" -o "$DEST_DIR"
            ;;
        DDSPerf)
            src="$CYCLONEDDS_HOME/src/tools/ddsperf"
            cp "$src"/async_listener.[hc] "$src"/cputime.[hc] "$src"/netload.[hc] "$DEST_DIR"
            sed 's/int\ main\ /int\ ddsperf_main\ /' "$src/ddsperf.c" > "$DEST_DIR/ddsperf.c"
            $IDLC "$src/ddsperf_types.idl" -o "$DEST_DIR"
            ;;
        *)
            echo "Unknown: $e"
            ;;
    esac
done
