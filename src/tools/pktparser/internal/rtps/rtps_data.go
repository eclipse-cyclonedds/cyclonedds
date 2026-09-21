package rtps

import (
	"fmt"
	"time"
)

type userDataItem struct {
	guid         GUID
	writerSeqNum SeqNum
	dataLen      int
	ts           time.Time
}

func (u *userDataItem) String() string {
	str := `userDataItem:{
		  guid: %v,
		  writerSeqNum: %v,
		  dataLen: %v,
		  ts: %v,
		}`
	return fmt.Sprintf(str, u.writerSeqNum, &u.guid, u.dataLen, u.ts)
}
