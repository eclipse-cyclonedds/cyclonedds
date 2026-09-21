package rtps

import (
	"fmt"
	"sort"
	"time"

	"autocore.ai/pktparser/pkg/errcode"
)

type UserDataTblItem struct {
	guid         GUID
	writerSeqNum SeqNum
	dataLen      int
	ts           time.Time
}

func (t UserDataTblItem) GetGUID() GUID {
	return t.guid
}

func (t UserDataTblItem) GetWriterSeqNum() SeqNum {
	return t.writerSeqNum
}

func (t UserDataTblItem) GetDataLen() int {
	return t.dataLen
}

func (t UserDataTblItem) GetTs() time.Time {
	return t.ts
}

// guid.string()+writerSeq.string() to UserDataTblItem
type UserDataTbl map[string]*UserDataTblItem

func NewUserDataTbl() UserDataTbl {
	t := make(map[string]*UserDataTblItem)
	return UserDataTbl(t)
}

func (t UserDataTbl) IfExist(guid GUID) bool {
	_, ok := t[guid.String()]
	return ok
}

func (t UserDataTbl) Add(guid GUID, writerSeqNum SeqNum, dataLen int, ts time.Time) errcode.ERRCODE {
	if t.IfExist(guid) {
		return errcode.RTE_ERR_EXISTS
	}
	t[guid.String()+"-"+writerSeqNum.String()] = &UserDataTblItem{
		guid:         guid,
		writerSeqNum: writerSeqNum,
		dataLen:      dataLen,
		ts:           ts,
	}
	return errcode.RTE_OK
}

func (t UserDataTbl) Get(guid GUID) (*UserDataTblItem, errcode.ERRCODE) {
	if t.IfExist(guid) {
		return t[guid.String()], errcode.RTE_OK
	}
	return nil, errcode.RTE_ERR_NOTFOUND
}

func (t UserDataTbl) Del(guid GUID, writerSeqNum SeqNum) errcode.ERRCODE {
	if t.IfExist(guid) {
		delete(t, guid.String()+writerSeqNum.String())
		return errcode.RTE_OK
	}
	return errcode.RTE_ERR_NOTFOUND

}

func (t UserDataTbl) String() string {
	var userDataSlice []*UserDataTblItem
	for _, v := range t {
		userDataSlice = append(userDataSlice, v)
	}

	sort.Slice(userDataSlice, func(i, j int) bool {
		return userDataSlice[i].writerSeqNum < userDataSlice[j].writerSeqNum
	})

	cnt := 0
	str := "UserDataTbl:{\n"
	for idx, u := range userDataSlice {
		str += fmt.Sprintf("idx:%v,%v\n", idx, *u)
		cnt++
	}
	str += "}"
	str = str + fmt.Sprintf("Total cnt:%v\n", cnt)
	return str
}

func (t UserDataTblItem) String() string {
	// str := `UserDataTblItem:{
	// 	  guid: %v,
	// 	  writerSeqNum: %v,
	// 	  dataLen: %v,
	// 	  ts: %v,
	// 	}`
	str := "UserDataTblItem:{\tguid:%v | writerSeqNum:%v | dataLen:%v | ts:%v}\n"
	return fmt.Sprintf(str, &t.guid, t.writerSeqNum, t.dataLen, t.ts.Format("2006-01-02 15:04:05.999999999"))
}
