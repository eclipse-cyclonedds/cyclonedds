package rtps

import (
	"fmt"

	"autocore.ai/pktparser/pkg/errcode"
)

type TopicTblItem struct {
	topicname string
	topictype string
}

func (t TopicTblItem) String() string {
	return fmt.Sprintf("TopicTblItem:{\ttopicname:%v | topictype:%v}\n", t.topicname, t.topictype)
}

// topicname+topictype to processName
type TopicTbl map[string]TopicTblItem

func NewTopicTbl() TopicTbl {
	t := make(map[string]TopicTblItem)
	return TopicTbl(t)
}

func (t TopicTbl) IfExist(key string) bool {
	_, ok := t[key]
	return ok
}

func (t TopicTbl) Add(topicname, topictype string) errcode.ERRCODE {
	key := topicname + topictype
	if t.IfExist(key) {
		return errcode.RTE_ERR_EXISTS
	}
	t[key] = TopicTblItem{
		topicname: topicname,
		topictype: topictype,
	}
	return errcode.RTE_OK
}

func (t TopicTbl) Get(name, topictype string) (TopicTblItem, errcode.ERRCODE) {
	key := name + topictype
	if t.IfExist(key) {
		return t[key], errcode.RTE_OK
	}
	return TopicTblItem{}, errcode.RTE_ERR_NOTFOUND
}

func (t TopicTbl) Del(name, topictype string) errcode.ERRCODE {
	key := name + topictype
	if t.IfExist(key) {
		delete(t, key)
		return errcode.RTE_OK
	}
	return errcode.RTE_ERR_NOTFOUND
}

func (t TopicTbl) String() string {
	str := "TopicTbl:{\n"
	cnt := 0
	for _, p := range t {
		str += fmt.Sprintf("idx:%v,%v\n", cnt, p)
		cnt++
	}
	str += "}"
	return str
}
