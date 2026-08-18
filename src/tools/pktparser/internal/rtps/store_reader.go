package rtps

import (
	"fmt"

	"autocore.ai/pktparser/pkg/errcode"
)

type ReaderTblItem struct {
	Guid      GUID
	TopicName string
	TypeName  string
	// QoS Parameters
	ParticipantLeaseDuration string
	DomainID                 string
	Reliability              string
	Liveliness               string
	Durability               string
	Presentation             string
	Partition                string
	History                  string
	TransportPriority        string
	KeyHash                  string
	// Extended QoS Parameters
	OwnershipStrength        string
	DurabilityService        string
	Deadline                 string
	DestinationOrder         string
	LatencyBudget            string
	Lifespan                 string
	UserData                 string
	GroupData                string
	TopicData                string
	ResourceLimits           string
	Ownership                string
	TimeBasedFilter          string
}

// guid.string() to ReaderTblItem
type ReaderTbl map[string]*ReaderTblItem

func NewReaderTbl() ReaderTbl {
	t := make(map[string]*ReaderTblItem)
	return ReaderTbl(t)
}

func (t ReaderTbl) IfExist(guid GUID) bool {
	_, ok := t[guid.String()]
	return ok
}

func (t ReaderTbl) Add(guid GUID, topicName, typeName string) errcode.ERRCODE {
	if t.IfExist(guid) {
		return errcode.RTE_ERR_EXISTS
	}
	t[guid.String()] = &ReaderTblItem{
		Guid:      guid,
		TopicName: topicName,
		TypeName:  typeName,
	}
	return errcode.RTE_OK
}

// AddWithQoS adds a reader with QoS parameters
func (t ReaderTbl) AddWithQoS(guid GUID, topicName, typeName string, qosParams map[string]string) errcode.ERRCODE {
	if t.IfExist(guid) {
		return errcode.RTE_ERR_EXISTS
	}
	item := &ReaderTblItem{
		Guid:      guid,
		TopicName: topicName,
		TypeName:  typeName,
		ParticipantLeaseDuration: qosParams["ParticipantLeaseDuration"],
		DomainID:                 qosParams["DomainID"],
		Reliability:              qosParams["Reliability"],
		Liveliness:               qosParams["Liveliness"],
		Durability:               qosParams["Durability"],
		Presentation:             qosParams["Presentation"],
		Partition:                qosParams["Partition"],
		History:                  qosParams["History"],
		TransportPriority:        qosParams["TransportPriority"],
		KeyHash:                  qosParams["KeyHash"],
		// Extended QoS Parameters
		OwnershipStrength:        qosParams["OwnershipStrength"],
		DurabilityService:        qosParams["DurabilityService"],
		Deadline:                 qosParams["Deadline"],
		DestinationOrder:         qosParams["DestinationOrder"],
		LatencyBudget:            qosParams["LatencyBudget"],
		Lifespan:                 qosParams["Lifespan"],
		UserData:                 qosParams["UserData"],
		GroupData:                qosParams["GroupData"],
		TopicData:                qosParams["TopicData"],
		ResourceLimits:           qosParams["ResourceLimits"],
		Ownership:                qosParams["Ownership"],
		TimeBasedFilter:          qosParams["TimeBasedFilter"],
	}
	t[guid.String()] = item
	return errcode.RTE_OK
}

func (t ReaderTbl) Get(guid GUID) (*ReaderTblItem, errcode.ERRCODE) {
	if t.IfExist(guid) {
		return t[guid.String()], errcode.RTE_OK
	}
	return nil, errcode.RTE_ERR_NOTFOUND
}

func (t ReaderTbl) Del(guid GUID) errcode.ERRCODE {
	if t.IfExist(guid) {
		delete(t, guid.String())
		return errcode.RTE_OK
	}
	return errcode.RTE_ERR_NOTFOUND
}

func (t ReaderTbl) String() string {
	str := "ReaderTbl:{\n"
	for _, r := range t {
		str += fmt.Sprintf("%v\n", r)
	}
	str += "}"
	return str
}

func (t ReaderTblItem) String() string {
	str := "ReaderTblItem:{\tguid:%v | topicName:%v | typeName:%v"
	if t.ParticipantLeaseDuration != "" {
		str += " | leaseDuration:%v"
	}
	if t.DomainID != "" {
		str += " | domainID:%v"
	}
	if t.Reliability != "" {
		str += " | reliability:%v"
	}
	if t.Liveliness != "" {
		str += " | liveliness:%v"
	}
	if t.Durability != "" {
		str += " | durability:%v"
	}
	if t.Presentation != "" {
		str += " | presentation:%v"
	}
	if t.Partition != "" {
		str += " | partition:%v"
	}
	if t.History != "" {
		str += " | history:%v"
	}
	if t.TransportPriority != "" {
		str += " | transportPriority:%v"
	}
	if t.KeyHash != "" {
		str += " | keyHash:%v"
	}
	str += "}\n"
	
	args := []interface{}{&t.Guid, t.TopicName, t.TypeName}
	if t.ParticipantLeaseDuration != "" {
		args = append(args, t.ParticipantLeaseDuration)
	}
	if t.DomainID != "" {
		args = append(args, t.DomainID)
	}
	if t.Reliability != "" {
		args = append(args, t.Reliability)
	}
	if t.Liveliness != "" {
		args = append(args, t.Liveliness)
	}
	if t.Durability != "" {
		args = append(args, t.Durability)
	}
	if t.Presentation != "" {
		args = append(args, t.Presentation)
	}
	if t.Partition != "" {
		args = append(args, t.Partition)
	}
	if t.History != "" {
		args = append(args, t.History)
	}
	if t.TransportPriority != "" {
		args = append(args, t.TransportPriority)
	}
	if t.KeyHash != "" {
		args = append(args, t.KeyHash)
	}
	
	return fmt.Sprintf(str, args...)
}

func (t ParticipantTbl) GetReaderTblItem(guid GUID) (*ReaderTblItem, errcode.ERRCODE) {
	return nil, errcode.RTE_ERR_NOTFOUND
}
