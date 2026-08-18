package rtps

import (
	"encoding/binary"
	"errors"
	"fmt"
	"regexp"
	"strings"
	"time"

	"autocore.ai/pktparser/pkg/errcode"
	"autocore.ai/pktparser/pkg/xlog"
)

// NetworkPacketInfo 存储网络包的基本信息
type NetworkPacketInfo struct {
	SrcIP        string
	DstIP        string
	SrcPort      uint16
	DstPort      uint16
	PacketLength int
	IsMulticast  bool
}

type Parser struct {
	logger        *xlog.Logger
	rxer          *receiver
	partTable     ParticipantTbl
	writerTable   WriterTbl
	readerTable   ReaderTbl
	userDataTable UserDataTbl
	processTable  ProcessTbl
	topicTable    TopicTbl
	locatorTable  LocatorTbl
	msgStats      map[string]int // Statistics for each submessage type
	networkStats  []NetworkPacketInfo // Network packet statistics
}

func NewParser() *Parser {
	p := &Parser{}
	p.partTable = NewParticipantTbl()
	p.writerTable = NewWriterTbl()
	p.readerTable = NewReaderTbl()
	p.userDataTable = NewUserDataTbl()
	p.processTable = NewProcessTbl()
	p.topicTable = NewTopicTbl()
	p.logger = xlog.NewLogger()
	p.locatorTable = NewLocatorTbl()
	p.msgStats = make(map[string]int)
	p.networkStats = make([]NetworkPacketInfo, 0)
	return p
}

func (p *Parser) SetLogLevel(level int16) {
	p.logger.SetLevel(level)
}

func (p *Parser) GetPartTable() ParticipantTbl {
	return p.partTable
}

func (p *Parser) GetWriterTable() WriterTbl {
	return p.writerTable
}

func (p *Parser) GetReaderTable() ReaderTbl {
	return p.readerTable
}

func (p *Parser) GetUserDataTable() UserDataTbl {
	return p.userDataTable
}

func (p *Parser) GetProcessTable() ProcessTbl {
	return p.processTable
}

func (p *Parser) GetTopicTable() TopicTbl {
	return p.topicTable
}

func (p *Parser) GetLocatorTable() LocatorTbl {
	return p.locatorTable
}

func (p *Parser) GetMsgStats() map[string]int {
	return p.msgStats
}

func (p *Parser) Parse(b []byte) (interface{}, error) {
	hdr, err := newHeaderFromBytes(b)
	if err != nil {
		return nil, err
	}

	if Magic != hdr.magic {
		return nil, errors.New("no magic here")
	}

	if hdr.protoVer.major < MY_RTPS_VERSION_MAJOR {
		return nil, errors.New("version too old")
	}
	p.logger.Debugf("\n Start Parse RTPS...\n")
	p.logger.Debugf("%s\n", hdr)

	rxer := receiver{
		srcProtoVer:   hdr.protoVer,
		srcVID:        hdr.vid,
		srcGUIDPrefix: hdr.guidPrefix,
	}

	p.rxer = &rxer

	submsgbuf := b[8+len(hdr.guidPrefix):]

	for len(submsgbuf) >= 4 {
		submsg, err := newSubMsgFromBytes(submsgbuf)
		if err != nil {
			p.logger.Errorf("[Parse] 解析子消息失败: %v，跳过此数据包", err.Error())
			break
		}
		
		// 验证子消息头部大小的合理性，防止无限循环
		if submsg.hdr.sz > uint16(len(submsgbuf)-4) {
			p.logger.Errorf("[Parse] 子消息大小异常：声明%d字节，但剩余数据只有%d字节", submsg.hdr.sz, len(submsgbuf)-4)
			break
		}
		
		p.parseSubMsg(submsg)
		submsgbuf = submsgbuf[4+submsg.hdr.sz:]
	}

	if len(submsgbuf) > 0 {
		p.logger.Warnf("oops, done processing with %v bytes remaining", len(submsgbuf))
	}

	p.logger.Debugf("\n End Parse RTPS...\n")
	return nil, nil
}

func (p *Parser) parseSubMsg(sm *subMsg) {
	switch sm.hdr.id {
	case SMID_ID_HE:
		p.logger.Debugf("SMID_ID_HE")
		p.msgStats["HE"]++

	case SUBMSG_ID_PAD:
		p.logger.Debugf("SUBMSG_ID_PAD")
		p.msgStats["PAD"]++

	case SUBMSG_ID_ACKNACK:
		p.logger.Debugf("SUBMSG_ID_ACKNACK")
		p.msgStats["ACKNACK"]++
		// r.rxAckNack(sm)

	case SUBMSG_ID_HEARTBEAT:
		p.logger.Debugf("SUBMSG_ID_HEARTBEAT")
		p.msgStats["HEARTBEAT"]++
		// r.rxHeartbeat(sm)

	case SUBMSG_ID_GAP:
		p.logger.Debugf("SUBMSG_ID_GAP")
		p.msgStats["GAP"]++

	case SUBMSG_ID_INFO_TS:
		p.logger.Debugf("SUBMSG_ID_INFO_TS")
		p.msgStats["INFO_TS"]++
		// r.rxInfoTS(sm)
		p.parseInfoTS(sm)

	case SUBMSG_ID_INFO_SRC:
		p.logger.Debugf("SUBMSG_ID_INFO_SRC")
		p.msgStats["INFO_SRC"]++
		// r.rxInfoSrc(sm)

	case SUBMSG_ID_INFO_REPLY_IP4:
		p.logger.Debugf("SUBMSG_ID_INFO_REPLY_IP4")
		p.msgStats["INFO_REPLY_IP4"]++

	case SUBMSG_ID_INFO_DST:
		p.logger.Debugf("SUBMSG_ID_INFO_DST")
		p.msgStats["INFO_DST"]++
		p.parseInfoDst(sm)

	case SUBMSG_ID_INFO_REPLY:
		p.logger.Debugf("SUBMSG_ID_INFO_REPLY")
		p.msgStats["INFO_REPLY"]++

	case SUBMSG_ID_NACK_FRAG:
		p.logger.Debugf("SUBMSG_ID_NACK_FRAG")
		p.msgStats["NACK_FRAG"]++

	case SUBMSG_ID_HEARTBEAT_FRAG:
		p.logger.Debugf("SUBMSG_ID_HEARTBEAT_FRAG")
		p.msgStats["HEARTBEAT_FRAG"]++

	case SUBMSG_ID_DATA:
		p.logger.Debugf("SUBMSG_ID_DATA")
		p.msgStats["DATA"]++
		p.parseData(sm)

	case SUBMSG_ID_DATA_FRAG:
		p.logger.Debugf("SUBMSG_ID_DATA_FRAG")
		p.msgStats["DATA_FRAG"]++

	// Security-specific submessages
	case SMID_ID_SEC_BODY:
		p.logger.Debugf("SMID_ID_SEC_BODY")
		p.msgStats["SEC_BODY"]++
		p.parseSecBody(sm)

	case SMID_ID_SEC_PREFIX:
		p.logger.Debugf("SMID_ID_SEC_PREFIX")
		p.msgStats["SEC_PREFIX"]++
		p.parseSecPrefix(sm)

	case SMID_ID_SEC_POSTFIX:
		p.logger.Debugf("SMID_ID_SEC_POSTFIX")
		p.msgStats["SEC_POSTFIX"]++
		p.parseSecPostfix(sm)

	case SMID_ID_SRTPS_PREFIX:
		p.logger.Debugf("SMID_ID_SRTPS_PREFIX")
		p.msgStats["SRTPS_PREFIX"]++
		p.parseSRtpsPrefix(sm)

	case SMID_ID_SRTPS_POSTFIX:
		p.logger.Debugf("SMID_ID_SRTPS_POSTFIX")
		p.msgStats["SRTPS_POSTFIX"]++
		p.parseSRtpsPostfix(sm)

	default:
		p.logger.Debugf("**** SUBMSG_ID_UNKNOWN (0x%02x)", sm.hdr.id)
		p.msgStats["UNKNOWN"]++
	}
}

func (p *Parser) parseInfoDst(sm *subMsg) {
	// only element in submsgInfoDest is the prefix
	if len(sm.data) == UDPGuidPrefixLen {
		p.rxer.dstGUIDPrefix = sm.data
		p.logger.Debugf("dstGUIDPrefix:%v", p.rxer.dstGUIDPrefix)
	}
}

// handler for SUBMSG_ID_INFO_TS submessages
func (p *Parser) parseInfoTS(sm *subMsg) {
	invalidate := sm.hdr.flags&FLAGS_INFOTS_INVALIDATE != 0
	if invalidate {
		p.rxer.haveTimestamp = false
		p.rxer.timestamp = timeInvalid
		p.logger.Warnf("INFO_TS: invalidate")
	} else {
		var err error
		if p.rxer.timestamp, err = timeFromBytes(sm.bin, sm.data); err == nil {
			p.rxer.haveTimestamp = true
			p.logger.Debugf("INFO_TS: %v now: %v\n", p.rxer.timestamp, time.Now().UTC())
		}
	}
}

// Parser for SUBMSG_ID_DATA submessages
func (p *Parser) parseData(sm *subMsg) {
	inlineQoS := sm.hdr.flags&FLAGS_DATA_INLINE_QOS != 0
	//d := sm.hdr.flags & FLAGS_DATA_DATAFLAG
	keyed := sm.hdr.flags&FLAGS_DATA_KEYFLAG != 0
	if keyed {
		p.logger.Warnf("how to keyed data")
		return
	}

	// 检查子消息数据长度是否足够解析submsgData结构（至少需要20字节）
	if len(sm.data) < 20 {
		p.logger.Errorf("[parseData] 子消息数据不足，需要至少20字节但只有%d字节", len(sm.data))
		return
	}

	// additional data-specific header info
	smd := submsgData{
		extraflags:        sm.bin.Uint16(sm.data[0:]),
		octetsToInlineQos: sm.bin.Uint16(sm.data[2:]),
		readerID:          EntityID(binary.BigEndian.Uint32(sm.data[4:])),
		writerID:          EntityID(binary.BigEndian.Uint32(sm.data[8:])),
		writerSeqNum:      newSeqNum(int32(sm.bin.Uint32(sm.data[12:])), sm.bin.Uint32(sm.data[16:])),
		data:              sm.data[20:],
	}

	p.logger.Debugf("%s", &smd)

	b := smd.data

	// parse and apply QoS parameters
	if inlineQoS {
		p.logger.Debugf("inlineQoS flag seted.")
		b = sm.data[4+smd.octetsToInlineQos:]
		_, n, err := newParamList(sm.bin, b)
		if err != nil {
			p.logger.Errorf("[parseData] newParamList Error: %v", err.Error())
			return
		}
		// for _, p := range plist {
		// 	// XXX: apply p
		// }
		b = b[n:]
	}

	// 验证剩余数据长度是否足够读取encapsulation scheme（至少4字节）
	if len(b) < 4 {
		p.logger.Errorf("[parseData] 数据不足，无法读取encapsulation scheme，需要4字节但只有%d字节", len(b))
		return
	}

	es := newSchemeFromBytes(sm.bin, b)
	switch es.scheme {
	case SCHEME_CDR_LE, SCHEME_PL_CDR_LE:
	case 0:
		// scheme为0表示数据格式错误或长度不足
		p.logger.Errorf("[parseData] 无效的encapsulation scheme，可能是数据截断或格式错误")
		return
	default:
		p.logger.Warnf("scheme unknown: %d", es.scheme)
	}
	b = b[4:]

	p.logger.Debugf("scheme : %d", es.scheme)

	// writerGUID := GUID{
	// 	prefix: p.rxer.srcGUIDPrefix,
	// 	eid:    smd.writerID,
	// }

	if p.isDataP(&smd) {
		p.logger.Debugf("DataP Appeared...")
		p.parseDataP(sm, es.scheme, b)
	} else if p.isDataR(&smd) {
		p.logger.Debugf("DataR Appeared...")
		p.parseDataRW(sm, es.scheme, b, true)
	} else if p.isDataW(&smd) {
		p.logger.Debugf("DataW Appeared...")
		p.parseDataRW(sm, es.scheme, b, false)
	} else if p.isDataM(&smd) {
		p.logger.Debugf("DataM Appeared...")
	} else {
		p.logger.Debugf("Data Appeared...")
		p.logger.Debugf("timestamp : %v\n", p.rxer.timestamp)
		userDataInfo := &userDataItem{}
		userDataInfo.guid.prefix = p.rxer.srcGUIDPrefix
		userDataInfo.guid.eid = smd.writerID
		userDataInfo.writerSeqNum = smd.writerSeqNum
		userDataInfo.ts = p.rxer.timestamp
		userDataInfo.dataLen = len(smd.data)
		p.logger.Debugf("userDataInfo: %s\n", userDataInfo)
		p.userDataTable.Add(userDataInfo.guid, userDataInfo.writerSeqNum, userDataInfo.dataLen, userDataInfo.ts)
	}
}

func (p *Parser) isDataP(s *submsgData) bool {
	// fmt.Printf("[isDataP] s.writerID: 0x%08x, s.readerID: 0x%08x\n", s.writerID, s.readerID)
	return s.writerID == ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER && (s.readerID == ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER || s.readerID == ENTITYID_UNKNOWN)
}

func (p *Parser) isDataR(s *submsgData) bool {
	// fmt.Printf("[isDataR] s.writerID: 0x%08x, s.readerID: 0x%08x\n", s.writerID, s.readerID)
	return s.writerID == ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER && (s.readerID == ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER || s.readerID == ENTITYID_UNKNOWN)
}

func (p *Parser) isDataW(s *submsgData) bool {
	// fmt.Printf("[isDataW] s.writerID: 0x%08x, s.readerID: 0x%08x\n", s.writerID, s.readerID)
	return s.writerID == ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER && (s.readerID == ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER || s.readerID == ENTITYID_UNKNOWN)
}

func (p *Parser) isDataM(s *submsgData) bool {
	// fmt.Printf("[isDataW] s.writerID: 0x%08x, s.readerID: 0x%08x\n", s.writerID, s.readerID)
	return s.writerID == ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER && (s.readerID == ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER || s.readerID == ENTITYID_UNKNOWN)
}

// called when data has been received for ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER
func (p *Parser) parseDataP(submsg *subMsg, scheme uint16, b []byte) {
	if scheme != SCHEME_PL_CDR_LE {
		p.logger.Errorf("expected spdp data to be PL_CDR_LE. bailing...")
		return
	}

	logger := p.logger
	plist, _, err := newParamList(submsg.bin, b)
	if err != nil {
		logger.Errorf("[parseDataP] newParamList err: %v", err.Error())
		return
	}

	part := &Participant{}

	for _, p := range plist {

		// if p.pid&0x8000 != 0 {
		// 	// ignoring vendor specific params for now
		// 	logger.Debugf("     ignoring vendor specific params for now... (0x%x len %d)\n", p.pid, len(p.value))
		// 	continue
		// }

		switch p.pid {
		case PID_DOMAIN_ID:
			logger.Debugf("PID_DOMAIN_ID Appeared...")
			part.domainID = DomainID(binary.LittleEndian.Uint16(p.value[0:]))

		case PID_PROTOCOL_VERSION:
			logger.Debugf("PID_PROTOCOL_VERSION Appeared...")
			part.protoVer = ProtoVersion{p.value[0], p.value[1]}

		case PID_VENDOR_ID:
			logger.Debugf("PID_VENDOR_ID Appeared...")
			part.vid = VendorID(binary.BigEndian.Uint16(p.value[0:]))

		case PID_DEFAULT_UNICAST_LOCATOR:
			logger.Debugf("PID_DEFAULT_UNICAST_LOCATOR Appeared...")
			if part.defaultUcastLoc, err = newUDPv4LocFromBytes(submsg.bin, p.value); err == nil {
				if part.defaultUcastLoc.kind == LOCATOR_KIND_UDPV4 {
					logger.Debugf("PID_DEFAULT_UNICAST_LOCATOR part.defaultUcastLoc.kind == LOCATOR_KIND_UDPV4")
				}
			}

		case PID_DEFAULT_MULTICAST_LOCATOR:
			logger.Debugf("PID_DEFAULT_MULTICAST_LOCATOR Appeared...")
			if part.defaultMcastLoc, err = newUDPv4LocFromBytes(submsg.bin, p.value); err == nil {
				if part.defaultMcastLoc.kind == LOCATOR_KIND_UDPV4 {
					logger.Debugf("PID_DEFAULT_MULTICAST_LOCATOR part.defaultMcastLoc.kind == LOCATOR_KIND_UDPV4")
				} else {
					logger.Warnf("        spdp unknown mcast locator kind: %v", part.defaultMcastLoc.kind)
				}
			}

		case PID_METATRAFFIC_UNICAST_LOCATOR:
			logger.Debugf("PID_METATRAFFIC_UNICAST_LOCATOR Appeared...")
			if part.metaUcastLoc, err = newUDPv4LocFromBytes(submsg.bin, p.value); err == nil {
				if part.metaUcastLoc.kind == LOCATOR_KIND_UDPV4 {
					logger.Debugf("PID_METATRAFFIC_UNICAST_LOCATOR part.metaUcastLoc.kind == LOCATOR_KIND_UDPV4")
				} else if part.metaUcastLoc.kind == LOCATOR_KIND_UDPV6 {
					logger.Debugf("ignore ip6 for now...")
				} else {
					logger.Warnf("        spdp unknown metatraffic mcast locator kind:%v", part.metaUcastLoc.kind)
				}
			}

		case PID_METATRAFFIC_MULTICAST_LOCATOR:
			logger.Debugf("PID_METATRAFFIC_MULTICAST_LOCATOR Appeared...")
			if part.metaMcastLoc, err = newUDPv4LocFromBytes(submsg.bin, p.value); err == nil {
				if part.metaMcastLoc.kind == LOCATOR_KIND_UDPV4 {
					logger.Debugf("PID_METATRAFFIC_MULTICAST_LOCATOR part.metaMcastLoc.kind == LOCATOR_KIND_UDPV4")
				} else if part.metaMcastLoc.kind == LOCATOR_KIND_UDPV6 {
					logger.Debugf("ignore ip6 for now...")
				} else {
					logger.Warnf("        spdp unknown metatraffic mcast locator kind:%v", part.metaMcastLoc.kind)
				}
			}

		case PID_PARTICIPANT_LEASE_DURATION:
			logger.Debugf("PID_PARTICIPANT_LEASE_DURATION Appeared...")
			if dur, err := durationFromBytes(submsg.bin, p.value); err == nil {
				part.leaseDuration = dur
			}

		case PID_USER_DATA:
			logger.Debugf("[parseDataP] PID_USER_DATA Appeared...")
			part.userData = make([]byte, len(p.value))
			copy(part.userData, p.value)

		case PID_GROUP_DATA:
			logger.Debugf("[parseDataP] PID_GROUP_DATA Appeared...")
			part.groupData = make([]byte, len(p.value))
			copy(part.groupData, p.value)

		case PID_TOPIC_DATA:
			logger.Debugf("[parseDataP] PID_TOPIC_DATA Appeared...")
			part.topicData = make([]byte, len(p.value))
			copy(part.topicData, p.value)

		case PID_TOPIC_NAME:
			logger.Debugf("PID_TOPIC_NAME Appeared...")

		case PID_TYPE_NAME:
			logger.Debugf("PID_TYPE_NAME Appeared...")

		case PID_PARTICIPANT_GUID:
			logger.Debugf("PID_PARTICIPANT_GUID Appeared...")
			part.guidPrefix = p.value[:UDPGuidPrefixLen]
			part.guid = GUID{part.guidPrefix, EntityID(binary.BigEndian.Uint32(p.value[UDPGuidPrefixLen:]))}

		case PID_BUILTIN_ENDPOINT_SET:
			logger.Debugf("PID_BUILTIN_ENDPOINT_SET Appeared...")
			part.builtinEndpoints = builtinEndpointSet(binary.LittleEndian.Uint32(p.value[0:]))

		case PID_PARTICIPANT_MANUAL_LIVELINESS_COUNT:
			logger.Debugf("PID_PARTICIPANT_MANUAL_LIVELINESS_COUNT Appeared...")
			if len(p.value) >= 4 {
				part.manualLivelinessCount = binary.LittleEndian.Uint32(p.value[0:])
			}

		case PID_METATRAFFIC_UNICAST_IPADDRESS:
			logger.Debugf("PID_METATRAFFIC_UNICAST_IPADDRESS Appeared...")
			// Legacy parameter for IPv4 metatraffic

		case PID_METATRAFFIC_MULTICAST_PORT:
			logger.Debugf("PID_METATRAFFIC_MULTICAST_PORT Appeared...")
			// Metatraffic multicast port

		case PID_DEFAULT_UNICAST_IPADDRESS:
			logger.Debugf("PID_DEFAULT_UNICAST_IPADDRESS Appeared...")
			// Legacy parameter for IPv4 default unicast

		case PID_DEFAULT_UNICAST_PORT:
			logger.Debugf("PID_DEFAULT_UNICAST_PORT Appeared...")
			// Default unicast port

		case PID_METATRAFFIC_UNICAST_PORT:
			logger.Debugf("PID_METATRAFFIC_UNICAST_PORT Appeared...")
			// Metatraffic unicast port

		case PID_EXPECTS_INLINE_QOS:
			logger.Debugf("PID_EXPECTS_INLINE_QOS Appeared...")
			if len(p.value) >= 1 {
				part.expectsInlineQoS = p.value[0] != 0
			}

		case PID_PARTICIPANT_BUILTIN_ENDPOINTS:
			logger.Debugf("PID_PARTICIPANT_BUILTIN_ENDPOINTS Appeared...")
			if len(p.value) >= 4 {
				part.builtinEndpoints = builtinEndpointSet(binary.LittleEndian.Uint32(p.value[0:]))
			}

		case PID_PROPERTY_LIST:
			logger.Debugf("PID_PROPERTY_LIST Appeared...")
			if len(p.value) < 16 {
				logger.Warnf("PID_PROPERTY_LIST too short len(p.value) == %d", len(p.value))
				break
			}
			part.autoCoreCode = make([]byte, 16)
			copy(part.autoCoreCode, p.value[len(p.value)-16:])
			result := strings.Split(string(p.value[:len(p.value)-16]), "__")
			// result := strings.Split(string(p.value), "__")
			logger.Debugf("result: %v\n", result)
			
			// Store the complete property list for display
			part.propertyList = make([]string, 0)
			for _, v := range result {
				if strings.TrimSpace(v) != "" {
					part.propertyList = append(part.propertyList, strings.TrimSpace(v))
				}
				
				if strings.HasPrefix(v, "ProcessName") {
					part.processName = strings.TrimPrefix(v, "ProcessName")
					part.processName = strings.TrimPrefix(part.processName, "\n")
					continue
				}

				if strings.HasPrefix(v, "Pid") {
					part.processID = strings.TrimPrefix(v, "Pid")
					// 正则表达式匹配数字部分
					re := regexp.MustCompile("[0-9]+")
					digits := re.FindAllString(part.processID, -1)

					// 拼接数字部分
					resultstr := ""
					for _, digit := range digits {
						resultstr += digit
					}
					part.processID = resultstr
					continue
				}

				if strings.HasPrefix(v, "Hostname") {
					part.hostname = strings.TrimPrefix(v, "Hostname")
					part.hostname = strings.Trim(part.hostname, " ")
					continue
				}

				// if strings.HasPrefix(v, "AutocoreCode") {
				// 	tmpStr := strings.TrimPrefix(v, "AutocoreCode")
				// 	part.autoCoreCode = make([]byte, len(tmpStr))
				// 	copy(part.autoCoreCode, []byte(tmpStr[:]))
				// 	continue
				// }
			}

		case PID_STATIC_DISCOVERY:
			logger.Debugf("PID_STATIC_DISCOVERY Appeared...")
			part.staticDiscoveryData = make([]byte, len(p.value))
			copy(part.staticDiscoveryData, p.value)
			logger.Debugf("PID_STATIC_DISCOVERY data length: %d bytes", len(p.value))

		default:
			logger.Debugf("      unhandled spdp rx param 0x%x len %d\n", p.pid, len(p.value))
		}
	}
    // 如果是domain 5 ， 没有PID_STATIC_DISCOVERY  需要打印一条携带guidPrefix的消息
	if part.domainID == 5 && len(part.staticDiscoveryData) == 0 {
		fmt.Println("domain 5, no PID_STATIC_DISCOVERY data %s process name is %s", part.guidPrefix.String(), part.processName)
	}
	logger.Debugf("part: %s\n", part)
	p.processTable.Add(part.guid, part.processName, part.processID)
	p.partTable.Add(part)
	p.locatorTable.Add(part.defaultUcastLoc, part.guid, part.processName)

	// now that we have stuff the "part" buffer, spin through our
	// participant list and see if we already have this one
	// if _, found := defaultSession.findParticipant(part.guidPrefix); found {
	// 	// TODO: see if anything has changed. update if needed
	// } else {
	// 	fmt.Printf("new participant in slot %d: %s\n", len(defaultSession.discoParticipants), part.guidPrefix.String())
	// 	defaultSession.addParticipant(part)
	// 	defaultSession.sedp.addBuiltinEndpoints(part)
	// }
}

func (p *Parser) parseDataRW(submsg *subMsg, scheme uint16, b []byte, isR bool) {
	if scheme != SCHEME_PL_CDR_LE {
		// report err
		return
	}

	plist, _, err := newParamList(submsg.bin, b)
	if err != nil {
		p.logger.Errorf("[parseDataRW] newParamList err:%v", err.Error())
		return
	}

	topicInfo := &sedpTopicInfoItem{
		isR: isR,
	}

	// QoS parameters map for the new AddWithQoS method
	qosParams := make(map[string]string)

	logger := p.logger
	for _, param := range plist {

		switch param.pid {
		case PID_ENDPOINT_GUID:
			logger.Debugf("[parseDataRW] PID_ENDPOINT_GUID Appeared...")
			topicInfo.guid = guidFromBytes(param.value)

		case PID_TOPIC_NAME:
			logger.Debugf("[parseDataRW] PID_TOPIC_NAME Appeared...")
			if str, err := param.valToString(submsg.bin); err == nil {
				topicInfo.topicName = str
			}

		case PID_TYPE_NAME:
			logger.Debugf("[parseDataRW] PID_TYPE_NAME Appeared...")
			if str, err := param.valToString(submsg.bin); err == nil {
				topicInfo.typeName = str
			}

		case PID_PARTICIPANT_LEASE_DURATION:
			logger.Debugf("[parseDataRW] PID_PARTICIPANT_LEASE_DURATION Appeared...")
			if len(param.value) >= 8 {
				secs := binary.LittleEndian.Uint32(param.value[0:])
				nanos := binary.LittleEndian.Uint32(param.value[4:])
				qosParams["ParticipantLeaseDuration"] = fmt.Sprintf("%d.%09d", secs, nanos)
			}

		case PID_DOMAIN_ID:
			logger.Debugf("[parseDataRW] PID_DOMAIN_ID Appeared...")
			if len(param.value) >= 2 {
				domainID := binary.LittleEndian.Uint16(param.value[0:])
				qosParams["DomainID"] = fmt.Sprintf("%d", domainID)
			}

		case PID_RELIABILITY:
			logger.Debugf("[parseDataRW] PID_RELIABILITY Appeared...")
			if len(param.value) >= 1 {
				reliabilityKind := param.value[0]
				switch reliabilityKind {
				case 1:
					qosParams["Reliability"] = "BEST_EFFORT"
				case 2:
					qosParams["Reliability"] = "RELIABLE"
				default:
					qosParams["Reliability"] = fmt.Sprintf("UNKNOWN(%d)", reliabilityKind)
				}
			}

		case PID_LIVELINESS:
			logger.Debugf("[parseDataRW] PID_LIVELINESS Appeared...")
			if len(param.value) >= 1 {
				livelinessKind := param.value[0]
				switch livelinessKind {
				case 0:
					qosParams["Liveliness"] = "AUTOMATIC"
				case 1:
					qosParams["Liveliness"] = "MANUAL_BY_PARTICIPANT"
				case 2:
					qosParams["Liveliness"] = "MANUAL_BY_TOPIC"
				default:
					qosParams["Liveliness"] = fmt.Sprintf("UNKNOWN(%d)", livelinessKind)
				}
			}

		case PID_DURABILITY:
			logger.Debugf("[parseDataRW] PID_DURABILITY Appeared...")
			if len(param.value) >= 1 {
				durabilityKind := param.value[0]
				switch durabilityKind {
				case 0:
					qosParams["Durability"] = "VOLATILE"
				case 1:
					qosParams["Durability"] = "TRANSIENT_LOCAL"
				case 2:
					qosParams["Durability"] = "TRANSIENT"
				case 3:
					qosParams["Durability"] = "PERSISTENT"
				default:
					qosParams["Durability"] = fmt.Sprintf("UNKNOWN(%d)", durabilityKind)
				}
			}

		case PID_PRESENTATION:
			logger.Debugf("[parseDataRW] PID_PRESENTATION Appeared...")
			if len(param.value) >= 1 {
				presentationKind := param.value[0]
				switch presentationKind {
				case 0:
					qosParams["Presentation"] = "INSTANCE"
				case 1:
					qosParams["Presentation"] = "TOPIC"
				case 2:
					qosParams["Presentation"] = "GROUP"
				default:
					qosParams["Presentation"] = fmt.Sprintf("UNKNOWN(%d)", presentationKind)
				}
			}

		case PID_PARTITION:
			logger.Debugf("[parseDataRW] PID_PARTITION Appeared...")
			if str, err := param.valToString(submsg.bin); err == nil {
				qosParams["Partition"] = str
			}

		case PID_HISTORY:
			logger.Debugf("[parseDataRW] PID_HISTORY Appeared...")
			if len(param.value) >= 1 {
				historyKind := param.value[0]
				switch historyKind {
				case 0:
					qosParams["History"] = "KEEP_LAST"
				case 1:
					qosParams["History"] = "KEEP_ALL"
				default:
					qosParams["History"] = fmt.Sprintf("UNKNOWN(%d)", historyKind)
				}
				if len(param.value) >= 4 {
					depth := binary.LittleEndian.Uint32(param.value[4:])
					qosParams["History"] += fmt.Sprintf("(depth:%d)", depth)
				}
			}

		case PID_TRANSPORT_PRIORITY:
			logger.Debugf("[parseDataRW] PID_TRANSPORT_PRIORITY Appeared...")
			if len(param.value) >= 4 {
				priority := binary.LittleEndian.Uint32(param.value[0:])
				qosParams["TransportPriority"] = fmt.Sprintf("%d", priority)
			}

		case PID_KEY_HASH:
			logger.Debugf("[parseDataRW] PID_KEY_HASH Appeared...")
			if len(param.value) >= 16 {
				keyHash := fmt.Sprintf("%x", param.value[:16])
				qosParams["KeyHash"] = keyHash
			}

		case PID_OWNERSHIP_STRENGTH:
			logger.Debugf("[parseDataRW] PID_OWNERSHIP_STRENGTH Appeared...")
			if len(param.value) >= 4 {
				strength := binary.LittleEndian.Uint32(param.value[0:])
				qosParams["OwnershipStrength"] = fmt.Sprintf("%d", strength)
			}

		case PID_DURABILITY_SERVICE:
			logger.Debugf("[parseDataRW] PID_DURABILITY_SERVICE Appeared...")
			if len(param.value) >= 28 {
				serviceCleanupDelay := binary.LittleEndian.Uint32(param.value[0:])
				historyKind := binary.LittleEndian.Uint32(param.value[8:])
				historyDepth := binary.LittleEndian.Uint32(param.value[12:])
				maxSamples := binary.LittleEndian.Uint32(param.value[16:])
				maxInstances := binary.LittleEndian.Uint32(param.value[20:])
				maxSamplesPerInstance := binary.LittleEndian.Uint32(param.value[24:])
				qosParams["DurabilityService"] = fmt.Sprintf("cleanup_delay:%d, history:%d(depth:%d), max_samples:%d, max_instances:%d, max_samples_per_instance:%d",
					serviceCleanupDelay, historyKind, historyDepth, maxSamples, maxInstances, maxSamplesPerInstance)
			}

		case PID_DEADLINE:
			logger.Debugf("[parseDataRW] PID_DEADLINE Appeared...")
			if len(param.value) >= 8 {
				secs := binary.LittleEndian.Uint32(param.value[0:])
				nanos := binary.LittleEndian.Uint32(param.value[4:])
				qosParams["Deadline"] = fmt.Sprintf("%d.%09d", secs, nanos)
			}

		case PID_DESTINATION_ORDER:
			logger.Debugf("[parseDataRW] PID_DESTINATION_ORDER Appeared...")
			if len(param.value) >= 1 {
				orderKind := param.value[0]
				switch orderKind {
				case 0:
					qosParams["DestinationOrder"] = "BY_RECEPTION_TIMESTAMP"
				case 1:
					qosParams["DestinationOrder"] = "BY_SOURCE_TIMESTAMP"
				default:
					qosParams["DestinationOrder"] = fmt.Sprintf("UNKNOWN(%d)", orderKind)
				}
			}

		case PID_LATENCY_BUDGET:
			logger.Debugf("[parseDataRW] PID_LATENCY_BUDGET Appeared...")
			if len(param.value) >= 8 {
				secs := binary.LittleEndian.Uint32(param.value[0:])
				nanos := binary.LittleEndian.Uint32(param.value[4:])
				qosParams["LatencyBudget"] = fmt.Sprintf("%d.%09d", secs, nanos)
			}

		case PID_LIFESPAN:
			logger.Debugf("[parseDataRW] PID_LIFESPAN Appeared...")
			if len(param.value) >= 8 {
				secs := binary.LittleEndian.Uint32(param.value[0:])
				nanos := binary.LittleEndian.Uint32(param.value[4:])
				qosParams["Lifespan"] = fmt.Sprintf("%d.%09d", secs, nanos)
			}

		case PID_USER_DATA:
			logger.Debugf("[parseDataRW] PID_USER_DATA Appeared...")
			if str, err := param.valToString(submsg.bin); err == nil {
				qosParams["UserData"] = str
			}

		case PID_GROUP_DATA:
			logger.Debugf("[parseDataRW] PID_GROUP_DATA Appeared...")
			if str, err := param.valToString(submsg.bin); err == nil {
				qosParams["GroupData"] = str
			}

		case PID_TOPIC_DATA:
			logger.Debugf("[parseDataRW] PID_TOPIC_DATA Appeared...")
			if str, err := param.valToString(submsg.bin); err == nil {
				qosParams["TopicData"] = str
			}

		case PID_RESOURCE_LIMITS:
			logger.Debugf("[parseDataRW] PID_RESOURCE_LIMITS Appeared...")
			if len(param.value) >= 12 {
				maxSamples := binary.LittleEndian.Uint32(param.value[0:])
				maxInstances := binary.LittleEndian.Uint32(param.value[4:])
				maxSamplesPerInstance := binary.LittleEndian.Uint32(param.value[8:])
				qosParams["ResourceLimits"] = fmt.Sprintf("max_samples:%d, max_instances:%d, max_samples_per_instance:%d",
					maxSamples, maxInstances, maxSamplesPerInstance)
			}

		case PID_OWNERSHIP:
			logger.Debugf("[parseDataRW] PID_OWNERSHIP Appeared...")
			if len(param.value) >= 1 {
				ownershipKind := param.value[0]
				switch ownershipKind {
				case 0:
					qosParams["Ownership"] = "SHARED"
				case 1:
					qosParams["Ownership"] = "EXCLUSIVE"
				default:
					qosParams["Ownership"] = fmt.Sprintf("UNKNOWN(%d)", ownershipKind)
				}
			}

		case PID_TIME_BASED_FILTER:
			logger.Debugf("[parseDataRW] PID_TIME_BASED_FILTER Appeared...")
			if len(param.value) >= 8 {
				secs := binary.LittleEndian.Uint32(param.value[0:])
				nanos := binary.LittleEndian.Uint32(param.value[4:])
				qosParams["TimeBasedFilter"] = fmt.Sprintf("%d.%09d", secs, nanos)
			}

		default:
			logger.Debugf("[parseDataRW]      unhandled sedp rx param 0x%x len %d\n", param.pid, len(param.value))
		}
	}

	logger.Debugf("[parseDataRW] topicInfo: %s\n", topicInfo)
	
	// 如果是domain 5 的datar  dataw， 需要打印一条携带guidPrefix的消息
	// 第一步先通过 topicInfo.guid 获取到guidPrefix， 在通过guidPrefix 获取到participant， 在通过participant 获取到domain id
	// 第二步再通过 domain id 如果是domain 5 的datar  dataw， 需要打印一条携带guidPrefix的消息
	{
		// 第一步：通过topicInfo.guid获取guidPrefix
		guidPrefix := topicInfo.guid.prefix
		// fmt.Printf("Looking for participant with guidPrefix: %s\n", guidPrefix.String())

		// 第二步：通过guidPrefix获取participant（因为participant和topic有相同的guidPrefix但不同的EntityID）
		if participant, err := p.partTable.GetByGUIDPrefix(guidPrefix); err == errcode.RTE_OK && participant != nil {
			// 第三步：检查participant的domain ID是否为5
			// fmt.Printf("Found participant with domain id: %d\n", participant.domainID)
			if participant.domainID == 5 {
				// 第四步：打印domain 5的DataR/DataW消息
				dataType := "DataR"
				if !isR {
					dataType = "DataW"
				}
				fmt.Printf("domain 5,process name %s , guidPrefix: %s, topic: %s  send %s \n",
					participant.processName, guidPrefix.String(), topicInfo.topicName, dataType)
			}
		} else {
			fmt.Printf("Get datar/w first, datap message doesn't exist, guidPrefix: %s\n", guidPrefix.String())
		}
	}
	// Use AddWithQoS if we have QoS parameters, otherwise use the basic Add method
	if len(qosParams) > 0 {
		if isR {
			p.readerTable.AddWithQoS(topicInfo.guid, topicInfo.topicName, topicInfo.typeName, qosParams)
			logger.Debugf("[parseDataRW] ReaderTblItem added with QoS: GUID=%s, TopicName=%s, TypeName=%s, QoS=%v\n", 
				topicInfo.guid.String(), topicInfo.topicName, topicInfo.typeName, qosParams)
		} else {
			p.writerTable.AddWithQoS(topicInfo.guid, topicInfo.topicName, topicInfo.typeName, qosParams)
			logger.Debugf("[parseDataRW] WriterTblItem added with QoS: GUID=%s, TopicName=%s, TypeName=%s, QoS=%v\n", 
				topicInfo.guid.String(), topicInfo.topicName, topicInfo.typeName, qosParams)
		}
	} else {
		if isR {
			p.readerTable.Add(topicInfo.guid, topicInfo.topicName, topicInfo.typeName)
		} else {
			p.writerTable.Add(topicInfo.guid, topicInfo.topicName, topicInfo.typeName)
		}
	}
	p.topicTable.Add(topicInfo.topicName, topicInfo.typeName)
}

// parseSecBody handles SEC_BODY submessages (0x30)
func (p *Parser) parseSecBody(sm *subMsg) {
	p.logger.Debugf("Parsing SEC_BODY submessage, data length: %d", len(sm.data))
	// SEC_BODY contains encrypted payload data
	// For now, just log the presence and basic info
	if len(sm.data) > 0 {
		p.logger.Debugf("SEC_BODY encrypted payload size: %d bytes", len(sm.data))
	}
}

// parseSecPrefix handles SEC_PREFIX submessages (0x31)
func (p *Parser) parseSecPrefix(sm *subMsg) {
	p.logger.Debugf("Parsing SEC_PREFIX submessage, data length: %d", len(sm.data))
	// SEC_PREFIX contains security header information
	// For now, just log the presence and basic info
	if len(sm.data) > 0 {
		p.logger.Debugf("SEC_PREFIX security header size: %d bytes", len(sm.data))
	}
}

// parseSecPostfix handles SEC_POSTFIX submessages (0x32)
func (p *Parser) parseSecPostfix(sm *subMsg) {
	p.logger.Debugf("Parsing SEC_POSTFIX submessage, data length: %d", len(sm.data))
	// SEC_POSTFIX contains security footer/authentication information
	// For now, just log the presence and basic info
	if len(sm.data) > 0 {
		p.logger.Debugf("SEC_POSTFIX security footer size: %d bytes", len(sm.data))
	}
}

// parseSRtpsPrefix handles SRTPS_PREFIX submessages (0x33)
func (p *Parser) parseSRtpsPrefix(sm *subMsg) {
	p.logger.Debugf("Parsing SRTPS_PREFIX submessage, data length: %d", len(sm.data))
	// SRTPS_PREFIX contains secure RTPS prefix information
	// For now, just log the presence and basic info
	if len(sm.data) > 0 {
		p.logger.Debugf("SRTPS_PREFIX secure RTPS header size: %d bytes", len(sm.data))
	}
}

// parseSRtpsPostfix handles SRTPS_POSTFIX submessages (0x34)
func (p *Parser) parseSRtpsPostfix(sm *subMsg) {
	p.logger.Debugf("Parsing SRTPS_POSTFIX submessage, data length: %d", len(sm.data))
	// SRTPS_POSTFIX contains secure RTPS postfix information
	// For now, just log the presence and basic info
	if len(sm.data) > 0 {
		p.logger.Debugf("SRTPS_POSTFIX secure RTPS footer size: %d bytes", len(sm.data))
	}
}

// AddNetworkPacketInfo 添加网络包信息到统计中
func (p *Parser) AddNetworkPacketInfo(srcIP, dstIP string, srcPort, dstPort uint16, packetLength int) {
	// 判断是否为组播地址
	isMulticast := isMulticastAddress(dstIP)
	
	packetInfo := NetworkPacketInfo{
		SrcIP:        srcIP,
		DstIP:        dstIP,
		SrcPort:      srcPort,
		DstPort:      dstPort,
		PacketLength: packetLength,
		IsMulticast:  isMulticast,
	}
	
	p.networkStats = append(p.networkStats, packetInfo)
}

// GetNetworkStats 获取网络统计信息
func (p *Parser) GetNetworkStats() []NetworkPacketInfo {
	return p.networkStats
}

// isMulticastAddress 判断IP地址是否为组播地址
func isMulticastAddress(ip string) bool {
	// IPv4组播地址范围: 224.0.0.0 到 239.255.255.255
	if len(ip) > 0 {
		// 简单的组播检测：以224-239开头的IPv4地址
		if len(ip) >= 3 {
			if ip[:3] == "224" || ip[:3] == "225" || ip[:3] == "226" || ip[:3] == "227" ||
			   ip[:3] == "228" || ip[:3] == "229" || ip[:3] == "230" || ip[:3] == "231" ||
			   ip[:3] == "232" || ip[:3] == "233" || ip[:3] == "234" || ip[:3] == "235" ||
			   ip[:3] == "236" || ip[:3] == "237" || ip[:3] == "238" || ip[:3] == "239" {
				return true
			}
		}
	}
	return false
}
