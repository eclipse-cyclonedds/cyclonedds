TEST_BIN=$1
DDSC_LIB=`ldd $1 | grep ddsc | sed -n -E 's/.* => (.*) .*/\1/p'`
SYMBOLS_TEST=`nm -u $TEST_BIN | sed -n -E 's/.* U (.*)/\1/p' | grep -P "^(?!__).+" | sort`
SYMBOLS_SO=`nm -gD $DDSC_LIB | sed -n -E 's/.* T (.*)/\1/p' | grep -P "^(?!__).+" | sort`
diff  <(echo "$SYMBOLS_TEST" ) <(echo "$SYMBOLS_SO")
exit $?
