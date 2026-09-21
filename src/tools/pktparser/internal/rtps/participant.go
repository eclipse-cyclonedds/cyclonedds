package rtps

import (
	"fmt"
	"strings"
	"time"
	"unicode"
)

// "allows a participant to indicate that it only contains a
// subset of the possible builtin endpoints"
// bitmask of _BUILTIN_ENDPOINT_ values below
type builtinEndpointSet uint32

const (
	NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER       = (1 << 0)
	NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR        = (1 << 1)
	NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER       = (1 << 2)
	NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR        = (1 << 3)
	NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER      = (1 << 4)
	NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR       = (1 << 5)
	NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_PROXY_ANNOUNCER = (1 << 6) // undefined meaning
	NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_PROXY_DETECTOR  = (1 << 7) // undefined meaning
	NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_STATE_ANNOUNCER = (1 << 8) // undefined meaning
	NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_STATE_DETECTOR  = (1 << 9) // undefined meaning
	NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER  = (1 << 10)
	NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER  = (1 << 11)
)

type Participant struct {
	domainID         DomainID
	protoVer         ProtoVersion
	vid              VendorID
	guidPrefix       GUIDPrefix
	guid             GUID
	expectsInlineQoS bool
	defaultUcastLoc  locator
	defaultMcastLoc  locator
	metaUcastLoc     locator
	metaMcastLoc     locator
	leaseDuration    time.Duration
	builtinEndpoints builtinEndpointSet
	processName      string
	processID        string
	hostname         string
	autoCoreCode     []byte
	// Additional QoS and configuration data
	userData         []byte
	groupData        []byte
	topicData        []byte
	manualLivelinessCount uint32
	propertyList     []string // Parsed property list items
	staticDiscoveryData []byte   // PID_STATIC_DISCOVERY data
}

func removeUnprintableChars(str string) string {
	// 使用strings.Map函数和unicode.IsPrint函数过滤不可打印字符
	return strings.Map(func(r rune) rune {
		if unicode.IsPrint(r) {
			return r
		}
		return -1
	}, str)
}

func (p *Participant) String() string {
	str := `participant:{
		  domainID: 0x%04x,protoVer: %v,vid: %v,guidPrefix: %v,guid: %v,expectsInlineQoS: %v,
		  defaultUcastLoc: %v,
		  defaultMcastLoc: %v,
		  metaUcastLoc: %v,
		  metaMcastLoc: %v,
		  leaseDuration: %v,
		  builtinEndpoints: 0x%08x,processName: %v,processID: %v,hostname: %v,
		  autoCoreCode: %v,
		  staticDiscoveryData: %v,
	}`
	return fmt.Sprintf(str, p.domainID, p.protoVer, p.vid, p.guidPrefix, &p.guid, p.expectsInlineQoS, p.defaultUcastLoc, p.defaultMcastLoc, p.metaUcastLoc,
		p.metaMcastLoc, p.leaseDuration, p.builtinEndpoints, removeUnprintableChars(p.processName), p.processID, removeUnprintableChars(p.hostname), p.autoCoreCode, p.staticDiscoveryData)
}

func (p *Participant) GetGUID() GUID {
	return p.guid
}

func (p *Participant) GetGUIDPrefix() GUIDPrefix {
	return p.guidPrefix
}

func (p *Participant) GetDomainID() DomainID {
	return p.domainID
}

func (p *Participant) GetProcessName() string {
	return p.processName
}

func (p *Participant) GetProcessID() string {
	return p.processID
}

func (p *Participant) GetHostname() string {
	return p.hostname
}

func (p *Participant) GetAutoCoreCode() []byte {
	return p.autoCoreCode
}

func (p *Participant) GetBuiltinEndpoints() builtinEndpointSet {
	return p.builtinEndpoints
}

func (p *Participant) GetDefaultUnicastLocatorStr() string {
	return p.defaultUcastLoc.String()
}

func (p *Participant) GetDefaultMulticastLocatorStr() string {
	return p.defaultMcastLoc.String()
}

func (p *Participant) GetMetatrafficUnicastLocatorStr() string {
	return p.metaUcastLoc.String()
}

func (p *Participant) GetMetatrafficMulticastLocatorStr() string {
	return p.metaMcastLoc.String()
}

func (p *Participant) GetLeaseDuration() time.Duration {
	return p.leaseDuration
}

func (p *Participant) GetProtoVersion() ProtoVersion {
	return p.protoVer
}

func (p *Participant) GetVendorID() VendorID {
	return p.vid
}

func (p *Participant) GetExpectsInlineQoS() bool {
	return p.expectsInlineQoS
}

func (p *Participant) GetUserData() []byte {
	return p.userData
}

func (p *Participant) GetGroupData() []byte {
	return p.groupData
}

func (p *Participant) GetTopicData() []byte {
	return p.topicData
}

func (p *Participant) GetManualLivelinessCount() uint32 {
	return p.manualLivelinessCount
}

func (p *Participant) GetPropertyList() []string {
	return p.propertyList
}

func (p *Participant) GetStaticDiscoveryData() []byte {
	return p.staticDiscoveryData
}
