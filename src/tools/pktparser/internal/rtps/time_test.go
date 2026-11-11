package rtps

import (
	"encoding/binary"
	"testing"
	"time"
)

func TestTimeRoundtrip(t *testing.T) {

	cases := []struct{ t time.Time }{
		{time.Unix(1451457191, 226962928)}, // arbitrary point in time
	}

	for _, c := range cases {
		b := timeToBytes(c.t, binary.LittleEndian)

		tout, err := timeFromBytes(binary.LittleEndian, b)
		if err != nil {
			t.Errorf("timeFromBytes: %v", err.Error())
		}
		if !tout.Equal(c.t) {
			t.Errorf("time roundtrip mismatch. got %v, want %v", tout, c.t)
		}
	}
}

func TestDurationRoundtrip(t *testing.T) {
	cases := []struct{ d time.Duration }{
		{time.Duration(1451457191)}, // arbitrary duration
	}

	for _, c := range cases {
		b := durationToBytes(c.d, binary.LittleEndian)

		dout, err := durationFromBytes(binary.LittleEndian, b)
		if err != nil {
			t.Errorf("durationFromBytes: %v", err.Error())
		}
		if dout != c.d {
			t.Errorf("duration roundtrip mismatch. got %v, want %v", dout, c.d)
		}
	}
}
