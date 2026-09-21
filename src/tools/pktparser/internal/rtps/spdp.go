package rtps

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"time"
)

// spdp: Simple Participant Discovery Protocol
//
// "The purpose of a PDP is to discover the presence of other Participants on the network and their properties.
// A Participant may support multiple PDPs, but for the purpose of interoperability,
// all implementations must support at least the Simple Participant Discovery Protocol."

type SPDP struct {
	part *Participant // currently unused - is this needed?
}

func (s *SPDP) init() {
	reader := &Reader{
		// writerGUID: , // all zeros == unknown
		readerEID: ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER,
		dataCB:    s.rxData,
	}
	defaultSession.addReader(reader)
}

// called when data has been received for ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER
func (s *SPDP) rxData(r *receiver, submsg *subMsg, scheme uint16, b []byte) {
	if scheme != SCHEME_PL_CDR_LE {
		println("expected spdp data to be PL_CDR_LE. bailing...")
		return
	}

	plist, _, err := newParamList(submsg.bin, b)
	if err != nil {
		println("newParamList err:", err.Error())
		return // XXX: report
	}

	part := &Participant{}

	for _, p := range plist {

		if p.pid&0x8000 != 0 {
			// ignoring vendor specific params for now
			continue
		}

		switch p.pid {
		case PID_PROTOCOL_VERSION:
			part.protoVer = ProtoVersion{p.value[0], p.value[1]}

		case PID_VENDOR_ID:
			part.vid = VendorID(binary.BigEndian.Uint16(p.value[0:]))

		case PID_DEFAULT_UNICAST_LOCATOR:
			if part.defaultUcastLoc, err = newUDPv4LocFromBytes(submsg.bin, p.value); err == nil {
				if part.defaultUcastLoc.kind == LOCATOR_KIND_UDPV4 {
					// ...
				}
			}

		case PID_DEFAULT_MULTICAST_LOCATOR:
			if part.defaultMcastLoc, err = newUDPv4LocFromBytes(submsg.bin, p.value); err == nil {
				if part.defaultMcastLoc.kind == LOCATOR_KIND_UDPV4 {
					// ...
				} else {
					println("        spdp unknown mcast locator kind: ", s.part.defaultMcastLoc.kind)
				}
			}

		case PID_METATRAFFIC_UNICAST_LOCATOR:
			if part.metaUcastLoc, err = newUDPv4LocFromBytes(submsg.bin, p.value); err == nil {
				if part.metaUcastLoc.kind == LOCATOR_KIND_UDPV4 {
					// ...
				} else if part.metaUcastLoc.kind == LOCATOR_KIND_UDPV6 {
					// ignore ip6 for now...
				} else {
					println("        spdp unknown metatraffic mcast locator kind:", part.metaUcastLoc.kind)
				}
			}

		case PID_METATRAFFIC_MULTICAST_LOCATOR:
			if part.metaMcastLoc, err = newUDPv4LocFromBytes(submsg.bin, p.value); err == nil {
				if part.metaMcastLoc.kind == LOCATOR_KIND_UDPV4 {
					// ...
				} else if part.metaMcastLoc.kind == LOCATOR_KIND_UDPV6 {
					// ignore ip6 for now...
				} else {
					println("        spdp unknown metatraffic mcast locator kind:", part.metaMcastLoc.kind)
				}
			}

		case PID_PARTICIPANT_LEASE_DURATION:
			if dur, err := durationFromBytes(submsg.bin, p.value); err == nil {
				part.leaseDuration = dur
			}

		case PID_PARTICIPANT_GUID:
			part.guidPrefix = p.value[:UDPGuidPrefixLen]
			// XXX: do something with the eid?

		case PID_BUILTIN_ENDPOINT_SET:
			part.builtinEndpoints = builtinEndpointSet(binary.LittleEndian.Uint32(p.value[0:]))

		case PID_PROPERTY_LIST:
			// todo

		default:
			fmt.Printf("      unhandled spdp rx param 0x%x len %d\n", p.pid, len(p.value))
		}
	}

	// now that we have stuff the "part" buffer, spin through our
	// participant list and see if we already have this one
	if _, found := defaultSession.findParticipant(part.guidPrefix); found {
		// TODO: see if anything has changed. update if needed
	} else {
		fmt.Printf("new participant in slot %d: %s\n", len(defaultSession.discoParticipants), part.guidPrefix.String())
		defaultSession.addParticipant(part)
		defaultSession.sedp.addBuiltinEndpoints(part)
	}
}

func (s *SPDP) start() {
	go func() {
		c := time.Tick(1 * time.Second)
		for range c {
			s.bcast()
		}
	}()
}

func (s *SPDP) bcast() {

	var msgbuf bytes.Buffer
	hdr := newHeader()
	hdr.WriteTo(&msgbuf)

	// timestamp submessage
	tsSubmsg := newTsSubMsg(time.Now(), binary.LittleEndian)
	tsSubmsg.WriteTo(&msgbuf)

	// data submessage
	dataSubmsg := submsgData{
		hdr: submsgHeader{
			id:    SUBMSG_ID_DATA,
			flags: FRUDP_FLAGS_LITTLE_ENDIAN | FRUDP_FLAGS_INLINE_QOS | FRUDP_FLAGS_DATA_PRESENT,
			sz:    20, // added to later
		},
		extraflags:        0,
		octetsToInlineQos: 16,
		readerID:          ENTITYID_UNKNOWN,
		writerID:          ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER,
		writerSeqNum:      1,
	}

	var submsgBuf bytes.Buffer

	/////////////////////////////////////////////////////////////
	inlineQosParam := paramListItem{
		pid:   PID_KEY_HASH,
		value: make([]byte, 16),
	}
	copy(inlineQosParam.value, defaultUDPConfig.guidPrefix)
	binary.BigEndian.PutUint32(inlineQosParam.value[12:], ENTITYID_PARTICIPANT)
	inlineQosParam.WriteTo(&submsgBuf)

	sentinel := paramListItem{pid: PID_SENTINEL}
	sentinel.WriteTo(&submsgBuf)

	/////////////////////////////////////////////////////////////
	scheme := encapsulationScheme{
		scheme:  uint16(SCHEME_PL_CDR_LE),
		options: 0,
	}
	scheme.WriteTo(&submsgBuf)

	/////////////////////////////////////////////////////////////
	paramList := paramListItem{
		pid:   PID_PROTOCOL_VERSION,
		value: []byte{MY_RTPS_VERSION_MAJOR, MY_RTPS_VERSION_MINOR, 0, 0},
	}
	paramList.WriteTo(&submsgBuf)

	/////////////////////////////////////////////////////////////
	paramListNext := paramListItem{
		pid:   PID_VENDOR_ID,
		value: []byte{(MY_RTPS_VENDOR_ID >> 8) & 0xff, MY_RTPS_VENDOR_ID & 0xff, 0, 0},
	}
	paramListNext.WriteTo(&submsgBuf)

	/////////////////////////////////////////////////////////////
	ucastLoc := newUDPv4Loc(defaultUDPConfig.unicastAddr, defaultUDPConfig.ucastUserPort())
	defUcastLocParam := paramListItem{
		pid:   PID_DEFAULT_UNICAST_LOCATOR,
		value: ucastLoc.Bytes(),
	}
	defUcastLocParam.WriteTo(&submsgBuf)

	/////////////////////////////////////////////////////////////
	mcastLoc := newUDPv4Loc(DEFAULT_MCAST_GROUP_IP, defaultUDPConfig.mcastUserPort())
	defMcastLocParam := paramListItem{
		pid:   PID_DEFAULT_MULTICAST_LOCATOR,
		value: mcastLoc.Bytes(),
	}
	defMcastLocParam.WriteTo(&submsgBuf)

	/////////////////////////////////////////////////////////////
	ucastBuiltinLoc := newUDPv4Loc(defaultUDPConfig.unicastAddr, defaultUDPConfig.ucastBuiltinPort())
	metaUcastLocParam := paramListItem{
		pid:   PID_METATRAFFIC_UNICAST_LOCATOR,
		value: ucastBuiltinLoc.Bytes(),
	}
	metaUcastLocParam.WriteTo(&submsgBuf)

	/////////////////////////////////////////////////////////////
	mcastBuiltinLoc := newUDPv4Loc(DEFAULT_MCAST_GROUP_IP, defaultUDPConfig.mcastBuiltinPort())
	metaMcastLocParam := paramListItem{
		pid:   PID_METATRAFFIC_MULTICAST_LOCATOR,
		value: mcastBuiltinLoc.Bytes(),
	}
	metaMcastLocParam.WriteTo(&submsgBuf)

	/////////////////////////////////////////////////////////////
	leaseDurParam := paramListItem{
		pid:   PID_PARTICIPANT_LEASE_DURATION,
		value: durationToBytes(time.Duration(time.Second*100), binary.LittleEndian),
	}
	leaseDurParam.WriteTo(&submsgBuf)

	/////////////////////////////////////////////////////////////
	partguidParam := paramListItem{
		pid:   PID_PARTICIPANT_GUID,
		value: make([]byte, 4+UDPGuidPrefixLen),
	}
	copy(partguidParam.value, defaultUDPConfig.guidPrefix)
	binary.BigEndian.PutUint32(partguidParam.value[UDPGuidPrefixLen:], ENTITYID_PARTICIPANT)
	partguidParam.WriteTo(&submsgBuf)

	/////////////////////////////////////////////////////////////
	epSetParam := paramListItem{
		pid:   PID_BUILTIN_ENDPOINT_SET,
		value: []byte{0x3f, 0, 0, 0}, // where does this come from?
	}
	epSetParam.WriteTo(&submsgBuf)

	/////////////////////////////////////////////////////////////
	sentinel.WriteTo(&submsgBuf)

	/////////////////////////////////////////////////////////////
	dataSubmsg.data = submsgBuf.Bytes()
	dataSubmsg.hdr.sz += uint16(len(dataSubmsg.data))

	dataSubmsg.WriteTo(&msgbuf)

	// xxx: cache address somewhere
	if err := udpTXAddr(msgbuf.Bytes(), DEFAULT_MCAST_GROUP_IP.String(), defaultUDPConfig.mcastBuiltinPort()); err != nil {
		println("couldn't transmit SPDP broadcast message:", err.Error())
	}
}
