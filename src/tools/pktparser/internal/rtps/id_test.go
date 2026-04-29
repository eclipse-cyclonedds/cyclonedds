package rtps

import (
	"testing"
)

func TestUserID(t *testing.T) {
	cases := []struct {
		kind     uint8
		isReader bool
		isWriter bool
	}{
		{ENTITYID_KIND_READER_NO_KEY, true, false},
		{ENTITYID_KIND_WRITER_NO_KEY, false, true},
	}

	for i, c := range cases {
		id := createUserID(c.kind)
		if id.isReader() != c.isReader {
			t.Errorf("[%d] reader mismatch, got %v want %v", i, id.isReader(), c.isReader)
		}
		if id.isWriter() != c.isWriter {
			t.Errorf("[%d] writer mismatch, got %v want %v", i, id.isWriter(), c.isReader)
		}
		if id.isBuiltin() {
			t.Errorf("[%d] builtin mismatch, user id should never be builtin", i)
		}
	}
}
