// Copyright(c) 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "test_common.h"
#include "test_oneliner.h"

CU_Test (ddsc_asymdisconnect, reader_keeps_nacking)
{
  // Override the default rescheduling delay to speed things up a bit
  const char *config = "<Internal><AutoReschedNackDelay>100ms</AutoReschedNackDelay></Internal>";
  
  const int result = test_oneliner_with_config (
    "da sm r(r=r,d=tl) pm w'(r=r,d=tl)"
    "  ?sm r ?pm w'"
    // given a matched reader/writer pair with a network in between
    // write some data & make sure it is present
    "  wr w' 1"
    "  ?da r take{(1,0,0)} r"
    // once the data has been acknowledged, make P deaf
    // - r loses sight of w'
    // - w' still knows r; all data ACK'd so no further heartbeats
    "  ?ack w'"
    "  deaf! P ?sm r"
    // disconnect causes invalid sample, take that
    // (?da r is there to reset the 'data available callback invoked flag'
    // else we can't assert it didn't get triggered after the reconnect)
    "  ?da r take{1} r"
    // make w' suppress retransmits
    // - this really is just a little devil dropping some packets
    "  setflags(r) w'"
    // restoring hearing triggers discovery
    "  hearing! P"
    // r should see w' again
    // - r will send a pre-emptive ACKNACK, w' will respond with heartbeat
    // - r will then send ACKNACKs requesting retransmit, w' forgets to send the data
    "  ?sm r"
    // let this go on for a little bit
    "  sleep 2"
    // change the debugging flags of w' to now ignore ACKNACKs
    // - this really is just a little devil dropping some packets
    // - this should cause the spontaneous heartbeats to stop
    // - r needs to keep trying (it wouldn't because of a bug)
    // setting the flags has some race conditions, but chances are it'll do what we want
    "  setflags(a) w'"
    // let this go on for a while
    "  sleep 2"
    // no data should have arrived: first retransmits were suppressed
    // then the retransmit requests were ignored
    "  ?!da"
    // clearing all test flags on w should fix the problem
    "  setflags() w'"
    // without the fix and with flags cleared, disconnecting and reconnecting fixes it:
    //   sleep 2 !?!da take{} r // so no data!
    //   deaf! P ?sm r hearing! P ?sm r // dis-/reconnect
    //   ?da r take{(1,0,0)} r
    // with fix present:
    "  ?da r take{(1,0,0)} r"
    , config);
  CU_ASSERT (result > 0);
}
