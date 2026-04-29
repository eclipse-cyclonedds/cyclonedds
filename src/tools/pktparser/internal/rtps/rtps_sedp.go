package rtps

import "fmt"

type sedpTopicInfoItem struct {
	guid      GUID
	topicName string
	typeName  string
	isR       bool
}

func (ti *sedpTopicInfoItem) String() string {
	str := `sedpTopicInfoItem:{
		  guid: %v,
		  topicName: %v,
		  typeName: %v,
		  isR: %v,
		}`
	return fmt.Sprintf(str, &ti.guid, ti.topicName, ti.typeName, ti.isR)
}
