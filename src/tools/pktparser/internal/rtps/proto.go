package rtps

import (
	"encoding/binary"
	"fmt"

	// "fmt"
	"io"
	"time"
)

// XXX: too much copying, use zero copy buffers

var (
	SeqNumUnknown = newSeqNum(-1, 0)
)



const (
	FRUDP_FLAGS_LITTLE_ENDIAN = 0x01
	FRUDP_FLAGS_INLINE_QOS    = 0x02
	FRUDP_FLAGS_DATA_PRESENT  = 0x04

	FLAGS_SM_ENDIAN = 0x01 // applies to all submessages

	FLAGS_INFOTS_INVALIDATE = 0x2

	FLAGS_DATA_INLINE_QOS = 0x02
	FLAGS_DATA_DATAFLAG   = 0x04
	FLAGS_DATA_KEYFLAG    = 0x08

	FLAGS_ACKNACK_FINAL = 0x02

	FLAGS_HEARTBEAT_FLAG_FINAL      = 0x02
	FLAGS_HEARTBEAT_FLAG_LIVELINESS = 0x04

	SMID_ID_HE               = 0x00
	SUBMSG_ID_PAD            = 0x01
	SUBMSG_ID_ACKNACK        = 0x06
	SUBMSG_ID_HEARTBEAT      = 0x07
	SUBMSG_ID_GAP            = 0x08
	SUBMSG_ID_INFO_TS        = 0x09
	SUBMSG_ID_INFO_SRC       = 0x0c
	SUBMSG_ID_INFO_REPLY_IP4 = 0x0d
	SUBMSG_ID_INFO_DST       = 0x0e
	SUBMSG_ID_INFO_REPLY     = 0x0f
	SUBMSG_ID_NACK_FRAG      = 0x12
	SUBMSG_ID_HEARTBEAT_FRAG = 0x13
	SUBMSG_ID_DATA           = 0x15
	SUBMSG_ID_DATA_FRAG      = 0x16
	/* security-specific sub messages */
	SMID_ID_SEC_BODY         = 0x30
    SMID_ID_SEC_PREFIX       = 0x31
    SMID_ID_SEC_POSTFIX      = 0x32
    SMID_ID_SRTPS_PREFIX     = 0x33
    SMID_ID_SRTPS_POSTFIX    = 0x34
	/* vendor-specific sub messages (0x80 .. 0xff) */
	SUBMSG_ID_PT_INFO_CONTAINER = 0x80
	SUBMSG_ID_PT_MSG_LEN        = 0x81
	SUBMSG_ID_PT_ENTITY_ID      = 0x82

	SCHEME_CDR_LE    = 0x0001
	SCHEME_PL_CDR_LE = 0x0003

	MY_RTPS_VERSION_MAJOR = 2
	MY_RTPS_VERSION_MINOR = 1
)

const (
	PID_PAD                           = 0x0000
	PID_SENTINEL                      = 0x0001
	PID_PARTICIPANT_LEASE_DURATION    = 0x0002
	PID_TIME_BASED_FILTER             = 0x0004
	PID_TOPIC_NAME                    = 0x0005
	PID_OWNERSHIP_STRENGTH            = 0x0006
	PID_TYPE_NAME                     = 0x0007
	PID_METATRAFFIC_MULTICAST_IPADDRESS = 0x000b
    PID_DEFAULT_UNICAST_IPADDRESS     = 0x000c
	PID_METATRAFFIC_UNICAST_PORT      = 0x000d
    PID_DEFAULT_UNICAST_PORT          = 0x000e
	PID_DOMAIN_ID                     = 0x000f
	PID_MULTICAST_IPADDRESS           = 0x0011
	PID_PROTOCOL_VERSION              = 0x0015
	PID_VENDOR_ID                     = 0x0016
	PID_RELIABILITY                   = 0x001a
	PID_LIVELINESS                    = 0x001b
	PID_DURABILITY                    = 0x001d
	PID_DURABILITY_SERVICE            = 0x001e
	PID_OWNERSHIP                     = 0x001f
	PID_PRESENTATION                  = 0x0021
	PID_DEADLINE                      = 0x0023
	PID_DESTINATION_ORDER             = 0x0025
	PID_LATENCY_BUDGET                = 0x0027
	PID_PARTITION                     = 0x0029
	PID_LIFESPAN                      = 0x002b
	PID_USER_DATA                     = 0x002c
	PID_GROUP_DATA                    = 0x002d
	PID_TOPIC_DATA                    = 0x002e
	PID_UNICAST_LOCATOR                  = 0x002f
	PID_MULTICAST_LOCATOR                = 0x0030
	PID_DEFAULT_UNICAST_LOCATOR       = 0x0031
	PID_METATRAFFIC_UNICAST_LOCATOR   = 0x0032
	PID_METATRAFFIC_MULTICAST_LOCATOR = 0x0033
	PID_PARTICIPANT_MANUAL_LIVELINESS_COUNT = 0x0034
	PID_CONTENT_FILTER_PROPERTY             = 0x0035	
	PID_HISTORY                       = 0x0040
	PID_RESOURCE_LIMITS                = 0x0041
	PID_EXPECTS_INLINE_QOS                   = 0x0043
	PID_PARTICIPANT_BUILTIN_ENDPOINTS        = 0x0044
	PID_METATRAFFIC_UNICAST_IPADDRESS        = 0x0045
	PID_METATRAFFIC_MULTICAST_PORT           = 0x0046
	PID_DEFAULT_MULTICAST_LOCATOR     = 0x0048
	PID_TRANSPORT_PRIORITY            = 0x0049
	PID_PARTICIPANT_GUID              = 0x0050
	PID_PARTICIPANT_ENTITYID                 = 0x0051
	PID_GROUP_GUID                           = 0x0052
	PID_GROUP_ENTITYID                       = 0x0053
	PID_CONTENT_FILTER_INFO                  = 0x0055
	PID_COHERENT_SET                         = 0x0056
	PID_DIRECTED_WRITE                       = 0x0057	
	PID_BUILTIN_ENDPOINT_SET          = 0x0058
	PID_PROPERTY_LIST                 = 0x0059
	PID_ENDPOINT_GUID                 = 0x005a
	PID_TYPE_MAX_SIZE_SERIALIZED             = 0x0060
	PID_ORIGINAL_WRITER_INFO                 = 0x0061
	PID_ENTITY_NAME                          = 0x0062
	PID_TIME_BASED_FILTER_BY_WRITER          = 0x006a
	PID_TSN_QOS                              = 0x006b	
	PID_KEY_HASH                      = 0x0070
	PID_STATUSINFO                           = 0x0071
	PID_DATA_REPRESENTATION                  = 0x0073
	PID_TYPE_CONSISTENCY_ENFORCEMENT         = 0x0074
	PID_TYPE_INFORMATION                     = 0x0075
	PID_IDENTITY_TOKEN                       = 0x1001
	PID_PERMISSIONS_TOKEN                    = 0x1002
	PID_DATA_TAGS                            = 0x1003
	PID_ENDPOINT_SECURITY_INFO               = 0x1004
	PID_PARTICIPANT_SECURITY_INFO            = 0x1005
	PID_IDENTITY_STATUS_TOKEN                = 0x1006
	PID_DOMAIN_TAG                           = 0x4014
	PID_STATIC_DISCOVERY                     = 0x801e
)

const (
	BUILTIN_EP_PARTICIPANT_ANNOUNCER           = 0x00000001
	BUILTIN_EP_PARTICIPANT_DETECTOR            = 0x00000002
	BUILTIN_EP_PUBLICATION_ANNOUNCER           = 0x00000004
	BUILTIN_EP_PUBLICATION_DETECTOR            = 0x00000008
	BUILTIN_EP_SUBSCRIPTION_ANNOUNCER          = 0x00000010
	BUILTIN_EP_SUBSCRIPTION_DETECTOR           = 0x00000020
	BUILTIN_EP_PARTICIPANT_PROXY_ANNOUNCER     = 0x00000040
	BUILTIN_EP_PARTICIPANT_PROXY_DETECTOR      = 0x00000080
	BUILTIN_EP_PARTICIPANT_STATE_ANNOUNCER     = 0x00000100
	BUILTIN_EP_PARTICIPANT_STATE_DETECTOR      = 0x00000200
	BUILTIN_EP_PARTICIPANT_MESSAGE_DATA_WRITER = 0x00000400
	BUILTIN_EP_PARTICIPANT_MESSAGE_DATA_READER = 0x00000800
)

const (
	MaxSeqNum = 0x7fffffffffffffff
)

type SeqNum int64

func newSeqNum(hi int32, lo uint32) SeqNum {
	return SeqNum(int64(hi)<<32 + int64(lo))
}

func (s SeqNum) String() string {
	return fmt.Sprintf("%d", s)
}

type ProtoVersion struct {
	major uint8
	minor uint8
}

func (p *ProtoVersion) String() string {
	return fmt.Sprintf("%d.%d", p.major, p.minor)
}

type Header struct {
	magic      uint32 // RTPS in ASCII
	protoVer   ProtoVersion
	vid        VendorID // vendor ID
	guidPrefix GUIDPrefix
}

func newHeader() *Header {
	return &Header{
		magic:      Magic,
		protoVer:   ProtoVersion{MY_RTPS_VERSION_MAJOR, MY_RTPS_VERSION_MINOR},
		vid:        MY_RTPS_VENDOR_ID,
		guidPrefix: defaultUDPConfig.guidPrefix,
	}
}

func (h *Header) String() string {
	str := "- Layer Application " +
		"{magic : 0x%x, protoVer : %v, vid : 0x%04x, guidPrefix : %v}"
	return fmt.Sprintf(str, h.magic, h.protoVer, h.vid, h.guidPrefix)
}

func (h *Header) WriteTo(w io.Writer) {
	b := make([]byte, 8)
	binary.BigEndian.PutUint32(b[0:], h.magic)
	b[4], b[5] = h.protoVer.major, h.protoVer.minor
	binary.BigEndian.PutUint16(b[6:], uint16(h.vid))
	w.Write(b)
	w.Write(h.guidPrefix)
	// XXX: err...
}

func newHeaderFromBytes(b []byte) (*Header, error) {
	if len(b) < 8+UDPGuidPrefixLen {
		return nil, io.EOF
	}

	hdr := &Header{
		magic:      binary.BigEndian.Uint32(b[0:]),
		protoVer:   ProtoVersion{major: b[4], minor: b[5]},
		vid:        VendorID(binary.BigEndian.Uint16(b[6:])),
		guidPrefix: b[8 : 8+UDPGuidPrefixLen],
	}

	return hdr, nil
}

type Msg struct {
	hdr  Header
	data []uint8 // []submsg
}

type SeqNumSet struct {
	bitmapBase SeqNum   // first sequence number in the set
	numBits    uint32   // total bit count
	bitmap     []uint32 // as many uint32s required by numBits
}

func (sns *SeqNumSet) Last() SeqNum {
	return sns.bitmapBase + SeqNum(sns.numBits)
}

func (sns *SeqNumSet) Valid() bool {
	if sns.bitmapBase <= 0 {
		return false
	}
	if sns.numBits <= 0 || sns.numBits > 256 {
		return false
	}
	return true
}

func (sns *SeqNumSet) BitMapWords() int {
	return int((sns.numBits + 31) / 32)
}

type submsgHeader struct {
	id    uint8
	flags uint8
	sz    uint16
}

func (s *submsgHeader) Write(b []byte) {
	b[0], b[1] = s.id, s.flags
	binary.LittleEndian.PutUint16(b[2:], s.sz)
}

func (s *submsgHeader) WriteTo(w io.Writer) {
	b := make([]byte, 4)
	b[0], b[1] = s.id, s.flags
	binary.LittleEndian.PutUint16(b[2:], s.sz)
	w.Write(b)
	// XXX: err...
}

type subMsg struct {
	hdr  submsgHeader
	bin  binary.ByteOrder // relevant for packing/unpacking
	data []uint8
}

func newSubMsgFromBytes(b []byte) (*subMsg, error) {
	// 检查最小长度要求：至少需要4字节的头部（id:1 + flags:1 + sz:2）
	if len(b) < 4 {
		return nil, fmt.Errorf("子消息数据不足，需要至少4字节但只有%d字节", len(b))
	}
	
	sm := &subMsg{
		hdr: submsgHeader{
			id:    b[0],
			flags: b[1],
		},
	}
	if sm.hdr.flags&FLAGS_SM_ENDIAN != 0 {
		sm.bin = binary.LittleEndian
	} else {
		sm.bin = binary.BigEndian
	}
	sm.hdr.sz = sm.bin.Uint16(b[2:])

	// make sure we can trust sm.hdr.sz
	if len(b) < int(sm.hdr.sz)+4 {
		return nil, fmt.Errorf("子消息长度不一致，头部声明%d字节但实际只有%d字节", sm.hdr.sz+4, len(b))
	}

	sm.data = b[4 : 4+sm.hdr.sz]
	return sm, nil
}

// helper to create a subMsg of type SUBMSG_ID_INFO_TS
func newTsSubMsg(t time.Time, order binary.ByteOrder) *subMsg {
	return &subMsg{
		hdr: submsgHeader{
			id:    SUBMSG_ID_INFO_TS,
			flags: FRUDP_FLAGS_LITTLE_ENDIAN,
			sz:    8,
		},
		data: timeToBytes(t, order),
	}
}

func (s *subMsg) WriteTo(w io.Writer) {
	s.hdr.WriteTo(w)
	w.Write(s.data)
}

type submsgData struct {
	hdr               submsgHeader
	extraflags        uint16
	octetsToInlineQos uint16
	readerID          EntityID
	writerID          EntityID
	writerSeqNum      SeqNum
	data              []uint8
}

func (s *submsgData) String() string {

	str := "submsgData " +
		"{extraflags: 0x%04x, octetsToInlineQos: %d, readerID: 0x%08x, writerID: 0x%08x, writerSeqNum: %v, data: %v}"

	return fmt.Sprintf(str, s.extraflags, s.octetsToInlineQos, s.readerID, s.writerID, s.writerSeqNum, s.data)
}

func (s *submsgData) WriteTo(w io.Writer) {
	s.hdr.WriteTo(w)

	b := make([]byte, 20)
	binary.LittleEndian.PutUint16(b[0:], s.extraflags)
	binary.LittleEndian.PutUint16(b[2:], s.octetsToInlineQos)
	binary.BigEndian.PutUint32(b[4:], uint32(s.readerID))
	binary.BigEndian.PutUint32(b[8:], uint32(s.writerID))
	binary.LittleEndian.PutUint32(b[12:], uint32(s.writerSeqNum>>32))
	binary.LittleEndian.PutUint32(b[16:], uint32(s.writerSeqNum&0xffffffff))
	w.Write(b)
	w.Write(s.data)
}

type submsgDataFrag struct {
	hdr                  submsgHeader
	extraflags           uint16
	octets_to_inline_qos uint16
	readerID             EntityID
	writerID             EntityID
	writerSN             SeqNum
	fragmentStartNum     uint32
	fragmentsInSubMsg    uint16
	fragmentSize         uint16
	sampleSize           uint32
	data                 []uint8
}

type submsgHeartbeat struct {
	hdr         submsgHeader
	readerEID   EntityID
	writerEID   EntityID
	firstSeqNum SeqNum
	lastSeqNum  SeqNum
	count       uint32
}

func newHeartbeatFromBytes(b []byte) (*submsgHeartbeat, error) {
	if len(b) < 28 {
		return nil, io.EOF
	}

	order := binary.LittleEndian
	return &submsgHeartbeat{
		readerEID:   EntityID(binary.BigEndian.Uint32(b[0:])),
		writerEID:   EntityID(binary.BigEndian.Uint32(b[4:])),
		firstSeqNum: newSeqNum(int32(order.Uint32(b[8:])), order.Uint32(b[12:])),
		lastSeqNum:  newSeqNum(int32(order.Uint32(b[16:])), order.Uint32(b[20:])),
		count:       order.Uint32(b[24:]),
	}, nil
}

func (s *submsgHeartbeat) WriteTo(w io.Writer) {
	s.hdr.WriteTo(w)

	b := make([]byte, 28)
	binary.BigEndian.PutUint32(b[0:], uint32(s.readerEID))
	binary.BigEndian.PutUint32(b[4:], uint32(s.writerEID))
	binary.LittleEndian.PutUint32(b[8:], uint32(s.firstSeqNum>>32))
	binary.LittleEndian.PutUint32(b[12:], uint32(s.firstSeqNum&0xffffffff))
	binary.LittleEndian.PutUint32(b[16:], uint32(s.lastSeqNum>>32))
	binary.LittleEndian.PutUint32(b[20:], uint32(s.lastSeqNum&0xffffffff))
	binary.LittleEndian.PutUint32(b[24:], s.count)
	w.Write(b)
}

type submsgGap struct {
	hdr      submsgHeader
	readerID EntityID
	writerID EntityID
	gapStart SeqNum
	gapEnd   SeqNumSet
}

type submsgAckNack struct {
	hdr           submsgHeader
	readerEID     EntityID
	writerEID     EntityID
	readerSNState SeqNumSet
	count         uint32
}

func (s *submsgAckNack) WriteTo(w io.Writer) {
	s.hdr.WriteTo(w)

	sz := 24 + s.readerSNState.BitMapWords()*4
	b := make([]byte, sz)
	binary.BigEndian.PutUint32(b[0:], uint32(s.readerEID))
	binary.BigEndian.PutUint32(b[4:], uint32(s.writerEID))

	binary.LittleEndian.PutUint32(b[8:], uint32(s.readerSNState.bitmapBase>>32))
	binary.LittleEndian.PutUint32(b[12:], uint32(s.readerSNState.bitmapBase&0xffffffff))
	binary.LittleEndian.PutUint32(b[16:], uint32(s.readerSNState.numBits))
	for i, n := range s.readerSNState.bitmap {
		binary.LittleEndian.PutUint32(b[20+i*4:], n)
	}
	binary.LittleEndian.PutUint32(b[sz-4:], s.count)
	w.Write(b)
}

type submsgInfoDest struct {
	guidPrefix GUIDPrefix
}

type submsgInfoSrc struct {
	unused     uint32
	version    ProtoVersion
	vid        VendorID
	guidPrefix GUIDPrefix
}

type paramID uint16

type paramListItem struct {
	pid   paramID
	value []uint8 // must be 32-bit aligned
}

func (p *paramListItem) WriteTo(w io.Writer) {
	var buf [4]byte
	binary.LittleEndian.PutUint16(buf[:], uint16(p.pid))
	binary.LittleEndian.PutUint16(buf[2:], uint16(len(p.value)))
	w.Write(buf[:])
	w.Write(p.value)
}

func newParamListItemFromBytes(bin binary.ByteOrder, b []byte) (*paramListItem, error) {
	sz := bin.Uint16(b[2:])
	if len(b) < int(sz+4) {
		return nil, io.EOF
	}

	return &paramListItem{
		pid:   paramID(bin.Uint16(b[0:])),
		value: b[4 : 4+sz],
	}, nil
}

func (p *paramListItem) valToString(bin binary.ByteOrder) (string, error) {
	if len(p.value) < 4 {
		return "", io.EOF
	}
	sz := int(bin.Uint32(p.value[0:]))
	if len(p.value) < 4+sz {
		return "", io.EOF
	}
	// encoded with null terminator, strip that out
	return string(p.value[4 : 4+sz-1]), nil
}

func packParamString(bin binary.ByteOrder, s string) []byte {
	b := make([]byte, (4+len(s)+1+3) & ^0x3) // must be 32-bit aligned
	bin.PutUint32(b[0:], uint32(len(s)+1))
	copy(b[4:], []byte(s))
	b[4+len(s)] = 0
	return b
}

func newParamList(bin binary.ByteOrder, b []byte) ([]*paramListItem, int, error) {
	var plist []*paramListItem
	n := 0

	for len(b) >= 4 {
		p, err := newParamListItemFromBytes(bin, b)
		if err != nil {
			return nil, 0, err
		}
		b = b[4+len(p.value):]
		n += 4 + len(p.value)
		// fmt.Printf("    param id 0x%x (len %d)\n", p.pid, len(p.value))
		if p.pid == PID_SENTINEL {
			break
		}
		plist = append(plist, p)
	}
	return plist, n, nil
}

type encapsulationScheme struct {
	scheme  uint16
	options uint16
}

func (es *encapsulationScheme) WriteTo(w io.Writer) {
	buf := make([]byte, 4)
	binary.BigEndian.PutUint16(buf, es.scheme)
	binary.LittleEndian.PutUint16(buf[2:], es.options)
	w.Write(buf)
}

func newSchemeFromBytes(bin binary.ByteOrder, b []byte) encapsulationScheme {
	// 检查字节切片长度，至少需要4字节（scheme: 2字节 + options: 2字节）
	if len(b) < 4 {
		// 返回默认的encapsulation scheme，避免panic
		return encapsulationScheme{
			scheme:  0, // 无效scheme
			options: 0,
		}
	}
	return encapsulationScheme{
		scheme:  binary.BigEndian.Uint16(b[0:]), // seems to always be BigEndian (?)
		options: bin.Uint16(b[2:]),
	}
}
