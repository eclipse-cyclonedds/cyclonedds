package analyse

import (
	"fmt"
	"math"
	"sort"
	"time"

	"autocore.ai/pktparser/internal/rtps"
	"autocore.ai/pktparser/pkg/errcode"
)

type AnalyzerUserData struct {
	base              *Analyzer
	writerStaticTable WriterStaticTbl
}

func NewAnalyzerUserData(a *Analyzer) *AnalyzerUserData {
	return &AnalyzerUserData{
		base:              a,
		writerStaticTable: NewWriterStaticTbl(),
	}
}

type WriterStaticee struct {
	guid             rtps.GUID
	cnt              int
	maxLen           int
	minLen           int
	lastSendAt       time.Time
	totalLen         int
	totalDur         time.Duration
	defaultUCLocator string
	processName      string
	topicName        string
	topicType        string
}

func (w *WriterStaticee) String() string {
	str := `WriterStaticee:{guid:%v | cnt:%v | maxLen:%v | minLen:%v | totalLen:%v | totalDur:%v | frequency:%v |
		                    defUCLoc:%v | Process:%v | tpN:%v | tpT:%v}
		 `
	return fmt.Sprintf(str, &w.guid, w.cnt, w.maxLen, w.minLen, w.totalLen, w.totalDur, w.totalDur/time.Duration(w.cnt), w.defaultUCLocator, removeUnprintableChars(w.processName), w.topicName, w.topicType)
}

func NewWriterStaticee(guid rtps.GUID) *WriterStaticee {
	return &WriterStaticee{
		guid:   guid,
		maxLen: math.MinInt,
		minLen: math.MaxInt,
	}
}

func (ws *WriterStaticee) Update(dataLen int, ts time.Time) {
	ws.cnt++
	ws.totalLen += dataLen
	if dataLen > ws.maxLen {
		ws.maxLen = dataLen
	}
	if dataLen < ws.minLen {
		ws.minLen = dataLen
	}
	if ws.lastSendAt.IsZero() {
		ws.lastSendAt = ts
	} else {
		ws.totalDur += ts.Sub(ws.lastSendAt)
	}
}

type WriterStaticTbl map[string]*WriterStaticee

func NewWriterStaticTbl() WriterStaticTbl {
	t := make(map[string]*WriterStaticee)
	return WriterStaticTbl(t)
}

func (t WriterStaticTbl) IfExist(guid rtps.GUID) bool {
	_, ok := t[guid.String()]
	return ok
}

func (t WriterStaticTbl) Add(guid rtps.GUID, defaultUCLocator, processName, topicName, typeType string) {
	if t.IfExist(guid) {
		return
	}
	w := NewWriterStaticee(guid)
	w.defaultUCLocator = defaultUCLocator
	w.processName = processName
	w.topicName = topicName
	w.topicType = typeType
	t[guid.String()] = w
}

func (t WriterStaticTbl) Get(guid rtps.GUID) (*WriterStaticee, bool) {
	if t.IfExist(guid) {
		return t[guid.String()], true
	}
	return nil, false
}

func (t WriterStaticTbl) String() string {
	cnt := 0
	str := "WriterStaticTbl:{\n"
	for _, w := range t {
		str += fmt.Sprintf("idx:%v, %v\n", cnt, w)
		cnt++
	}
	str += "}"
	return str
}

func (a *AnalyzerUserData) AnalyzePerformance() {
	userDataTbl := a.base.parser.GetUserDataTable()
	partTab := a.base.parser.GetPartTable()
	writerTbl := a.base.parser.GetWriterTable()

	seqList := make([]*rtps.UserDataTblItem, 0)

	for _, ud := range userDataTbl {
		seqList = append(seqList, ud)
	}

	sort.Slice(seqList, func(i, j int) bool {
		gi := seqList[i].GetGUID()
		gj := seqList[j].GetGUID()
		if gi.Equal(&gj) {
			return seqList[i].GetWriterSeqNum() < seqList[j].GetWriterSeqNum()
		}
		return gi.Compare(&gj)
	})

	writerStaticTable := a.writerStaticTable
	for _, ud := range seqList {
		wrierGuid := ud.GetGUID()

		wr, err := writerTbl.Get(wrierGuid)
		if errcode.RTE_OK != err {
			a.base.logger.Errorf("writerTbl.Get(%v) failed: %v\n", wrierGuid, err)
			continue
		}

		pp, err := partTab.GetByGUIDPrefix(wr.GetGUIDPrefix())
		if errcode.RTE_OK != err {
			a.base.logger.Errorf("partTab.Get(%v) failed: %v\n", wrierGuid, err)
			continue
		}

		defUCLoc := pp.GetDefaultUnicastLocatorStr()
		processName := pp.GetProcessName()
		topicName := wr.GetTopicName()
		topicType := wr.GetTypeName()

		a.base.logger.Infof("UserData: %v", ud)
		wso, ok := writerStaticTable.Get(ud.GetGUID())
		if ok {
			wso.Update(ud.GetDataLen(), ud.GetTs())
		} else {
			writerStaticTable.Add(ud.GetGUID(), defUCLoc, processName, topicName, topicType)
			wsn, _ := writerStaticTable.Get(ud.GetGUID())
			wsn.Update(ud.GetDataLen(), ud.GetTs())
		}
	}
}

func (a *AnalyzerUserData) Result() {
	a.base.logger.Infof("WriterStaticTable: %v", a.writerStaticTable)
}
