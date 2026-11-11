package rtps

import (
	"encoding/binary"
	"io"
	"time"
)

const (
	QOS_RELIABILITY_KIND_BEST_EFFORT = 1
	QOS_RELIABILITY_KIND_RELIABLE    = 2
	QOS_HISTORY_KIND_KEEP_LAST       = 0
	QOS_HISTORY_KIND_KEEP_ALL        = 1
	QOS_PRESENTATION_SCOPE_TOPIC     = 1
)

type qosReliability struct {
	kind            uint32
	maxBlockingTime time.Duration
}

func newQosReliabilityFromBytes(bin binary.ByteOrder, b []byte) (qosReliability, error) {
	if len(b) < 4+4+4 {
		return qosReliability{}, io.EOF
	}
	dur, err := durationFromBytes(bin, b[4:])
	if err != nil {
		return qosReliability{}, err
	}
	return qosReliability{
		kind:            bin.Uint32(b[0:]),
		maxBlockingTime: dur,
	}, nil
}

func (r *qosReliability) WriteTo(w io.Writer) {
	b := make([]byte, 4)
	binary.LittleEndian.PutUint32(b, r.kind)
	w.Write(b)
	w.Write(durationToBytes(r.maxBlockingTime, binary.LittleEndian))
}

type qosHistory struct {
	kind  uint32
	depth uint32
}

func newQosHistoryFromBytes(bin binary.ByteOrder, b []byte) (qosHistory, error) {
	if len(b) < 4+4 {
		return qosHistory{}, io.EOF
	}
	return qosHistory{
		kind:  bin.Uint32(b[0:]),
		depth: bin.Uint32(b[4:]),
	}, nil
}

type qosPresentation struct {
	scope          uint32
	coherentAccess uint16
	orderedAccess  uint16
}
