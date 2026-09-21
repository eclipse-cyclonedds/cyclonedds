package rtps

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"net"
	"time"
)

type Writer struct {
	readerGUID GUID
	writerEID  EntityID
}

type Pub struct {
	topicName        string
	typeName         string
	writerEID        EntityID
	maxTxSeqNumAvail SeqNum
	minTxSeqNumAvail SeqNum
	dataSubmsgs      []*submsgData // buffered data messages waiting to be sent
	nextSeqNum       SeqNum
	reliable         bool
}

func newPub(topicName, typeName string, writerID EntityID, dataSubmsgs []*submsgData) *Pub {
	if writerID == ENTITYID_UNKNOWN {
		return nil // xxx: better err reporting
	}

	pub := &Pub{
		topicName:  topicName,
		typeName:   typeName,
		writerEID:  writerID,
		nextSeqNum: 1,
	}
	if dataSubmsgs != nil { // it's for a reliable connection
		pub.dataSubmsgs = dataSubmsgs
		pub.reliable = true
	}
	// else unreliable; fire-and-forget from the caller's memory

	defaultSession.pubs = append(defaultSession.pubs, pub)
	return pub
}

var (
	// where does this belong?
	hb_count = uint32(0)
)

func (pub *Pub) publish(submsg *submsgData) {
	// TODO: for best-effort connections, don't buffer it like this
	// check to see if the publisher has a buffer or not, first....

	// (todo: allow people to stuff the message directly in the pub and
	// call this function with sample set to NULL to indicate this)

	// find first place we can buffer this sample
	pub.maxTxSeqNumAvail += 1
	pub.dataSubmsgs = append(pub.dataSubmsgs, submsg)

	var msgbuf bytes.Buffer
	newHeader().WriteTo(&msgbuf)
	newTsSubMsg(time.Now(), binary.LittleEndian).WriteTo(&msgbuf)

	submsg.WriteTo(&msgbuf)

	hb := submsgHeartbeat{
		hdr: submsgHeader{
			id:    SUBMSG_ID_HEARTBEAT,
			flags: FLAGS_SM_ENDIAN | FLAGS_ACKNACK_FINAL,
			sz:    28,
		},
		readerEID:   submsg.readerID,
		writerEID:   submsg.writerID,
		firstSeqNum: 1, // todo, should increase each time (?)
		lastSeqNum:  1, //submsg.writer_sn;
		count:       hb_count,
	}
	hb_count += 1
	hb.WriteTo(&msgbuf)

	addrstr := fmt.Sprintf("%s:%d", DEFAULT_MCAST_GROUP_IP.String(), defaultUDPConfig.mcastBuiltinPort())
	udpaddr, err := net.ResolveUDPAddr("udp", addrstr)
	println("publish to:", addrstr, submsg.hdr.sz)
	if err == nil {
		if err = udpTX(msgbuf.Bytes(), udpaddr); err != nil {
			println("couldn't transmit publish message:", err.Error())
		}
	}
}

var (
	rxAckNackHB = uint32(0)
)

// pub has received an AckNack packet
func (pub *Pub) rxAckNack(an *submsgAckNack, gp GUIDPrefix) {
	// see if we have any of the requested messages
	for reqSeqNum := an.readerSNState.bitmapBase; reqSeqNum < an.readerSNState.Last()+1; reqSeqNum++ {
		// println("     request for seq num:", reqSeqNum, "avail:", len(pub.dataSubmsgs))
		for _, smdata := range pub.dataSubmsgs {
			// fmt.Printf("       %d ?= %d\n", reqSeqNum, smdata.writerSeqNum)
			if smdata.writerSeqNum == reqSeqNum {
				// println("        found it in the history cache! now i need to tx to", gp.String())

				// look up the GUID prefix of this reader, so we can call them back
				part, found := defaultSession.findParticipant(gp)
				if !found {
					println("      woah there partner. you from around these parts?")
					return
				}

				var msgbuf bytes.Buffer
				newHeader().WriteTo(&msgbuf)
				newTsSubMsg(time.Now(), binary.LittleEndian).WriteTo(&msgbuf)
				smdata.WriteTo(&msgbuf)
				hb := submsgHeartbeat{
					hdr: submsgHeader{
						id:    SUBMSG_ID_HEARTBEAT,
						flags: FLAGS_SM_ENDIAN | FLAGS_ACKNACK_FINAL,
						sz:    28,
					},
					readerEID:   smdata.readerID,
					writerEID:   smdata.writerID,
					firstSeqNum: 1, // todo
					lastSeqNum:  1, // xxx
					count:       rxAckNackHB,
				}
				rxAckNackHB++
				hb.WriteTo(&msgbuf)

				addr := part.metaUcastLoc.addrStr()
				if err := udpTXStr(msgbuf.Bytes(), addr); err != nil {
					println("couldn't transmit rxAckNack messages:", err.Error())
				}
			}
		}
	}
}
