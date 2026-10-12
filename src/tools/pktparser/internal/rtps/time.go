package rtps

import (
	"encoding/binary"
	"io"
	"time"
)

// From the spec:
// The representation of the time is the one defined by the IETF Network Time Protocol (NTP) Standard (IETF RFC 1305).
// In this representation, time is expressed in seconds and fraction of seconds using the formula:
//    time = seconds + (fraction / 2^(32))
// The time origin is represented by the reserved value TIME_ZERO and corresponds to the Unix prime epoch 0h, 1 January 1970.

const (
	nanosPerSec = 1e9
)

var (
	timeZero     = time.Unix(0, 0)
	timeInvalid  = time.Unix(-1, 0xffffffff)
	timeInfinite = time.Unix(0x7fffffff, 0xffffffff)
)

func timeFromBytes(order binary.ByteOrder, b []byte) (time.Time, error) {
	if len(b) < 8 {
		return timeInvalid, io.EOF
	}

	sec := int64(order.Uint32(b[0:]))
	frac := int64(order.Uint32(b[4:]))
	return time.Unix(sec, (frac*nanosPerSec)>>32).UTC(), nil
}

func timeToBytes(t time.Time, order binary.ByteOrder) []byte {
	sec := uint32(t.Unix())
	frac := uint32((nanosPerSec - 1 + (int64(t.Nanosecond()) << 32)) / nanosPerSec)

	b := make([]byte, 8)
	order.PutUint32(b[0:], sec)
	order.PutUint32(b[4:], frac)
	return b
}

func durationToBytes(d time.Duration, order binary.ByteOrder) []byte {
	buf := make([]byte, 8)
	nsec := d.Nanoseconds()
	order.PutUint32(buf, uint32(nsec/nanosPerSec))
	order.PutUint32(buf[4:], uint32(nsec%nanosPerSec))
	return buf
}

func durationFromBytes(order binary.ByteOrder, b []byte) (time.Duration, error) {
	if len(b) < 8 {
		return time.Duration(0), io.EOF
	}

	sec := order.Uint32(b[0:])
	frac := order.Uint32(b[4:])

	// Convert fraction to nanoseconds: frac * 1e9 / 2^32
	nsec := (int64(frac) * nanosPerSec) >> 32

	return time.Duration(int64(sec)*nanosPerSec + nsec), nil
}
