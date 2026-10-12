package rtps

import (
	"fmt"
	"sort"

	"autocore.ai/pktparser/pkg/errcode"
)

type LocatorTblItem struct {
	defaultUcastLoc locator
	partGuid        GUID
	processList     []string
}

// defaultUcastLoc.string() to LocatorTblItem
type LocatorTbl map[string]*LocatorTblItem

func NewLocatorTbl() LocatorTbl {
	t := make(map[string]*LocatorTblItem)
	return LocatorTbl(t)
}

func (t LocatorTbl) IfExist(defaultUcastLoc locator) bool {
	_, ok := t[defaultUcastLoc.String()]
	return ok
}

func (t LocatorTbl) Add(defaultUcastLoc locator, partGuid GUID, processName string) errcode.ERRCODE {
	if t.IfExist(defaultUcastLoc) {
		for _, pro := range t[defaultUcastLoc.String()].processList {
			if pro == processName {
				return errcode.RTE_ERR_EXISTS
			}
		}
		t[defaultUcastLoc.String()].processList = append(t[defaultUcastLoc.String()].processList, processName)
	} else {
		t[defaultUcastLoc.String()] = &LocatorTblItem{
			defaultUcastLoc: defaultUcastLoc,
			partGuid:        partGuid,
			processList:     []string{processName},
		}
	}
	return errcode.RTE_OK
}

func (t LocatorTbl) Get(defaultUcastLoc locator) (*LocatorTblItem, errcode.ERRCODE) {
	if t.IfExist(defaultUcastLoc) {
		return t[defaultUcastLoc.String()], errcode.RTE_OK
	}
	return nil, errcode.RTE_ERR_NOTFOUND
}

func (t LocatorTbl) Del(defaultUcastLoc locator, processName string) errcode.ERRCODE {
	if t.IfExist(defaultUcastLoc) {
		for i, pro := range t[defaultUcastLoc.String()].processList {
			if pro == processName {
				t[defaultUcastLoc.String()].processList = append(t[defaultUcastLoc.String()].processList[:i], t[defaultUcastLoc.String()].processList[i+1:]...)
				return errcode.RTE_OK
			}
		}
		return errcode.RTE_ERR_NOTFOUND
	}
	return errcode.RTE_ERR_NOTFOUND
}

func (t LocatorTbl) String() string {
	//sotre by defaultUcastLoc
	defaultUcastLocSlice := make([]locator, 0)
	for _, v := range t {
		defaultUcastLocSlice = append(defaultUcastLocSlice, v.defaultUcastLoc)
	}
	sort.Slice(defaultUcastLocSlice, func(i, j int) bool {
		return defaultUcastLocSlice[i].Compare(defaultUcastLocSlice[j])
	})

	str := "LocatorTbl:{\n"
	for idx, l := range defaultUcastLocSlice {
		str += fmt.Sprintf("%d:", idx)
		str += t[l.String()].String()
	}
	str += "}"
	return str
}

func (t LocatorTblItem) String() string {
	str := "LocatorTblItem:{defaultUcastLoc: %v,partGuid: %v,processList: %v}\n"
	return fmt.Sprintf(str, t.defaultUcastLoc, &t.partGuid, t.processList)
}
