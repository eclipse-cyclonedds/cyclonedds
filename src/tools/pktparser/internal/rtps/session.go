package rtps

import (
	"bytes"
	"fmt"
)

// session collects that that would otherwise be global
type session struct {
	readers           []*Reader
	writers           []*Writer
	discoParticipants []*Participant // xxx: store as map keyed by guid prefix?
	pubs              []*Pub
	subs              []*Sub
	spdp              SPDP
	sedp              SEDP
	// XXX: some kind of config for which transport(s) are operational? just UDP for now
}

var (
	defaultSession session
)

func (s *session) init() {
	s.spdp.init()
	s.sedp.init()

	if err := udpInit(); err != nil {
		fmt.Println("udp init err:", err.Error())
	}
}

func (s *session) start() {
	s.spdp.start()
	s.sedp.start()
}

func (s *session) addReader(r *Reader) {
	// XXX: locking
	for _, rdr := range s.readers {
		if rdr.writerGUID.Equal(&r.writerGUID) {
			return
		}
	}
	s.readers = append(s.readers, r)
}

func (s *session) readerWithWriterGUID(g *GUID) (*Reader, bool) {
	for _, rdr := range defaultSession.readers {
		if rdr.writerGUID.Equal(g) {
			return rdr, true
		}
	}
	return nil, false
}

func (s *session) readerWithWriterGUIDAndRdrID(g *GUID, rdrID EntityID) (*Reader, bool) {
	for _, rdr := range defaultSession.readers {
		if rdr.writerGUID.Equal(g) && (rdrID == rdr.readerEID || rdrID == ENTITYID_UNKNOWN) {
			return rdr, true
		}
	}
	return nil, false
}

func (s *session) addWriter(w *Writer) {
	// XXX: locking
	// for _, wrtr := range s.writers {
	//     if wrtr.writerGUID.Equal(&w.writerGUID) {
	//         return
	//     }
	// }
	s.writers = append(s.writers, w)
}

func (s *session) writerWithReaderGUID(g *GUID) (*Writer, bool) {
	for _, wrtr := range defaultSession.writers {
		if wrtr.readerGUID.Equal(g) {
			return wrtr, true
		}
	}
	return nil, false
}

func (s *session) addSub(sub *Sub) {
	// XXX: locking
	fmt.Printf("sub %d: 0x%x\n", len(s.subs), sub.readerEID) // eid printed with wrong endianness
	s.subs = append(s.subs, sub)
}

func (s *session) subWithRdrID(rdrID EntityID) (*Sub, bool) {
	for _, sub := range defaultSession.subs {
		if sub.readerEID == rdrID {
			return sub, true
		}
	}
	return nil, false
}

func (s *session) pubWithWriterID(id EntityID) *Pub {
	for _, pub := range s.pubs {
		if id == pub.writerEID {
			return pub
		}
	}
	return nil
}

func (s *session) findParticipant(gp GUIDPrefix) (*Participant, bool) {
	for _, p := range s.discoParticipants {
		if bytes.Equal(gp, p.guidPrefix) {
			return p, true
		}
	}
	return nil, false
}

func (s *session) addParticipant(p *Participant) {
	// XXX: ensure this part is not already in our list?
	s.discoParticipants = append(s.discoParticipants, p)
}
