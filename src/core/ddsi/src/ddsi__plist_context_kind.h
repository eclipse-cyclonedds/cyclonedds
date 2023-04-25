// Copyright(c) 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__PLIST_CONTEXT_KIND_H
#define DDSI__PLIST_CONTEXT_KIND_H

/** \brief Context in which a DDSI parameter list is being interpreted/generated

 A participant has no "liveliness" setting in the specs and the DDSI spec states that a
 participant has a lease that is communicated via the "participant lease duration"
 parameter, but to all intents and purposes, this "participant lease duration" is entirely
 equivalent to an "automatic" liveliness setting. Cyclone internally represents this
 participant lease duration as a liveliness, requiring a transformation to be performed
 when (de)serialising the parameter lists. This necessitates indicating which
 interpretation is to be taken. Currently only two situations: one where this
 transformation is needed, and one where a liveliness is simply that.

 One approach would be to add (yet another) boolean boolean parameter "transform
 liveliness to participant lease duration" to the (de)serialisation of parameter lists,
 but this would not help with the readability of the code. Using an enumerated type for
 the two helps, but naming it becomes problematic and raises the question whether it will
 always be only the liveliness QoS that needs special treatment.

 Another approach uses a "context" argument and leaves the way these are interpreted to
 the (de)serialisation code. That way the call sites become straightforward and all
 details on what it means to handle these parameter lists in a particular context becomes
 local to the (de)serialiser. */
enum ddsi_plist_context_kind {
  DDSI_PLIST_CONTEXT_PARTICIPANT,   /**< for SPDP, maps "participant lease duration" to "liveliness" */
  DDSI_PLIST_CONTEXT_ENDPOINT,      /**< for endpoint discovery ("liveliness" is just that) */
  DDSI_PLIST_CONTEXT_TOPIC,         /**< for topics (equivalent to ENDPOINT for current specs) */
  DDSI_PLIST_CONTEXT_INLINE_QOS,    /**< for inline QoS interpretation (equivalent to ENDPOINT for current specs) */
  DDSI_PLIST_CONTEXT_QOS_DISALLOWED /**< contexts where QoS are disallowed (currently only used for "pserop" serdes) */
};

#endif
