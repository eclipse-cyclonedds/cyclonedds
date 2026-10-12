package rtps

import (
	"bytes"
	"fmt"

	"autocore.ai/pktparser/pkg/errcode"
)

// guid.string() to Participant
type ParticipantTbl map[string]*Participant

func NewParticipantTbl() ParticipantTbl {
	t := make(map[string]*Participant)
	return ParticipantTbl(t)
}

func (t ParticipantTbl) IfExist(guid GUID) bool {
	_, ok := t[guid.String()]
	return ok
}

func (t ParticipantTbl) Add(p *Participant) errcode.ERRCODE {
	if t.IfExist(p.guid) {
		return errcode.RTE_ERR_EXISTS
	}
	t[p.guid.String()] = p
	return errcode.RTE_OK
}

func (t ParticipantTbl) Get(guid GUID) (*Participant, errcode.ERRCODE) {
	if t.IfExist(guid) {
		return t[guid.String()], errcode.RTE_OK
	}
	return nil, errcode.RTE_ERR_NOTFOUND
}

func (t ParticipantTbl) GetByGUIDPrefix(guidPrefix GUIDPrefix) (*Participant, errcode.ERRCODE) {
	for _, p := range t {
		if bytes.Equal(p.guid.prefix, guidPrefix) {
			return p, errcode.RTE_OK
		}
	}
	return nil, errcode.RTE_ERR_NOTFOUND
}

func (t ParticipantTbl) Del(guid GUID) errcode.ERRCODE {
	if t.IfExist(guid) {
		delete(t, guid.String())
		return errcode.RTE_OK
	}
	return errcode.RTE_ERR_NOTFOUND
}

func (t ParticipantTbl) String() string {
	str := "ParticipantTbl:{\n"
	for _, p := range t {
		str += fmt.Sprintf("%v\n", p)
	}
	str += "}"
	return str
}
