package analyse

import (
	"sort"
	"strconv"
	"strings"
	"unicode"

	"autocore.ai/pktparser/internal/rtps"
	"autocore.ai/pktparser/pkg/errcode"
	"autocore.ai/pktparser/pkg/xlog"
)

type Analyzer struct {
	logger *xlog.Logger
	parser *rtps.Parser
}

func NewAnalyzer(p *rtps.Parser) *Analyzer {
	a := &Analyzer{}
	a.parser = p
	a.logger = xlog.NewLogger()
	return a
}

func removeUnprintableChars(str string) string {
	// 使用strings.Map函数和unicode.IsPrint函数过滤不可打印字符
	return strings.Map(func(r rune) rune {
		if unicode.IsPrint(r) {
			return r
		}
		return -1
	}, str)
}

func (a *Analyzer) SetLogLevel(level int16) {
	a.logger.SetLevel(level)
}

func (a *Analyzer) DoAnalyseParticipant() {
	a.logger.Infof("===DoAnalyseParticipant ...\n")
	partTbl := a.parser.GetPartTable()
	cnt := 0
	domainIDs := make(map[rtps.DomainID]bool)
	for _, p := range partTbl {
		a.logger.Debugf("idx:%v : Participant: %v\n", cnt, p)
		cnt++
		domainIDs[p.GetDomainID()] = true
	}
	a.logger.Infof("total Participant:%v\n", cnt)
	a.logger.Infof("total DomainID:%v\n", len(domainIDs))
}

func (a *Analyzer) DoAnalyseProcess() {
	a.logger.Infof("===DoAnalyseProcess ...\n")
	partTbl := a.parser.GetPartTable()
	cnt := 0
	ppList := make([]*rtps.Participant, 0)
	for _, p := range partTbl {
		ppList = append(ppList, p)
		// guid := p.GetGUID()
		// a.logger.Debugf("idx:%v : ProcessGuid(pp): %v | pName:%v | pID:%v\n", cnt, &guid, p.GetProcessName(), p.GetProcessID())
		// cnt++
	}

	sort.Slice(ppList, func(i, j int) bool {
		pidI := ppList[i].GetProcessID()
		pidJ := ppList[j].GetProcessID()
		numI, err := strconv.Atoi(pidI)
		numJ, err1 := strconv.Atoi(pidJ)
		if err != nil {
			numI = 0
		}
		if err1 != nil {
			numJ = 0
		}
		return numI < numJ
	})

	for _, p := range ppList {
		guid := p.GetGUID()
		hostname := p.GetHostname()
		a.logger.Debugf("idx:%v : ProcessGuid(pp): %v | pName:%v                        | pID:%v | hostname:%v | ucdefLoc:%v\n", cnt, &guid, removeUnprintableChars(p.GetProcessName()), p.GetProcessID(), removeUnprintableChars(hostname), p.GetDefaultUnicastLocatorStr())
		cnt++
	}
}

func (a *Analyzer) DoAnalyseUserData() {
	a.logger.Infof("===DoAnalyseUserData ...\n")
	writerTbl := a.parser.GetWriterTable()
	userDataTbl := a.parser.GetUserDataTable()
	cnt := 0
	ntfCnt := 0
	reqCnt := 0
	repCnt := 0
	totleLen := 0
	for _, ud := range userDataTbl {
		w, err := writerTbl.Get(ud.GetGUID())
		if err != errcode.RTE_OK {
			a.logger.Errorf("Get writer failed: %v", err)
			continue
		}
		a.logger.Debugf("idx:%v : Writer Topic Type : %v\n", cnt, w.GetTypeName())
		//a.logger.Infof("User Data: %v", ud)
		cnt++
		if strings.Contains(w.GetTypeName(), "_Ntf") {
			ntfCnt++
		} else if strings.Contains(w.GetTypeName(), "_Request") {
			reqCnt++
		} else if strings.Contains(w.GetTypeName(), "_Reply") {
			repCnt++
		}
		totleLen += ud.GetDataLen()
	}
	a.logger.Infof("totalLen:%v Bytes\n", totleLen)
	a.logger.Infof("totalcnt:%v\n", cnt)
	ntfPercent := float64(ntfCnt) / float64(cnt) * 100
	reqPercent := float64(reqCnt) / float64(cnt) * 100
	repPercent := float64(repCnt) / float64(cnt) * 100
	a.logger.Infof("ntfCnt:%v(%v%%), reqCnt:%v(%v%%), repCnt:%v(%v%%)\n", ntfCnt, ntfPercent, reqCnt, reqPercent, repCnt, repPercent)
	if cnt != ntfCnt+reqCnt+repCnt {
		a.logger.Errorf("cnt != ntfCnt+reqCnt+repCnt")
	}
}

func (a *Analyzer) DoAnalyseWriter() {
	a.logger.Infof("===DoAnalyseWriter ...\n")
	writerTbl := a.parser.GetWriterTable()
	cnt := 0
	for _, w := range writerTbl {
		// a.logger.Debugf("idx:%v : Writer Topic Type : %v\n", cnt, w.GetTypeName())
		a.logger.Debugf("idx:%v : Writer: %v\n", cnt, w)
		cnt++
	}
	a.logger.Infof("total Writer:%v\n", cnt)
}

// func (a *Analyzer) DoAnalyseProcess() {
// 	a.logger.Infof("===DoAnalyseProcess ...\n")
// 	processTbl := a.parser.GetProcessTable()
// 	cnt := 0
// 	for _, p := range processTbl {
// 		a.logger.Debugf("idx:%v : Process: %v\n", cnt, p)
// 		cnt++
// 	}
// 	a.logger.Infof("total Process:%v\n", cnt)
// }

func (a *Analyzer) DoAnalyseTopic() {
	a.logger.Infof("===DoAnalyseTopic ...\n")
	topicTbl := a.parser.GetTopicTable()
	cnt := 0
	for _, t := range topicTbl {
		a.logger.Debugf("idx:%v : Topic: %v\n", cnt, t)
		cnt++
	}
	a.logger.Infof("total Topic:%v\n", cnt)
}

func (a *Analyzer) DoAnalyseLocator() {
	a.logger.Infof("===DoAnalyseLocator ...\n")
	locatorTbl := a.parser.GetLocatorTable()
	cnt := 0
	for _, l := range locatorTbl {
		a.logger.Debugf("idx:%v : Locator: %v\n", cnt, l)
		cnt++
	}
	a.logger.Infof("total Locator:%v\n", cnt)
}

func (a *Analyzer) DoAnalyseReader() {
	a.logger.Infof("===DoAnalyseReader ...\n")
	readerTbl := a.parser.GetReaderTable()
	cnt := 0
	for _, r := range readerTbl {
		a.logger.Debugf("idx:%v : Reader: %v\n", cnt, r)
		cnt++
	}
	a.logger.Infof("total Reader:%v\n", cnt)
}
