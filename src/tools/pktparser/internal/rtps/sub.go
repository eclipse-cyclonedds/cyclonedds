package rtps

import (
	"bytes"
	"fmt"
)

type dataCallback func(r *receiver, submsg *subMsg, scheme uint16, b []byte)
type msgCallback func(b []byte)

// Reader is a reader
type Reader struct {
	reliable    bool
	writerGUID  GUID
	readerEID   EntityID
	maxRxSeqNum SeqNum
	dataCB      dataCallback
	msgCB       msgCallback
}

type Sub struct {
	topicName string
	typeName  string
	readerEID EntityID
	dataCB    dataCallback
	msgCB     msgCallback
	reliable  bool
}

func AddUserSub(topicName, typeName string, cb msgCallback) {
	subEID := createUserID(ENTITYID_KIND_READER_NO_KEY)
	fmt.Printf("frudp_add_user_sub(%s, %s) on EID 0x%08x\r\n", topicName, typeName, subEID)
	defaultSession.addSub(&Sub{
		topicName: topicName,
		typeName:  typeName,
		readerEID: subEID,
		msgCB:     cb,
		reliable:  false,
	})
	//sedp_publish_sub(&sub); // can't do this yet; spdp hasn't started bcast
}

var (
	// better place for this to live
	s_acknack_count = uint32(1)
)

func (r *Reader) Matches(s *submsgData) bool {
	// have to special-case the SPDP entity ID's, since they come in
	// with any GUID prefix and with either an unknown reader entity ID
	// or the unknown-reader entity ID
	return s.writerID == ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER && (r.readerEID == ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER || r.readerEID == ENTITYID_UNKNOWN)
}

func (r *Reader) generateAckNackForHB(hb *submsgHeartbeat) SeqNumSet {
	// generate a SeqNumSet that represents the seqnums that we're still waiting for,
	// based on the given heartbeat
	if r.maxRxSeqNum >= hb.lastSeqNum {
		return SeqNumSet{
			bitmapBase: hb.firstSeqNum + 1,
			numBits:    0,
		}
	}
	sns := SeqNumSet{
		bitmapBase: r.maxRxSeqNum + 1,
		numBits:    uint32(hb.lastSeqNum - r.maxRxSeqNum),
		bitmap:     []uint32{0xffffffff}, // all bits acked for now
	}
	// xxx: make a real bitmap
	if sns.numBits > 31 {
		sns.numBits = 31
	}
	return sns
}

func (r *Reader) txAckNack(srcPrefix GUIDPrefix, set SeqNumSet) {
	// find the participant we are trying to talk to
	part, found := defaultSession.findParticipant(srcPrefix)
	if !found {
		println("tried to acknack an unknown participant:", srcPrefix.String())
		return // better error handling
	}

	var msgbuf bytes.Buffer
	newHeader().WriteTo(&msgbuf)

	dstSubmsg := subMsg{
		hdr: submsgHeader{
			id:    SUBMSG_ID_INFO_DST,
			flags: FLAGS_SM_ENDIAN | FLAGS_ACKNACK_FINAL,
			sz:    UDPGuidPrefixLen,
		},
		data: srcPrefix,
	}
	dstSubmsg.WriteTo(&msgbuf)

	an := submsgAckNack{
		hdr: submsgHeader{
			id:    SUBMSG_ID_ACKNACK,
			flags: FLAGS_SM_ENDIAN,
			sz:    uint16(24 + set.BitMapWords()*4),
		},
		readerEID:     r.readerEID,
		writerEID:     r.writerGUID.eid,
		readerSNState: set,
		count:         s_acknack_count,
	}
	an.WriteTo(&msgbuf)
	s_acknack_count++

	if err := udpTXStr(msgbuf.Bytes(), part.metaUcastLoc.addrStr()); err != nil {
		println("udpTxAckNack failed to tx:", err.Error())
	}
}
