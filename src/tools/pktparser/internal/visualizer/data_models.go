package visualizer

import (
	"time"
)

// VisualizationData 包含所有可视化需要的数据
type VisualizationData struct {
	NetworkTopology  *NetworkTopology  `json:"networkTopology"`
	TimelineData     *TimelineData     `json:"timelineData"`
	StatisticsData   *StatisticsData   `json:"statisticsData"`
	ProcessData      *ProcessData      `json:"processData"`
	TopicData        *TopicData        `json:"topicData"`
	ParticipantData  *ParticipantData  `json:"participantData"`
	WriterData       *WriterData       `json:"writerData"`
	ReaderData       *ReaderData       `json:"readerData"`
	GeneratedAt      time.Time         `json:"generatedAt"`
}

// NetworkTopology 网络拓扑图数据
type NetworkTopology struct {
	Nodes     []Node      `json:"nodes"`
	Edges     []Edge      `json:"edges"`
	TreeNodes []TreeNode  `json:"treeNodes"` // 新增：层次化树状结构
}

// Node 网络节点（参与者/进程）
type Node struct {
	ID              string            `json:"id"`
	Label           string            `json:"label"`
	Type            string            `json:"type"` // "participant", "process", "domain"
	ProcessName     string            `json:"processName,omitempty"`
	ProcessID       string            `json:"processID,omitempty"`
	Hostname        string            `json:"hostname,omitempty"`
	IPAddress       string            `json:"ipAddress,omitempty"`
	Port            int               `json:"port,omitempty"`
	DomainID        int               `json:"domainID,omitempty"`
	VendorID        string            `json:"vendorID,omitempty"`
	Group           string            `json:"group,omitempty"` // 用于节点分组
	Properties      map[string]string `json:"properties,omitempty"`
}

// Edge 网络连接边
type Edge struct {
	ID     string            `json:"id"`
	From   string            `json:"from"`
	To     string            `json:"to"`
	Type   string            `json:"type"` // "communication", "subscription", "publication"
	Label  string            `json:"label,omitempty"`
	Weight int               `json:"weight,omitempty"` // 连接强度
	Props  map[string]string `json:"properties,omitempty"`
}

// TreeNode 树状节点结构
type TreeNode struct {
	ID       string     `json:"id"`
	Name     string     `json:"name"`
	Type     string     `json:"type"` // "machine", "process", "participant", "topic", "writer", "reader"
	Icon     string     `json:"icon,omitempty"`
	Status   string     `json:"status,omitempty"` // "active", "inactive", "error"
	Count    int        `json:"count,omitempty"`  // 子节点数量
	Children []TreeNode `json:"children,omitempty"`
	
	// 详细信息
	Details map[string]interface{} `json:"details,omitempty"`
	
	// 层次关系
	ParentID string `json:"parentId,omitempty"`
	Level    int    `json:"level"`
	Path     string `json:"path,omitempty"` // 节点路径，如 "machine1/process1/participant1"
}

// TimelineData 时间序列数据
type TimelineData struct {
	PacketTimeline   []TimelineEvent `json:"packetTimeline"`
	CommunicationFlow []FlowEvent    `json:"communicationFlow"`
	StartTime        time.Time       `json:"startTime"`
	EndTime          time.Time       `json:"endTime"`
}

// TimelineEvent 时间轴事件
type TimelineEvent struct {
	Timestamp     time.Time         `json:"timestamp"`
	EventType     string            `json:"eventType"` // "packet", "discovery", "heartbeat", "data"
	SourceID      string            `json:"sourceID"`
	TargetID      string            `json:"targetID,omitempty"`
	Description   string            `json:"description"`
	PacketSize    int               `json:"packetSize,omitempty"`
	TopicName     string            `json:"topicName,omitempty"`
	Metadata      map[string]string `json:"metadata,omitempty"`
}

// FlowEvent 通信流事件
type FlowEvent struct {
	Timestamp  time.Time `json:"timestamp"`
	FromNode   string    `json:"fromNode"`
	ToNode     string    `json:"toNode"`
	MessageType string   `json:"messageType"`
	Size       int       `json:"size"`
	Topic      string    `json:"topic,omitempty"`
}

// StatisticsData 统计数据
type StatisticsData struct {
	ParticipantStats  []ParticipantStat `json:"participantStats"`
	TopicStats        []TopicStat       `json:"topicStats"`
	DomainStats       []DomainStat      `json:"domainStats"`
	MessageStats      MessageStats      `json:"messageStats"`
	NetworkStats      NetworkStats      `json:"networkStats"`
}

// ParticipantStat 参与者统计
type ParticipantStat struct {
	ParticipantID    string `json:"participantID"`
	ProcessName      string `json:"processName"`
	MessagesSent     int    `json:"messagesSent"`
	MessagesReceived int    `json:"messagesReceived"`
	BytesSent        int64  `json:"bytesSent"`
	BytesReceived    int64  `json:"bytesReceived"`
	TopicsPublished  int    `json:"topicsPublished"`
	TopicsSubscribed int    `json:"topicsSubscribed"`
	Uptime           string `json:"uptime"`
}

// TopicStat 主题统计
type TopicStat struct {
	TopicName       string   `json:"topicName"`
	TopicType       string   `json:"topicType"`
	Publishers      []string `json:"publishers"`
	Subscribers     []string `json:"subscribers"`
	MessageCount    int      `json:"messageCount"`
	TotalSize       int64    `json:"totalSize"`
	AverageSize     float64  `json:"averageSize"`
	MessageRate     float64  `json:"messageRate"` // messages per second
}

// DomainStat 域统计
type DomainStat struct {
	DomainID        int      `json:"domainID"`
	ParticipantCount int     `json:"participantCount"`
	ProcessNames    []string `json:"processNames"`
	MessageCount    int      `json:"messageCount"`
}

// MessageStats 消息统计
type MessageStats struct {
	TotalPackets     int     `json:"totalPackets"`
	TotalBytes       int64   `json:"totalBytes"`
	PacketsPerSecond float64 `json:"packetsPerSecond"`
	AveragePacketSize float64 `json:"averagePacketSize"`
	MessageTypes     map[string]int `json:"messageTypes"`
}

// NetworkStats 网络统计
type NetworkStats struct {
	UniqueIPs        []string           `json:"uniqueIPs"`
	PortDistribution map[string]int     `json:"portDistribution"`
	MulticastTraffic int                `json:"multicastTraffic"`
	UnicastTraffic   int                `json:"unicastTraffic"`
	Bandwidth        map[string]float64 `json:"bandwidth"` // IP -> bandwidth
}

// ProcessData 进程数据
type ProcessData struct {
	Processes []ProcessInfo `json:"processes"`
}

// ProcessInfo 进程信息
type ProcessInfo struct {
	ProcessGUID      string   `json:"processGUID"`
	ProcessName      string   `json:"processName"`
	ProcessID        string   `json:"processID"`
	Hostname         string   `json:"hostname"`
	DefaultLocator   string   `json:"defaultLocator"`
	MetaLocator      string   `json:"metaLocator"`
	DomainID         int      `json:"domainID"`
	VendorID         string   `json:"vendorID"`
	Participants     []string `json:"participants"`
}

// TopicData 主题数据
type TopicData struct {
	Topics []TopicInfo `json:"topics"`
}

// TopicInfo 主题信息
type TopicInfo struct {
	TopicName       string   `json:"topicName"`
	TopicType       string   `json:"topicType"`
	Publishers      []string `json:"publishers"`
	Subscribers     []string `json:"subscribers"`
	QoSProfile      string   `json:"qosProfile,omitempty"`
}

// ParticipantData 参与者数据
type ParticipantData struct {
	Participants []ParticipantDetail `json:"participants"`
}

// ParticipantDetail 参与者详细信息
type ParticipantDetail struct {
	ParticipantGUID     string               `json:"participantGUID"`
	GUIDPrefix          string               `json:"guidPrefix"`
	DomainID            int                  `json:"domainID"`
	VendorID            string               `json:"vendorID"`
	ProtocolVersion     string               `json:"protocolVersion"`
	ProcessName         string               `json:"processName"`
	ProcessID           string               `json:"processID"`
	Hostname            string               `json:"hostname"`
	DefaultUnicastLoc   string               `json:"defaultUnicastLoc"`
	DefaultMulticastLoc string               `json:"defaultMulticastLoc"`
	MetaUnicastLoc      string               `json:"metaUnicastLoc"`
	MetaMulticastLoc    string               `json:"metaMulticastLoc"`
	LeaseDuration       string               `json:"leaseDuration"`
	BuiltinEndpoints    string               `json:"builtinEndpoints"`
	ExpectsInlineQoS    bool                 `json:"expectsInlineQoS"`
	AutoCoreCode        []byte               `json:"autoCoreCode"`
	// Additional QoS and configuration data from parseDataP
	UserData            string               `json:"userData,omitempty"`
	GroupData           string               `json:"groupData,omitempty"`
	TopicData           string               `json:"topicData,omitempty"`
	ManualLivelinessCount uint32             `json:"manualLivelinessCount,omitempty"`
	PropertyList        []string             `json:"propertyList,omitempty"`
	StaticDiscoveryData []byte               `json:"staticDiscoveryData"` // PID_STATIC_DISCOVERY data
	Writers             []string             `json:"writers"`    // Writer GUIDs belonging to this participant
	Readers             []string             `json:"readers"`    // Reader GUIDs belonging to this participant
	Children            []ParticipantChild   `json:"children"`   // Hierarchical relationship
}

// ParticipantChild 参与者子节点（Writer/Reader）
type ParticipantChild struct {
	GUID      string `json:"guid"`
	Type      string `json:"type"`      // "writer" or "reader"
	TopicName string `json:"topicName"`
	TypeName  string `json:"typeName"`
}

// WriterData Writer数据
type WriterData struct {
	Writers []WriterDetail `json:"writers"`
}

// WriterDetail Writer详细信息
type WriterDetail struct {
	WriterGUID       string                 `json:"writerGUID"`
	ParticipantGUID  string                 `json:"participantGUID"`
	GUIDPrefix       string                 `json:"guidPrefix"`
	TopicName        string                 `json:"topicName"`
	TypeName         string                 `json:"typeName"`
	QoSProfile       map[string]interface{} `json:"qosProfile,omitempty"`
	Locators         []string               `json:"locators,omitempty"`
	Properties       map[string]string      `json:"properties,omitempty"`
}

// ReaderData Reader数据
type ReaderData struct {
	Readers []ReaderDetail `json:"readers"`
}

// ReaderDetail Reader详细信息
type ReaderDetail struct {
	ReaderGUID       string                 `json:"readerGUID"`
	ParticipantGUID  string                 `json:"participantGUID"`
	GUIDPrefix       string                 `json:"guidPrefix"`
	TopicName        string                 `json:"topicName"`
	TypeName         string                 `json:"typeName"`
	QoSProfile       map[string]interface{} `json:"qosProfile,omitempty"`
	Locators         []string               `json:"locators,omitempty"`
	Properties       map[string]string      `json:"properties,omitempty"`
}

