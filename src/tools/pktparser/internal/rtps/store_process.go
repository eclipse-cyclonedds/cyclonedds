package rtps

import (
	"fmt"

	"autocore.ai/pktparser/pkg/errcode"
)

type ProcessTblItem struct {
	Guid GUID
	Name string
	Pid  string
}

func (t ProcessTblItem) String() string {
	return fmt.Sprintf("ProcessTblItem:{\tguid:%v | name:%v | pid:%v}\n", &t.Guid, t.Name, t.Pid)
}

// GUID to ProcessTblItem (changed from processName to GUID for uniqueness)
type ProcessTbl map[string]ProcessTblItem

func NewProcessTbl() ProcessTbl {
	t := make(map[string]ProcessTblItem)
	return ProcessTbl(t)
}

func (t ProcessTbl) IfExist(guidStr string) bool {
	_, ok := t[guidStr]
	return ok
}

func (t ProcessTbl) Add(guid GUID, name, pid string) errcode.ERRCODE {
	guidStr := guid.String()
	if t.IfExist(guidStr) {
		return errcode.RTE_ERR_EXISTS
	}
	t[guidStr] = ProcessTblItem{
		Name: name,
		Pid:  pid,
		Guid: guid,
	}
	return errcode.RTE_OK
}

func (t ProcessTbl) Get(guidStr string) (ProcessTblItem, errcode.ERRCODE) {
	if t.IfExist(guidStr) {
		return t[guidStr], errcode.RTE_OK
	}
	return ProcessTblItem{}, errcode.RTE_ERR_NOTFOUND
}

// GetByName 根据进程名查找（兼容性方法，返回第一个匹配的）
func (t ProcessTbl) GetByName(name string) (ProcessTblItem, errcode.ERRCODE) {
	for _, proc := range t {
		if proc.Name == name {
			return proc, errcode.RTE_OK
		}
	}
	return ProcessTblItem{}, errcode.RTE_ERR_NOTFOUND
}

func (t ProcessTbl) Del(name string) errcode.ERRCODE {
	if t.IfExist(name) {
		delete(t, name)
		return errcode.RTE_OK
	}
	return errcode.RTE_ERR_NOTFOUND
}

func (t ProcessTbl) String() string {
	str := "ProcessTbl:{\n"
	cnt := 0
	for _, p := range t {
		str += fmt.Sprintf("idx:%v:%v\n", cnt, p)
		cnt++
	}
	str += "}"
	return str
}
