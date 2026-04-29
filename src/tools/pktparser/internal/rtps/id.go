package rtps

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"sync/atomic"
)

const (
	UDPGuidPrefixLen  = 12
	Magic             = 0x52545053 // RTPS in ASCII
	MY_RTPS_VENDOR_ID = 0x0110
)

const (
	ENTITYID_UNKNOWN                                = 0x0
	ENTITYID_PARTICIPANT                            = 0x1c1
	ENTITYID_SEDP_BUILTIN_TOPIC_WRITER              = 0x2c2
	ENTITYID_SEDP_BUILTIN_TOPIC_READER              = 0x2c7
	ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER       = 0x3c2
	ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER       = 0x3c7
	ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER      = 0x4c2
	ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER      = 0x4c7
	ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER        = 0x100c2
	ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER        = 0x100c7
	ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER = 0x200c2
	ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER = 0x200c7
	ENTITYID_SOURCE_MASK                            = 0xc0
	ENTITYID_SOURCE_USER                            = 0x00
	ENTITYID_SOURCE_BUILTIN                         = 0xc0
	ENTITYID_SOURCE_VENDOR                          = 0x40
	ENTITYID_KIND_MASK                              = 0x3f
	ENTITYID_KIND_WRITER_WITH_KEY                   = 0x02
	ENTITYID_KIND_WRITER_NO_KEY                     = 0x03
	ENTITYID_KIND_READER_NO_KEY                     = 0x04
	ENTITYID_KIND_READER_WITH_KEY                   = 0x07
	ENTITYID_ALLOCSTEP                              = 0x100
)

var (
	unknownGUIDPrefix = GUIDPrefix{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
	nextUserEntityID  = int32(0)
)

func vendorName(id VendorID) string {
	switch id {
	case 0x0101:
		return "RTI Connext"
	case 0x0102:
		return "PrismTech OpenSplice"
	case 0x0103:
		return "OCI OpenDDS"
	case 0x0104:
		return "MilSoft"
	case 0x0105:
		return "Gallium InterCOM"
	case 0x0106:
		return "TwinOaks CoreDX"
	case 0x0107:
		return "Lakota Technical Systems"
	case 0x0108:
		return "ICOUP Consulting"
	case 0x0109:
		return "ETRI"
	case 0x010a:
		return "RTI Connext Micro"
	case 0x010b:
		return "PrismTech Vortex Cafe"
	case 0x010c:
		return "PrismTech Vortex Gateway"
	case 0x010d:
		return "PrismTech Vortex Lite"
	case 0x010e:
		return "Technicolor Qeo"
	case 0x010f:
		return "eProsima"
	case 0x0120:
		return "PrismTech Vortex Cloud"
	case MY_RTPS_VENDOR_ID:
		return "AutoCore DDS"
	default:
		return "unknown"
	}
}

type DomainID uint16

// EntityID is an entity id.
// NB: always encoded big endian, regardless of submessage endian flag
type EntityID uint32

func (eid EntityID) kind() uint8 {
	return uint8(eid & 0xff)
}

func (eid EntityID) key() []byte {
	return []byte{byte(eid>>8) & 0xff, byte(eid>>16) & 0xff, byte(eid>>24) & 0xff}
}

func createUserID(entityKind uint8) EntityID {
	// For user IDs, "the entityKey field within the EntityId_t
	// can be chosen arbitrarily by the middleware implementation
	// as long as the resulting EntityId_t is unique within the Participant.", sec 9.3.1.2
	return EntityID(atomic.AddInt32(&nextUserEntityID, ENTITYID_ALLOCSTEP) | int32(entityKind))
}

func (eid EntityID) isWriter() bool {
	switch eid & ENTITYID_KIND_MASK {
	case ENTITYID_KIND_WRITER_WITH_KEY, ENTITYID_KIND_WRITER_NO_KEY:
		return true
	}
	return false
}

func (eid EntityID) isReader() bool {
	switch eid & ENTITYID_KIND_MASK {
	case ENTITYID_KIND_READER_WITH_KEY, ENTITYID_KIND_READER_NO_KEY:
		return true
	}
	return false
}

func (eid EntityID) isBuiltin() bool {
	return (eid & ENTITYID_SOURCE_MASK) == ENTITYID_SOURCE_BUILTIN
}

func (eid EntityID) isBuiltinEndpoint() bool {
	return eid.isBuiltin() && eid != ENTITYID_PARTICIPANT
}

type VendorID uint16

func (vid VendorID) String() string {
	return fmt.Sprintf("0x%04x (%s)", uint16(vid), vendorName(vid))
}

type GUIDPrefix []byte

func newGUIDPrefix() GUIDPrefix {
	return make([]byte, UDPGuidPrefixLen)
}

func (gp GUIDPrefix) String() string {
	if gp == nil {
		return "<nil guid>"
	}
	return fmt.Sprintf("%02x%02x%02x%02x-%02x%02x%02x%02x-%02x%02x%02x%02x",
		gp[0], gp[1], gp[2], gp[3], gp[4], gp[5], gp[6], gp[7], gp[8], gp[9], gp[10], gp[11])
}

type GUID struct {
	prefix GUIDPrefix
	eid    EntityID
}

func guidFromBytes(b []byte) GUID {
	return GUID{
		prefix: b[:UDPGuidPrefixLen],
		eid:    EntityID(binary.BigEndian.Uint32(b[UDPGuidPrefixLen:])),
	}
}

func (g *GUID) Bytes() []byte {
	b := make([]byte, 16)
	copy(b, g.prefix)
	binary.BigEndian.PutUint32(b[UDPGuidPrefixLen:], uint32(g.eid))
	return b
}

func (g *GUID) Equal(other *GUID) bool {
	return g.eid == other.eid && bytes.Equal(g.prefix, other.prefix)
}

func (g *GUID) Compare(other *GUID) bool {
	if bytes.Equal(g.prefix, other.prefix) {
		return g.eid < other.eid
	}
	return bytes.Compare(g.prefix, other.prefix) < 0
}

func (g *GUID) Unknown() bool {
	return g.eid == ENTITYID_UNKNOWN && (g.prefix == nil || bytes.Equal(g.prefix, unknownGUIDPrefix))
}

func (g *GUID) String() string {
	return fmt.Sprintf("[%s : 0x%08x]", g.prefix.String(), g.eid)
}

func (g *GUID) CopyGUID() GUID {
	rg := GUID{
		eid: g.eid,
	}
	rg.prefix = make([]byte, UDPGuidPrefixLen)
	copy(rg.prefix, g.prefix)
	return rg
}
