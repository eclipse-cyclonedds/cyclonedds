package rtps

import (
	"encoding/binary"
	"testing"
)

func TestParamString(t *testing.T) {

	cases := []struct{ s string }{
		{"i am a test"},
		{"test"}, // already aligned
		{""},     // empty
	}

	order := binary.LittleEndian

	for i, c := range cases {
		pstr := paramListItem{
			pid:   0x123, // don't care
			value: packParamString(order, c.s),
		}
		if len(pstr.value)&0x3 != 0 {
			t.Errorf("[%d] packed str len not 32-bit aligned", i)
		}
		strout, err := pstr.valToString(order)
		if err != nil {
			t.Errorf("error unpacking str: %v", err)
		}
		if strout != c.s {
			t.Errorf("[%d] str mismatch. got %v, want %v", i, strout, c.s)
		}
	}

}
