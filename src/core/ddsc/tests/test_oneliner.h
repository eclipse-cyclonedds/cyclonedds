// Copyright(c) 2020 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef _TEST_ONELINER_H_
#define _TEST_ONELINER_H_

#include <stdint.h>
#include <setjmp.h>

#include "dds/dds.h"
#include "dds/ddsrt/sync.h"

/** @brief run a "test" consisting of a sequence of simplish operations
 *
 * This operation takes a test description, really a program in a bizarre syntax, and
 * executes it.  Any failures, be it because of error codes coming out of the Cyclone
 * calls or expected values being wrong cause it to fail the test via CU_ASSERT_FATAL.
 * While it is doing this, it outputs the test steps to stdout including some actual
 * values. An invalid program is mostly reported by calling abort(). It is geared towards
 * checking for listener invocations and the effects on statuses.
 *
 * Entities in play:
 *
 * - participants:   P      P'        P''
 * - subscribers:    R      R'        R''
 * - publishers:     W      W'        W''
 * - readers:        r s t  r' s' t'  r'' s'' t''
 * - writers:        w x y  w' x' y'  w'' x'' y''
 *
 * The unprimed ones exist in domain 0, the primed ones in domain 1 (but configured such
 * that it talks to domain 0), and the double-primed ones in domain 2 (again configured such
 * that it talks to domain 0) so that network-related listener invocations can be checked
 * as well.
 *
 * The first mention of an entity creates it as well as its ancestors.  Implicitly created
 * ancestors always have standard QoS and have no listeners.  There is one topic that is
 * created implicitly when the participant is created.
 *
 * Standard QoS is: default + reliable (100ms), by-source-timestamp, keep-all.  The QoS of
 * a reader/writer can be overridden at the first mention of it (i.e., when it is created)
 * by appending a list of QoS overrides between parentheses.
 *
 * A program consists of a sequence of operations separated by whitespace, ';' or '/'
 * (there is no meaning to the separators, they exist to allow visual grouping):
 *
 * PROGRAM     ::= (OP (\s+|;)*)*
 *
 * OP          ::= (LISTENER)* ENTITY-NAME[(QOS[,QOS[,QOS...]])]
 *
 *                       If entity ENTITY-NAME does not exist:
 *                         creates the entity with the given listeners installed
 *                         QOS can be used to override the standard QoS
 *                       else
 *                         changes the entity's listeners to the specified ones
 *                       (see above for the valid ENTITY-NAMEs)
 *
 *               | -ENTITY-NAME
 *
 *                       Deletes the specified entity
 *
 *               | WRITE-LIKE[fail] ENTITY-NAME K[@DT]
 *               | WRITE-LIKE[fail] ENTITY-NAME (K,X,Y)[@DT]
 *
 *                       Writes/disposes/unregisters (K,0,0) (first form) or (K,X,Y).  If
 *                       "fail" is appended, the expectation is that it fails with a
 *                       timeout, if @DT is appended, the timestamp is the start time of
 *                       the test + <dt>s rather than the current time; DT is a
 *                       floating-point number
 *
 *               | READ-LIKE ENTITY-NAME
 *               | READ-LIKE(A,B) ENTITY-NAME
 *               | READ-LIKE{[S1[,S2[,S3...]][,...]} ENTITY-NAME
 *
 *                       Reads/takes at most 10 samples.  The second form counts the
 *                       number of valid and invalid samples seen and checks them against
 *                       A and B.
 *
 *                       In the third form, the exact result set is given by the sample
 *                       Si, which is a comma-separated list of samples:
 *
 *                         [STATE]K[ENTITY-NAME][@DT]
 *                         [STATE](K,X,Y)[ENTITY-NAME][@DT]
 *
 *                       The first form is an invalid sample with only the (integer) key
 *                       value K, the second form also specifies the two (integer)
 *                       attribute fields.
 *
 *                       STATE specifies allowed sample (f - not-read (fresh), s - read
 *                       (stale)), instance (a - alive, u - no-writers (unregistered) d -
 *                       disposed) and view states (n - new, o - old).  If no sample state
 *                       is specified, all sample states are allowed, &c.
 *
 *                       ENTITY-NAME is the name of the publishing writer expected in the
 *                       publication_handle.  Not specifying a writer means any writer is
 *                       ok.  DT is the timestamp in the same manner as the write-like
 *                       operations.  Not specifying a timestamp means any timestamp is
 *                       ok.
 *
 *                       If the expected set ends up with "..." there may be other samples
 *                       in the result as well.
 *
 *               | ?LISTENER[(ARGS)] ENTITY-NAME
 *
 *                       Waits until the specified listener has been invoked on <entity
 *                       name> using a flag set by the listener function, resets the flag
 *                       and verifies that neither the entity status bit nor the "change"
 *                       fields in the various statuses were set.
 *
 *                       ARGS is used to check the status argument most recently passed to
 *                       the listener:
 *
 *                         da(A)     verifies that it has been invoked A times
 *                         dor(A)    see da
 *                         it(A,B)   verifies count and change match A and B, policy
 *                                   matches RELIABILITY
 *                         lc(A,B,C,D,E) verifies that alive and not-alive counts match A
 *                                   and B, that alive and not-alive changes match C and D
 *                                   and that the last handle matches E if an entity name
 *                                   (ignored if E = "*")
 *                         ll (A,B)  verifies count and change match A and B
 *                         odm (A,B) verifies count and change match A and B, last handle
 *                                   is ignored
 *                         oiq (A,B,C) verifies that total count and change match A and B
 *                                   and that the mismatching QoS is C (using the same
 *                                   abbreviations as used for defining QoS on entity
 *                                   creation)
 *                         pm (A,B,C,D,E) verifies that total count and change match A and
 *                                   B, that current count and change match C and D and
 *                                   that the last handle matches E if an entity name
 *                                   (ignored if E = "*")
 *                         rdm       see odm
 *                         riq       see oiq
 *                         sl (A,B)  verifies that total count and change match A and B
 *                         sr (A,B,C) verifies total count and change match A and B, and
 *                                   that the reason matches C (one of "s" for samples,
 *                                   "i" for instances, "spi" for samples per instance)
 *                         sm        see pm
 *
 *                       A * can be substituted for any argument to indicate the value
 *                       doesn't matter.  (The value does get printed, so it can be a
 *                       handy way to get quickly check the actual value in some cases).
 *
 *               | ?!LISTENER
 *
 *                       (Not listener) tests that LISTENER has not been invoked since
 *                       last reset
 *
 *               | sleep D
 *
 *                       Delay program execution for D s (D is a floating-point number)
 *
 *               | deaf ENTITY-NAME
 *               | deaf! ENTITY-NAME
 *               | hearing ENTITY-NAME
 *               | hearing! ENTITY-NAME
 *
 *                       Makes the domain wherein the specified entity exists deaf,
 *                       respectively restoring hearing.  The entity must be either P or
 *                       P' and both must exist.  The ones suffixed with "!" play use
 *                       some tricks to speed up lease expiry and reconnection (like
 *                       forcibly deleting a proxy participant or triggering the publication
 *                       of SPDP packets).
 *
 *               | setflags(FLAGS) ENTITY-NAME
 *
 *                       Sets test flags on the specified entity (and clears any not
 *                       listed).  Currently only on writers:
 *                         a   ignore ACKNACK messages
 *                         r   ignore retransmit requests
 *                         h   suppress periodic heartbeats
 *                         d   drop outgoing data
 *
 *               | status LISTENER(ARGS) ENTITY-NAME
 *
 *                       Compare the result of dds_get_L_status against ARGS for the
 *                       specified entity, where L is corresponds to the event named by
 *                       LISTENER.  "da" and "dor" events are not allowed.
 *
 * WRITE-LIKE  ::= wr    write
 *               | wrdisp  write-dispose
 *               | disp  dispose
 *               | unreg unregister
 *
 * READ-LIKE   ::= read  dds_read (so any state)
 *               | take  dds_take (so any state)
 *
 * LISTENER    ::= da    data available (acts on a reader)
 *               | dor   data on readers (acts on a subcsriber)
 *               | it    incompatible topic (acts on a topic)
 *               | lc    liveliness changed (acts on a reader)
 *               | ll    liveliness lost (acts on a writer)
 *               | odm   offered deadline missed (acts on a writer)
 *               | oiq   offered incompatible QoS (acts on a writer)
 *               | pm    publication matched (acts on a writer)
 *               | rdm   requested deadline missed (acts on a reader)
 *               | riq   requested incompatible QoS (acts on a reader)
 *               | sl    sample lost (acts on a reader)
 *               | sr    sample rejected (acts on a reader)
 *               | sm    subscription matched (acts on a reader)
 *
 * QOS         ::= ad={y|n}      auto-dispose unregistered instances
 *               | d={v|tl|t|p}  durability
 *               | dl={inf|DT}   deadline (infinite or DT seconds)
 *               | ds=DT/H/RL    durability service: cleanup delay, history,
 *                               resource limits
 *               | do={r|s}      by-reception or by-source destination order
 *               | h={N|all}     history keep-last-N or keep-all
 *               | lb={inf|DT}   latency budget
 *               | ll={a[:DT]|p:DT|w:DT} liveliness (automatic, manual by
 *                               participant, manual by topic)
 *               | ls={inf|DT}   lifespan
 *               | o={s|x[:N]}   ownership shared or exclusive (strength N)
 *               | p={i|t|g}     presentation: instance, coherent-topic or
 *                               coherent-group
 *               | r={be|r[:DT]} best-effort or reliable (with max blocking time)
 *               | rl=N[/N[/N]]  resource limits (sample, instances, samples per
 *                               instance; "inf" is allowed, ommitted ones are
 *                               unlimited)
 *               | tp=N          transport-priority
 *               | ud=...        user data (with escape sequences and hex/octal
 *                               input allowed)
 *
 * All entities share the listeners with their global state. Only the latest invocation is visible.
 *
 * @param[in]  ops  Program to execute.
 * @param[in]  config_override  XML configuration fragment or NULL
 *
 * @return > 0 success, 0 failure, < 0 invalid input
 */
int test_oneliner_with_config (const char *ops, const char *config_override);

/** @brief shorthand for test_oneliner_with_config (ops, NULL)
 * @param[in] ops Program to execute
 * @return > 0 sucess, 0 failure, < 0 invalid input
 */
int test_oneliner (const char *ops);

/** @brief shorthand for test_oneliner with an override that disables any use of shared memory
 * @param[in] ops Program to execute
 * @return > 0 sucess, 0 failure, < 0 invalid input
 */
int test_oneliner_no_shm (const char *ops);

union oneliner_tokval {
  int i;
  int64_t d;
  char n[32];
};

struct oneliner_lex {
  const char *inp;
  dds_time_t tref;
  int tok;
  union oneliner_tokval v;
};

struct oneliner_ctx;

struct oneliner_cb {
  struct oneliner_ctx *ctx;
  dds_listener_t *list;
  uint32_t cb_called[DDS_STATUS_ID_MAX + 1];
  dds_entity_t cb_topic, cb_writer, cb_reader, cb_subscriber;
  dds_inconsistent_topic_status_t cb_inconsistent_topic_status;
  dds_liveliness_changed_status_t cb_liveliness_changed_status;
  dds_liveliness_lost_status_t cb_liveliness_lost_status;
  dds_offered_deadline_missed_status_t cb_offered_deadline_missed_status;
  dds_offered_incompatible_qos_status_t cb_offered_incompatible_qos_status;
  dds_publication_matched_status_t cb_publication_matched_status;
  dds_requested_deadline_missed_status_t cb_requested_deadline_missed_status;
  dds_requested_incompatible_qos_status_t cb_requested_incompatible_qos_status;
  dds_sample_lost_status_t cb_sample_lost_status;
  dds_sample_rejected_status_t cb_sample_rejected_status;
  dds_subscription_matched_status_t cb_subscription_matched_status;
};

struct oneliner_ctx {
  struct oneliner_lex l;

  dds_entity_t es[3 * 9];
  dds_entity_t tps[3];
  dds_entity_t doms[3];
  dds_instance_handle_t esi[3 * 9];
  // built-in topic readers for cross-referencing instance handles
  dds_entity_t pubrd[3];
  dds_entity_t subrd[3];
  // topic name used for data
  char topicname[100];

  const dds_qos_t *qos;
  dds_qos_t *entqos;

  int result;
  char msg[256];

  jmp_buf jb;
  
  int mprintf_needs_timestamp;

  ddsrt_mutex_t g_mutex;
  ddsrt_cond_t g_cond;
  struct oneliner_cb cb[3];
  
  const char *config_override; // optional
};

/** @brief Initialize a "oneliner test" context
 *
 * @param[out] ctx   context to initialize
 * @param[in] config_override  XML configuration fragment or NULL
 */
void test_oneliner_init (struct oneliner_ctx *ctx, const char *config_override);

/** @brief Run a sequence of operations in an initialized context
 *
 * If the context indicates a preceding step has failed, this is a
 * no-op and the previous result is propagated to the return value.
 *
 * @param[in,out] ctx   context to operate in
 * @param[in] ops       sequence of operations to execute (@ref test_oneliner)
 *
 * @return integer indicating success or failure
 *
 * @retval 1   success
 * @retval 0   test failure
 * @retval <0  syntax error unexpected error
 */
int test_oneliner_step (struct oneliner_ctx *ctx, const char *ops);

/** @brief Get a pointer to the error message from a "oneliner test"
 *
 * If a preceding step has failed, this returns a pointer to a message
 * containing some information about the failure.  If no error
 * occurred, the message is meaningless.
 *
 *
 * @param[in] ctx   context to retrieve message from
 *
 * @return pointer to null-terminated string aliasing a string in ctx
 */
const char *test_oneliner_message (const struct oneliner_ctx *ctx);

/** @brief Deinitialize a "oneliner test" context
 *
 * This releases all resources used by the context.
 *
 * @param[in,out] ctx   context to operate in
 *
 * @return integer indicating success or failure in any of the
 * preceding steps.  If no steps were taken, the result is success.
 *
 * @retval 1   success
 * @retval 0   test failure
 * @retval <0  syntax error unexpected error
 */
int test_oneliner_fini (struct oneliner_ctx *ctx);

#endif
