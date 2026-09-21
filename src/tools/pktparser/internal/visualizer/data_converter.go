package visualizer

import (
	"encoding/base64"
	"fmt"
	"strings"
	"time"
	"autocore.ai/pktparser/internal/rtps"
	"autocore.ai/pktparser/internal/analyse"
)

// DataConverter 负责将解析数据转换为可视化数据
type DataConverter struct {
	parser   *rtps.Parser
	analyzer *analyse.Analyzer
}

// NewDataConverter 创建数据转换器
func NewDataConverter(parser *rtps.Parser, analyzer *analyse.Analyzer) *DataConverter {
	return &DataConverter{
		parser:   parser,
		analyzer: analyzer,
	}
}

// ConvertToVisualizationData 转换为可视化数据
func (dc *DataConverter) ConvertToVisualizationData() *VisualizationData {
	return &VisualizationData{
		NetworkTopology: dc.buildNetworkTopology(),
		TimelineData:    dc.buildTimelineData(),
		StatisticsData:  dc.buildStatisticsData(),
		ProcessData:     dc.buildProcessData(),
		TopicData:       dc.buildTopicData(),
		ParticipantData: dc.buildParticipantData(),
		WriterData:      dc.buildWriterData(),
		ReaderData:      dc.buildReaderData(),
		GeneratedAt:     time.Now(),
	}
}

// buildNetworkTopology 构建网络拓扑图数据
func (dc *DataConverter) buildNetworkTopology() *NetworkTopology {
	nodes := make([]Node, 0)
	edges := make([]Edge, 0)
	
	// 添加参与者节点
	partTable := dc.parser.GetPartTable()
	processTable := dc.parser.GetProcessTable()
	
	// 节点分组：按域ID分组
	domainGroups := make(map[int][]string)
	
	for partID, participant := range partTable {
		// 获取对应的进程信息
		var processName, processID, hostname string
		participantGUIDPrefix := participant.GetGUIDPrefix().String()
		participantGUIDStr := fmt.Sprintf("[%s : 0x000001c1]", participantGUIDPrefix)
		
		if proc, exists := processTable[participantGUIDStr]; exists {
			processName = proc.Name
			processID = proc.Pid
			hostname = participant.GetHostname()
		}
		
		node := Node{
			ID:          partID,
			Label:       processName,
			Type:        "participant",
			ProcessName: processName,
			ProcessID:   processID,
			Hostname:    hostname,
			IPAddress:   participant.GetDefaultUnicastLocatorStr(),
			Port:        7400,
			DomainID:    int(participant.GetDomainID()),
			VendorID:    participant.GetVendorID().String(),
			Group:       fmt.Sprintf("Domain_%d", participant.GetDomainID()),
			Properties: map[string]string{
				"leaseDuration": participant.GetLeaseDuration().String(),
				"vendorID":      participant.GetVendorID().String(),
				"protocol":      fmt.Sprintf("%v", participant.GetProtoVersion()),
			},
		}
		nodes = append(nodes, node)
		
		// 记录域分组
		domainID := int(participant.GetDomainID())
		domainGroups[domainID] = append(domainGroups[domainID], partID)
	}
	
	// 添加域节点（虚拟节点用于分组）
	for domainID, participants := range domainGroups {
		domainNode := Node{
			ID:       fmt.Sprintf("domain_%d", domainID),
			Label:    fmt.Sprintf("Domain %d", domainID),
			Type:     "domain",
			DomainID: domainID,
			Group:    fmt.Sprintf("Domain_%d", domainID),
		}
		nodes = append(nodes, domainNode)
		
		// 为域内参与者创建连接边
		for i, part1 := range participants {
			for j, part2 := range participants {
				if i < j { // 避免重复边
					edge := Edge{
						ID:   fmt.Sprintf("domain_edge_%s_%s", part1, part2),
						From: part1,
						To:   part2,
						Type: "domain_connection",
						Label: fmt.Sprintf("Domain %d", domainID),
						Weight: 1,
					}
					edges = append(edges, edge)
				}
			}
		}
	}
	
	// 添加基于主题的通信边
	dc.addTopicBasedEdges(&edges)
	
	return &NetworkTopology{
		Nodes:     nodes,
		Edges:     edges,
		TreeNodes: dc.buildTreeStructure(),
	}
}

// addTopicBasedEdges 添加基于主题订阅/发布关系的边
func (dc *DataConverter) addTopicBasedEdges(edges *[]Edge) {
	writerTable := dc.parser.GetWriterTable()
	readerTable := dc.parser.GetReaderTable()
	
	// 为每个主题建立发布者-订阅者连接
	topicConnections := make(map[string][]string) // topic -> participants
	
	// 收集每个主题的参与者
	for _, writer := range writerTable {
		topicName := writer.GetTopicName()
		writerGUID := writer.GetGUID()
		participantID := writerGUID.String()
		topicConnections[topicName] = append(topicConnections[topicName], participantID)
	}
	
	for _, reader := range readerTable {
		topicName := reader.TopicName
		participantID := (&reader.Guid).String()
		topicConnections[topicName] = append(topicConnections[topicName], participantID)
	}
	
	// 为每个主题的参与者之间创建边
	for topicName, participants := range topicConnections {
		for i, part1 := range participants {
			for j, part2 := range participants {
				if i != j {
					edge := Edge{
						ID:    fmt.Sprintf("topic_edge_%s_%s_%s", topicName, part1, part2),
						From:  part1,
						To:    part2,
						Type:  "topic_communication",
						Label: topicName,
						Weight: 2,
						Props: map[string]string{
							"topic": topicName,
						},
					}
					*edges = append(*edges, edge)
				}
			}
		}
	}
}

// buildTimelineData 构建时间序列数据
func (dc *DataConverter) buildTimelineData() *TimelineData {
	// 注意：当前RTPS解析器没有时间戳信息
	// 这里创建模拟的时间序列数据结构
	events := make([]TimelineEvent, 0)
	flows := make([]FlowEvent, 0)
	
	baseTime := time.Now().Add(-time.Hour) // 假设从1小时前开始
	
	// 基于参与者创建发现事件
	partTable := dc.parser.GetPartTable()
	i := 0
	for partID, participant := range partTable {
		event := TimelineEvent{
			Timestamp:   baseTime.Add(time.Duration(i) * time.Second * 10),
			EventType:   "discovery",
			SourceID:    partID,
			Description: fmt.Sprintf("Participant %s discovered", participant.GetProcessName()),
			Metadata: map[string]string{
				"domain": fmt.Sprintf("%d", participant.GetDomainID()),
				"vendor": fmt.Sprintf("0x%04x", participant.GetVendorID()),
			},
		}
		events = append(events, event)
		i++
	}
	
	return &TimelineData{
		PacketTimeline:    events,
		CommunicationFlow: flows,
		StartTime:         baseTime,
		EndTime:           time.Now(),
	}
}

// buildStatisticsData 构建统计数据
func (dc *DataConverter) buildStatisticsData() *StatisticsData {
	participantStats := dc.buildParticipantStats()
	topicStats := dc.buildTopicStats()
	domainStats := dc.buildDomainStats()
	messageStats := dc.buildMessageStats()
	networkStats := dc.buildNetworkStats()
	
	return &StatisticsData{
		ParticipantStats: participantStats,
		TopicStats:       topicStats,
		DomainStats:      domainStats,
		MessageStats:     messageStats,
		NetworkStats:     networkStats,
	}
}

// buildParticipantStats 构建参与者统计
func (dc *DataConverter) buildParticipantStats() []ParticipantStat {
	stats := make([]ParticipantStat, 0)
	
	partTable := dc.parser.GetPartTable()
	processTable := dc.parser.GetProcessTable()
	writerTable := dc.parser.GetWriterTable()
	readerTable := dc.parser.GetReaderTable()
	
	for partID, participant := range partTable {
		// 获取进程信息
		var processName string
		participantGUIDPrefix := participant.GetGUIDPrefix().String()
		participantGUIDStr := fmt.Sprintf("[%s : 0x000001c1]", participantGUIDPrefix)
		
		if proc, exists := processTable[participantGUIDStr]; exists {
			processName = proc.Name
		}
		
		// 统计该参与者的发布和订阅主题数
		topicsPublished := 0
		topicsSubscribed := 0
		
		for _, writer := range writerTable {
			writerGUID := writer.GetGUID()
			if writerGUID.String() == partID {
				topicsPublished++
			}
		}
		
		for _, reader := range readerTable {
			if (&reader.Guid).String() == partID {
				topicsSubscribed++
			}
		}
		
		stat := ParticipantStat{
			ParticipantID:    partID,
			ProcessName:      processName,
			MessagesSent:     topicsPublished * 100, // 模拟数据
			MessagesReceived: topicsSubscribed * 150, // 模拟数据
			BytesSent:        int64(topicsPublished * 1024 * 50),
			BytesReceived:    int64(topicsSubscribed * 1024 * 75),
			TopicsPublished:  topicsPublished,
			TopicsSubscribed: topicsSubscribed,
			Uptime:           participant.GetLeaseDuration().String(),
		}
		stats = append(stats, stat)
	}
	
	return stats
}

// buildTopicStats 构建主题统计
func (dc *DataConverter) buildTopicStats() []TopicStat {
	stats := make([]TopicStat, 0)
	topicMap := make(map[string]*TopicStat)
	
	writerTable := dc.parser.GetWriterTable()
	readerTable := dc.parser.GetReaderTable()
	
	// 统计发布者
	for _, writer := range writerTable {
		topicName := writer.GetTopicName()
		if topicMap[topicName] == nil {
			topicMap[topicName] = &TopicStat{
				TopicName:   topicName,
				TopicType:   writer.GetTypeName(),
				Publishers:  make([]string, 0),
				Subscribers: make([]string, 0),
			}
		}
		writerGUID := writer.GetGUID()
		topicMap[topicName].Publishers = append(topicMap[topicName].Publishers, writerGUID.String())
	}
	
	// 统计订阅者
	for _, reader := range readerTable {
		topicName := reader.TopicName
		if topicMap[topicName] == nil {
			topicMap[topicName] = &TopicStat{
				TopicName:   topicName,
				TopicType:   reader.TypeName,
				Publishers:  make([]string, 0),
				Subscribers: make([]string, 0),
			}
		}
		topicMap[topicName].Subscribers = append(topicMap[topicName].Subscribers, (&reader.Guid).String())
	}
	
	// 转换为数组并添加模拟统计数据
	for _, topicStat := range topicMap {
		topicStat.MessageCount = len(topicStat.Publishers) * len(topicStat.Subscribers) * 50 // 模拟
		topicStat.TotalSize = int64(topicStat.MessageCount * 256) // 模拟
		topicStat.AverageSize = 256.0 // 模拟
		topicStat.MessageRate = float64(topicStat.MessageCount) / 60.0 // 模拟
		stats = append(stats, *topicStat)
	}
	
	return stats
}

// buildDomainStats 构建域统计
func (dc *DataConverter) buildDomainStats() []DomainStat {
	stats := make([]DomainStat, 0)
	domainMap := make(map[int]*DomainStat)
	
	partTable := dc.parser.GetPartTable()
	processTable := dc.parser.GetProcessTable()
	
	for _, participant := range partTable {
		domainID := int(participant.GetDomainID())
		
		if domainMap[domainID] == nil {
			domainMap[domainID] = &DomainStat{
				DomainID:         domainID,
				ParticipantCount: 0,
				ProcessNames:     make([]string, 0),
				MessageCount:     0,
			}
		}
		
		domainMap[domainID].ParticipantCount++
		
		// 获取进程名
		participantGUIDPrefix := participant.GetGUIDPrefix().String()
		participantGUIDStr := fmt.Sprintf("[%s : 0x000001c1]", participantGUIDPrefix)
		
		if proc, exists := processTable[participantGUIDStr]; exists {
			domainMap[domainID].ProcessNames = append(domainMap[domainID].ProcessNames, proc.Name)
		}
	}
	
	// 转换为数组
	for _, domainStat := range domainMap {
		domainStat.MessageCount = domainStat.ParticipantCount * 500 // 模拟
		stats = append(stats, *domainStat)
	}
	
	return stats
}

// buildMessageStats 构建消息统计
func (dc *DataConverter) buildMessageStats() MessageStats {
	// 从Parser获取真实的子消息统计数据
	realStats := dc.parser.GetMsgStats()
	
	// 复制统计数据，确保所有可能的消息类型都有值
	messageTypes := make(map[string]int)
	
	// 从真实统计数据复制
	for msgType, count := range realStats {
		messageTypes[msgType] = count
	}
	
	// 如果没有统计数据，使用默认值防止空显示
	if len(messageTypes) == 0 {
		messageTypes = map[string]int{
			"DATA":      0,
			"HEARTBEAT": 0,
			"ACKNACK":   0,
			"PAD":       0,
			"INFO_TS":   0,
			"INFO_DST":  0,
		}
	}
	
	totalPackets := 0
	for _, count := range messageTypes {
		totalPackets += count
	}
	
	// 避免除零错误
	packetsPerSecond := 0.0
	if totalPackets > 0 {
		packetsPerSecond = float64(totalPackets) / 60.0 // 假设1分钟的数据
	}
	
	return MessageStats{
		TotalPackets:      totalPackets,
		TotalBytes:        int64(totalPackets * 256), // 假设平均包大小256字节
		PacketsPerSecond:  packetsPerSecond,
		AveragePacketSize: 256.0,
		MessageTypes:      messageTypes,
	}
}

// buildNetworkStats 构建网络统计 - 基于真实的IP层数据包信息
func (dc *DataConverter) buildNetworkStats() NetworkStats {
	uniqueIPs := make([]string, 0)
	portDist := make(map[string]int)
	bandwidth := make(map[string]float64)
	
	// 从Parser获取真实的网络包统计数据
	networkPackets := dc.parser.GetNetworkStats()
	
	ipSet := make(map[string]bool)
	unicastTraffic := 0
	multicastTraffic := 0
	ipTrafficMap := make(map[string]int) // 记录每个IP的流量
	
	// 基于真实的网络包数据统计
	for _, packet := range networkPackets {
		// 统计唯一IP地址
		if !ipSet[packet.SrcIP] {
			uniqueIPs = append(uniqueIPs, packet.SrcIP)
			ipSet[packet.SrcIP] = true
		}
		if !ipSet[packet.DstIP] {
			uniqueIPs = append(uniqueIPs, packet.DstIP)
			ipSet[packet.DstIP] = true
		}
		
		// 统计端口分布
		srcPortKey := fmt.Sprintf("%d", packet.SrcPort)
		dstPortKey := fmt.Sprintf("%d", packet.DstPort)
		portDist[srcPortKey]++
		portDist[dstPortKey]++
		
		// 基于目标IP是否为组播地址来分类流量 - 统计包个数
		if packet.IsMulticast {
			multicastTraffic++ // 统计包个数，不是字节数
		} else {
			unicastTraffic++ // 统计包个数，不是字节数
		}
		
		// 统计每个IP的流量
		ipTrafficMap[packet.SrcIP] += packet.PacketLength
		ipTrafficMap[packet.DstIP] += packet.PacketLength
	}
	
	// 计算每个IP的带宽（简单估算，假设捕获时长为60秒）
	captureDurationSec := 60.0
	if len(networkPackets) > 0 {
		// 如果有实际包数据，可以更准确地估算时长
		// 这里简化处理，基于包数量估算
		captureDurationSec = float64(len(networkPackets)) / 10.0 // 假设平均10pps
		if captureDurationSec < 1.0 {
			captureDurationSec = 1.0
		}
	}
	
	for ip, totalBytes := range ipTrafficMap {
		// 计算该IP的平均带宽 (bytes/sec)
		bytesPerSec := float64(totalBytes) / captureDurationSec
		bandwidth[ip] = bytesPerSec / 1024.0 // 转换为 KB/s
	}
	
	// 如果没有真实的网络包数据，回退到基于参与者的估算
	if len(networkPackets) == 0 {
		partTable := dc.parser.GetPartTable()
		participantCount := len(partTable)
		
		// 从参与者信息中提取IP地址
		for _, participant := range partTable {
			unicastLocStr := participant.GetDefaultUnicastLocatorStr()
			multicastLocStr := participant.GetDefaultMulticastLocatorStr()
			
			if unicastLocStr != "" && !ipSet[unicastLocStr] {
				uniqueIPs = append(uniqueIPs, unicastLocStr)
				ipSet[unicastLocStr] = true
			}
			
			if multicastLocStr != "" && !ipSet[multicastLocStr] {
				uniqueIPs = append(uniqueIPs, multicastLocStr)
				ipSet[multicastLocStr] = true
			}
			
			// 基本端口统计
			domainID := participant.GetDomainID()
			basePort := 7400 + int(domainID)*250
			portKey := fmt.Sprintf("%d", basePort)
			portDist[portKey]++
		}
		
		// 基于参与者数量的包个数估算
		multicastTraffic = participantCount * 5     // 每个参与者约5个组播包(发现协议)
		unicastTraffic = participantCount * 15      // 每个参与者约15个单播包(数据传输)
		
		// 基本带宽估算
		for ip := range ipSet {
			bandwidth[ip] = 512.0 // 默认512 KB/s
		}
	}
	
	return NetworkStats{
		UniqueIPs:        uniqueIPs,
		PortDistribution: portDist,
		MulticastTraffic: multicastTraffic,
		UnicastTraffic:   unicastTraffic,
		Bandwidth:        bandwidth,
	}
}

// buildProcessData 构建进程数据
func (dc *DataConverter) buildProcessData() *ProcessData {
	processes := make([]ProcessInfo, 0)
	
	processTable := dc.parser.GetProcessTable()
	partTable := dc.parser.GetPartTable()
	
	// 按真正的进程ID分组 - 一个PID可能对应多个DomainParticipant
	processMap := make(map[string]*ProcessInfo) // key: hostname_processName_processID
	
	for _, proc := range processTable {
		// 从participant获取hostname信息
		procGUIDStr := proc.Guid.String()
		var hostname string
		for _, participant := range partTable {
			participantGUIDPrefix := participant.GetGUIDPrefix().String()
			participantGUIDStr := fmt.Sprintf("[%s : 0x000001c1]", participantGUIDPrefix)
			
			if procGUIDStr == participantGUIDStr {
				hostname = cleanString(participant.GetHostname())
				if hostname == "" {
					hostname = "localhost"
				}
				break
			}
		}
		
		// 创建唯一的进程key
		processKey := fmt.Sprintf("%s_%s_%s", hostname, cleanString(proc.Name), proc.Pid)
		
		if processMap[processKey] == nil {
			// 查找该进程的所有参与者
			participants := make([]string, 0)
			domains := make(map[int]bool)
			var vendorID string
			var defaultLoc, metaLoc string
			
			// 查找所有属于这个真实进程的participant
			for _, p := range processTable {
				if p.Name == proc.Name && p.Pid == proc.Pid {
					pGUIDStr := p.Guid.String()
					for partID, participant := range partTable {
						participantGUIDPrefix := participant.GetGUIDPrefix().String()
						participantGUIDStr := fmt.Sprintf("[%s : 0x000001c1]", participantGUIDPrefix)
						
						if pGUIDStr == participantGUIDStr && cleanString(participant.GetHostname()) == hostname {
							participants = append(participants, partID)
							domains[int(participant.GetDomainID())] = true
							if vendorID == "" {
								vendorID = participant.GetVendorID().String()
								defaultLoc = participant.GetDefaultUnicastLocatorStr()
								metaLoc = participant.GetMetatrafficUnicastLocatorStr()
							}
						}
					}
				}
			}
			
			// 选择一个主domain ID (取最小的)
			var primaryDomainID int
			for domainID := range domains {
				if primaryDomainID == 0 || domainID < primaryDomainID {
					primaryDomainID = domainID
				}
			}
			
			processInfo := ProcessInfo{
				ProcessGUID:    (&proc.Guid).String(), // 使用第一个遇到的GUID作为代表
				ProcessName:    cleanString(proc.Name),
				ProcessID:      proc.Pid,
				Hostname:       hostname,
				DefaultLocator: defaultLoc,
				MetaLocator:    metaLoc,
				DomainID:       primaryDomainID,
				VendorID:       vendorID,
				Participants:   participants,
			}
			processMap[processKey] = &processInfo
		}
	}
	
	// 转换为数组
	for _, processInfo := range processMap {
		processes = append(processes, *processInfo)
	}
	
	return &ProcessData{
		Processes: processes,
	}
}

// buildTopicData 构建主题数据
func (dc *DataConverter) buildTopicData() *TopicData {
	topics := make([]TopicInfo, 0)
	topicMap := make(map[string]*TopicInfo)
	
	writerTable := dc.parser.GetWriterTable()
	readerTable := dc.parser.GetReaderTable()
	
	// 收集发布者信息
	for _, writer := range writerTable {
		topicName := writer.GetTopicName()
		if topicMap[topicName] == nil {
			topicMap[topicName] = &TopicInfo{
				TopicName:   topicName,
				TopicType:   writer.GetTypeName(),
				Publishers:  make([]string, 0),
				Subscribers: make([]string, 0),
			}
		}
		writerGUID := writer.GetGUID()
		topicMap[topicName].Publishers = append(topicMap[topicName].Publishers, writerGUID.String())
	}
	
	// 收集订阅者信息
	for _, reader := range readerTable {
		topicName := reader.TopicName
		if topicMap[topicName] == nil {
			topicMap[topicName] = &TopicInfo{
				TopicName:   topicName,
				TopicType:   reader.TypeName,
				Publishers:  make([]string, 0),
				Subscribers: make([]string, 0),
			}
		}
		topicMap[topicName].Subscribers = append(topicMap[topicName].Subscribers, (&reader.Guid).String())
	}
	
	// 转换为数组
	for _, topicInfo := range topicMap {
		topics = append(topics, *topicInfo)
	}
	
	return &TopicData{
		Topics: topics,
	}
}

// buildParticipantData 构建参与者详细数据
func (dc *DataConverter) buildParticipantData() *ParticipantData {
	participants := make([]ParticipantDetail, 0)
	
	partTable := dc.parser.GetPartTable()
	processTable := dc.parser.GetProcessTable()
	writerTable := dc.parser.GetWriterTable()
	readerTable := dc.parser.GetReaderTable()
	
	for partID, participant := range partTable {
		// 获取进程信息
		var processName, processID, hostname string
		participantGUIDPrefix := participant.GetGUIDPrefix().String()
		participantGUIDStr := fmt.Sprintf("[%s : 0x000001c1]", participantGUIDPrefix)
		
		if proc, exists := processTable[participantGUIDStr]; exists {
			processName = proc.Name
			processID = proc.Pid
			hostname = participant.GetHostname()
		}
		
		// 基于GUID前缀找到属于这个participant的writers和readers
		guidPrefix := participant.GetGUIDPrefix().String()
		writers := make([]string, 0)
		readers := make([]string, 0)
		children := make([]ParticipantChild, 0)
		
		// 查找Writers - 比较GUID前缀（前12字节）
		for _, writer := range writerTable {
			writerGUID := writer.GetGUID()
			writerGUIDStr := writerGUID.String()
			// 提取GUID前缀部分（格式: [prefix : entityid]）
			if strings.Contains(writerGUIDStr, guidPrefix) {
				writers = append(writers, writerGUIDStr)
				children = append(children, ParticipantChild{
					GUID:      writerGUIDStr,
					Type:      "writer",
					TopicName: writer.GetTopicName(),
					TypeName:  writer.GetTypeName(),
				})
			}
		}
		
		// 查找Readers - 比较GUID前缀（前12字节）
		for _, reader := range readerTable {
			readerGUIDStr := (&reader.Guid).String()
			// 提取GUID前缀部分（格式: [prefix : entityid]）
			if strings.Contains(readerGUIDStr, guidPrefix) {
				readers = append(readers, readerGUIDStr)
				children = append(children, ParticipantChild{
					GUID:      readerGUIDStr,
					Type:      "reader",
					TopicName: reader.TopicName,
					TypeName:  reader.TypeName,
				})
			}
		}
		
		// Helper function to format byte data as base64 string if present
		formatByteData := func(data []byte) string {
			if len(data) > 0 {
				return base64.StdEncoding.EncodeToString(data)
			}
			return ""
		}

		participantDetail := ParticipantDetail{
			ParticipantGUID:     partID,
			GUIDPrefix:          guidPrefix,
			DomainID:            int(participant.GetDomainID()),
			VendorID:            participant.GetVendorID().String(),
			ProtocolVersion:     fmt.Sprintf("%v", participant.GetProtoVersion()),
			ProcessName:         processName,
			ProcessID:           processID,
			Hostname:            hostname,
			DefaultUnicastLoc:   participant.GetDefaultUnicastLocatorStr(),
			DefaultMulticastLoc: participant.GetDefaultMulticastLocatorStr(),
			MetaUnicastLoc:      participant.GetMetatrafficUnicastLocatorStr(),
			MetaMulticastLoc:    participant.GetMetatrafficMulticastLocatorStr(),
			LeaseDuration:       participant.GetLeaseDuration().String(),
			BuiltinEndpoints:    fmt.Sprintf("0x%08x", participant.GetBuiltinEndpoints()),
			ExpectsInlineQoS:    participant.GetExpectsInlineQoS(),
			AutoCoreCode:        participant.GetAutoCoreCode(),
			// Additional QoS and configuration data from parseDataP
			UserData:            formatByteData(participant.GetUserData()),
			GroupData:           formatByteData(participant.GetGroupData()),
			TopicData:           formatByteData(participant.GetTopicData()),
			ManualLivelinessCount: participant.GetManualLivelinessCount(),
			PropertyList:        participant.GetPropertyList(),
			StaticDiscoveryData: participant.GetStaticDiscoveryData(),
			Writers:             writers,
			Readers:             readers,
			Children:            children,
		}
		
		participants = append(participants, participantDetail)
	}
	
	return &ParticipantData{
		Participants: participants,
	}
}

// buildWriterData 构建Writer详细数据
func (dc *DataConverter) buildWriterData() *WriterData {
	writers := make([]WriterDetail, 0)
	
	writerTable := dc.parser.GetWriterTable()
	partTable := dc.parser.GetPartTable()
	
	for _, writer := range writerTable {
		writerGUID := writer.GetGUID()
		writerGUIDStr := writerGUID.String()
		// 从GUID字符串中提取前缀部分 (格式: [prefix : entityid])
		guidPrefix := strings.Split(strings.Trim(writerGUIDStr, "[]"), " : ")[0]
		
		// 查找对应的participant
		var participantGUID string
		for partID, participant := range partTable {
			if participant.GetGUIDPrefix().String() == guidPrefix {
				participantGUID = partID
				break
			}
		}
		
		// Build QoS profile from the enhanced WriterTblItem structure
		qosProfile := make(map[string]interface{})
		if writer.ParticipantLeaseDuration != "" {
			qosProfile["ParticipantLeaseDuration"] = writer.ParticipantLeaseDuration
		}
		if writer.DomainID != "" {
			qosProfile["DomainID"] = writer.DomainID
		}
		if writer.Reliability != "" {
			qosProfile["Reliability"] = writer.Reliability
		}
		if writer.Liveliness != "" {
			qosProfile["Liveliness"] = writer.Liveliness
		}
		if writer.Durability != "" {
			qosProfile["Durability"] = writer.Durability
		}
		if writer.Presentation != "" {
			qosProfile["Presentation"] = writer.Presentation
		}
		if writer.Partition != "" {
			qosProfile["Partition"] = writer.Partition
		}
		if writer.History != "" {
			qosProfile["History"] = writer.History
		}
		if writer.TransportPriority != "" {
			qosProfile["TransportPriority"] = writer.TransportPriority
		}
		if writer.KeyHash != "" {
			qosProfile["KeyHash"] = writer.KeyHash
		}
		// Extended QoS Parameters
		if writer.OwnershipStrength != "" {
			qosProfile["OwnershipStrength"] = writer.OwnershipStrength
		}
		if writer.DurabilityService != "" {
			qosProfile["DurabilityService"] = writer.DurabilityService
		}
		if writer.Deadline != "" {
			qosProfile["Deadline"] = writer.Deadline
		}
		if writer.DestinationOrder != "" {
			qosProfile["DestinationOrder"] = writer.DestinationOrder
		}
		if writer.LatencyBudget != "" {
			qosProfile["LatencyBudget"] = writer.LatencyBudget
		}
		if writer.Lifespan != "" {
			qosProfile["Lifespan"] = writer.Lifespan
		}
		if writer.UserData != "" {
			qosProfile["UserData"] = writer.UserData
		}
		if writer.GroupData != "" {
			qosProfile["GroupData"] = writer.GroupData
		}
		if writer.TopicData != "" {
			qosProfile["TopicData"] = writer.TopicData
		}
		if writer.ResourceLimits != "" {
			qosProfile["ResourceLimits"] = writer.ResourceLimits
		}
		if writer.Ownership != "" {
			qosProfile["Ownership"] = writer.Ownership
		}
		if writer.TimeBasedFilter != "" {
			qosProfile["TimeBasedFilter"] = writer.TimeBasedFilter
		}

		writerDetail := WriterDetail{
			WriterGUID:      writerGUIDStr,
			ParticipantGUID: participantGUID,
			GUIDPrefix:      guidPrefix,
			TopicName:       writer.GetTopicName(),
			TypeName:        writer.GetTypeName(),
			QoSProfile:      qosProfile,
			Properties: map[string]string{
				"entityKind": "writer",
			},
		}
		
		writers = append(writers, writerDetail)
	}
	
	return &WriterData{
		Writers: writers,
	}
}

// buildReaderData 构建Reader详细数据
func (dc *DataConverter) buildReaderData() *ReaderData {
	readers := make([]ReaderDetail, 0)
	
	readerTable := dc.parser.GetReaderTable()
	partTable := dc.parser.GetPartTable()
	
	for _, reader := range readerTable {
		readerGUIDStr := (&reader.Guid).String()
		// 从GUID字符串中提取前缀部分 (格式: [prefix : entityid])
		guidPrefix := strings.Split(strings.Trim(readerGUIDStr, "[]"), " : ")[0]
		
		// 查找对应的participant
		var participantGUID string
		for partID, participant := range partTable {
			if participant.GetGUIDPrefix().String() == guidPrefix {
				participantGUID = partID
				break
			}
		}
		
		// Build QoS profile from the enhanced ReaderTblItem structure
		qosProfile := make(map[string]interface{})
		if reader.ParticipantLeaseDuration != "" {
			qosProfile["ParticipantLeaseDuration"] = reader.ParticipantLeaseDuration
		}
		if reader.DomainID != "" {
			qosProfile["DomainID"] = reader.DomainID
		}
		if reader.Reliability != "" {
			qosProfile["Reliability"] = reader.Reliability
		}
		if reader.Liveliness != "" {
			qosProfile["Liveliness"] = reader.Liveliness
		}
		if reader.Durability != "" {
			qosProfile["Durability"] = reader.Durability
		}
		if reader.Presentation != "" {
			qosProfile["Presentation"] = reader.Presentation
		}
		if reader.Partition != "" {
			qosProfile["Partition"] = reader.Partition
		}
		if reader.History != "" {
			qosProfile["History"] = reader.History
		}
		if reader.TransportPriority != "" {
			qosProfile["TransportPriority"] = reader.TransportPriority
		}
		if reader.KeyHash != "" {
			qosProfile["KeyHash"] = reader.KeyHash
		}
		// Extended QoS Parameters
		if reader.OwnershipStrength != "" {
			qosProfile["OwnershipStrength"] = reader.OwnershipStrength
		}
		if reader.DurabilityService != "" {
			qosProfile["DurabilityService"] = reader.DurabilityService
		}
		if reader.Deadline != "" {
			qosProfile["Deadline"] = reader.Deadline
		}
		if reader.DestinationOrder != "" {
			qosProfile["DestinationOrder"] = reader.DestinationOrder
		}
		if reader.LatencyBudget != "" {
			qosProfile["LatencyBudget"] = reader.LatencyBudget
		}
		if reader.Lifespan != "" {
			qosProfile["Lifespan"] = reader.Lifespan
		}
		if reader.UserData != "" {
			qosProfile["UserData"] = reader.UserData
		}
		if reader.GroupData != "" {
			qosProfile["GroupData"] = reader.GroupData
		}
		if reader.TopicData != "" {
			qosProfile["TopicData"] = reader.TopicData
		}
		if reader.ResourceLimits != "" {
			qosProfile["ResourceLimits"] = reader.ResourceLimits
		}
		if reader.Ownership != "" {
			qosProfile["Ownership"] = reader.Ownership
		}
		if reader.TimeBasedFilter != "" {
			qosProfile["TimeBasedFilter"] = reader.TimeBasedFilter
		}

		readerDetail := ReaderDetail{
			ReaderGUID:      readerGUIDStr,
			ParticipantGUID: participantGUID,
			GUIDPrefix:      guidPrefix,
			TopicName:       reader.TopicName,
			TypeName:        reader.TypeName,
			QoSProfile:      qosProfile,
			Properties: map[string]string{
				"entityKind": "reader",
			},
		}
		
		readers = append(readers, readerDetail)
	}
	
	return &ReaderData{
		Readers: readers,
	}
}

// cleanString 清理字符串，去除所有控制字符
func cleanString(s string) string {
	// 去除所有控制字符（ASCII 0-31）
	result := ""
	for _, r := range s {
		if r >= 32 && r <= 126 { // 只保留可打印的ASCII字符
			result += string(r)
		}
	}
	return strings.TrimSpace(result)
}

// buildTreeStructure 构建层次化树状结构
// 层次：机器节点 → 进程 → 域 → 参与者 → 主题 → Writer/Reader
func (dc *DataConverter) buildTreeStructure() []TreeNode {
	treeNodes := make([]TreeNode, 0)
	
	partTable := dc.parser.GetPartTable()
	processTable := dc.parser.GetProcessTable()
	writerTable := dc.parser.GetWriterTable()
	readerTable := dc.parser.GetReaderTable()
	
	// 1. 按机器（hostname/IP）分组
	machineMap := make(map[string]*TreeNode)
	
	// 2. 统计信息用于显示
	machineDomainsMap := make(map[string]map[int]bool) // 记录每个machine涉及的domain
	processDomainsMap := make(map[string]map[int]bool) // 记录每个process涉及的domain
	
	// 3. 处理每个参与者，构建层次结构
	for partID, participant := range partTable {
		// 获取对应的进程信息
		var processName, processID, hostname, ipAddress string
		participantGUIDPrefix := participant.GetGUIDPrefix().String()
		domainID := participant.GetDomainID()
		
		// 直接通过participant的GUID查找对应的process
		participantGUIDStr := fmt.Sprintf("[%s : 0x000001c1]", participantGUIDPrefix)
		if proc, exists := processTable[participantGUIDStr]; exists {
			processName = proc.Name
			processID = proc.Pid
			hostname = participant.GetHostname() // 使用GetHostname获取真实节点名称
			ipAddress = participant.GetDefaultUnicastLocatorStr()
		}
		
		// 清理hostname（去除所有控制字符）
		hostnameClean := cleanString(hostname)
		if hostnameClean == "" {
			hostnameClean = "Unknown"
		}
		
		// 从locator字符串中提取IP地址（去除端口信息）
		ipClean := ipAddress
		if strings.Contains(ipAddress, "addr:") {
			// 匹配 "addr:172.27.14.219" 部分
			parts := strings.Split(ipAddress, ",")
			for _, part := range parts {
				if strings.Contains(part, "addr:") {
					ipClean = strings.TrimSpace(strings.Replace(part, "addr:", "", 1))
					break
				}
			}
		}
		
		// 记录domain信息
		machineKey := fmt.Sprintf("%s_%s", hostnameClean, ipClean)
		processKey := fmt.Sprintf("%s_proc_%s", machineKey, processID)
		
		if machineDomainsMap[machineKey] == nil {
			machineDomainsMap[machineKey] = make(map[int]bool)
		}
		machineDomainsMap[machineKey][int(domainID)] = true
		
		if processDomainsMap[processKey] == nil {
			processDomainsMap[processKey] = make(map[int]bool)
		}
		processDomainsMap[processKey][int(domainID)] = true
		
		// 创建或获取机器节点
		if machineMap[machineKey] == nil {
			machineMap[machineKey] = &TreeNode{
				ID:       machineKey,
				Name:     fmt.Sprintf("%s (%s)", hostnameClean, ipClean),
				Type:     "machine",
				Icon:     "🖥️",
				Status:   "active",
				Level:    0,
				Path:     machineKey,
				Children: make([]TreeNode, 0),
				Details: map[string]interface{}{
					"hostname":  hostnameClean,
					"ipAddress": ipClean,
				},
			}
		}
		
		machine := machineMap[machineKey]
		
		// 清理进程名称（去除控制字符）
		processNameClean := cleanString(processName)
		if processNameClean == "" {
			processNameClean = "Unknown Process"
		}
		
		// 查找或创建进程节点
		var processNode *TreeNode
		for i := range machine.Children {
			if machine.Children[i].Type == "process" && machine.Children[i].Details["processID"] == processID {
				processNode = &machine.Children[i]
				break
			}
		}
		
		if processNode == nil {
			processNode = &TreeNode{
				ID:       processKey,
				Name:     fmt.Sprintf("%s (PID: %s)", processNameClean, processID),
				Type:     "process",
				Icon:     "⚙️",
				Status:   "active",
				Level:    1,
				ParentID: machineKey,
				Path:     fmt.Sprintf("%s/%s", machineKey, processID),
				Children: make([]TreeNode, 0),
				Details: map[string]interface{}{
					"processName": processNameClean,
					"processID":   processID,
				},
			}
			machine.Children = append(machine.Children, *processNode)
			processNode = &machine.Children[len(machine.Children)-1]
		}
		
		// 查找或创建domain节点（新增层级）
		domainKey := fmt.Sprintf("%s_domain_%d", processKey, int(domainID))
		var domainNode *TreeNode
		for i := range processNode.Children {
			if processNode.Children[i].Type == "domain" && processNode.Children[i].Details["domainID"] == int(domainID) {
				domainNode = &processNode.Children[i]
				break
			}
		}
		
		if domainNode == nil {
			domainNode = &TreeNode{
				ID:       domainKey,
				Name:     fmt.Sprintf("Domain %d", int(domainID)),
				Type:     "domain",
				Icon:     "🌐",
				Status:   "active",
				Level:    2,
				ParentID: processKey,
				Path:     fmt.Sprintf("%s/domain_%d", processNode.Path, int(domainID)),
				Children: make([]TreeNode, 0),
				Details: map[string]interface{}{
					"domainID": int(domainID),
				},
			}
			processNode.Children = append(processNode.Children, *domainNode)
			domainNode = &processNode.Children[len(processNode.Children)-1]
		}
		
		// 创建参与者节点（现在是第4级）
		participantNode := TreeNode{
			ID:       partID,
			Name:     fmt.Sprintf("Participant %s", participant.GetGUIDPrefix().String()),
			Type:     "participant",
			Icon:     "👥",
			Status:   "active",
			Level:    3,
			ParentID: domainNode.ID,
			Path:     fmt.Sprintf("%s/%s", domainNode.Path, partID),
			Children: make([]TreeNode, 0),
			Details: map[string]interface{}{
				"guid":        partID,
				"guidPrefix":  participant.GetGUIDPrefix().String(),
				"domainID":    int(participant.GetDomainID()),
				"vendorID":    participant.GetVendorID(),
				"processName": processNameClean,
			},
		}
		
		// 收集该参与者的所有主题
		topicMap := make(map[string]*TreeNode)
		guidPrefix := participant.GetGUIDPrefix().String()
		
		// 处理Writers
		for _, writer := range writerTable {
			writerGUID := writer.GetGUID()
			if strings.Contains(writerGUID.String(), guidPrefix) {
				topicName := writer.GetTopicName()
				
				// 创建或获取主题节点（现在是第5级）
				if topicMap[topicName] == nil {
					topicMap[topicName] = &TreeNode{
						ID:       fmt.Sprintf("%s_topic_%s", partID, topicName),
						Name:     topicName,
						Type:     "topic",
						Icon:     "📋",
						Status:   "active",
						Level:    4,
						ParentID: partID,
						Path:     fmt.Sprintf("%s/%s", participantNode.Path, topicName),
						Children: make([]TreeNode, 0),
						Details: map[string]interface{}{
							"topicName": topicName,
							"typeName":  writer.GetTypeName(),
						},
					}
				}
				
				// 添加Writer节点（现在是第6级）
				writerGUIDStr := writerGUID.String()
				writerNode := TreeNode{
					ID:       writerGUIDStr,
					Name:     fmt.Sprintf("Writer (%s) [%s]", writer.GetTypeName(), writerGUIDStr),
					Type:     "writer",
					Icon:     "✍️",
					Status:   "active",
					Level:    5,
					ParentID: topicMap[topicName].ID,
					Path:     fmt.Sprintf("%s/%s", topicMap[topicName].Path, "writer"),
					Details: map[string]interface{}{
						"guid":      writerGUIDStr,
						"topicName": topicName,
						"typeName":  writer.GetTypeName(),
					},
				}
				topicMap[topicName].Children = append(topicMap[topicName].Children, writerNode)
			}
		}
		
		// 处理Readers
		for _, reader := range readerTable {
			readerGUIDStr := (&reader.Guid).String()
			if strings.Contains(readerGUIDStr, guidPrefix) {
				topicName := reader.TopicName
				
				// 创建或获取主题节点（现在是第5级）
				if topicMap[topicName] == nil {
					topicMap[topicName] = &TreeNode{
						ID:       fmt.Sprintf("%s_topic_%s", partID, topicName),
						Name:     topicName,
						Type:     "topic",
						Icon:     "📋",
						Status:   "active",
						Level:    4,
						ParentID: partID,
						Path:     fmt.Sprintf("%s/%s", participantNode.Path, topicName),
						Children: make([]TreeNode, 0),
						Details: map[string]interface{}{
							"topicName": topicName,
							"typeName":  reader.TypeName,
						},
					}
				}
				
				// 添加Reader节点（现在是第6级）
				readerNode := TreeNode{
					ID:       readerGUIDStr,
					Name:     fmt.Sprintf("Reader (%s) [%s]", reader.TypeName, readerGUIDStr),
					Type:     "reader",
					Icon:     "👀",
					Status:   "active",
					Level:    5,
					ParentID: topicMap[topicName].ID,
					Path:     fmt.Sprintf("%s/%s", topicMap[topicName].Path, "reader"),
					Details: map[string]interface{}{
						"guid":      readerGUIDStr,
						"topicName": topicName,
						"typeName":  reader.TypeName,
					},
				}
				topicMap[topicName].Children = append(topicMap[topicName].Children, readerNode)
			}
		}
		
		// 将主题节点添加到参与者
		for _, topicNode := range topicMap {
			participantNode.Children = append(participantNode.Children, *topicNode)
		}
		
		// 将参与者节点添加到domain节点
		domainNode.Children = append(domainNode.Children, participantNode)
	}
	
	// 更新host和process节点的domain信息显示
	for machineKey, machine := range machineMap {
		// 收集该machine涉及的domains
		domains := make([]int, 0)
		for domainID := range machineDomainsMap[machineKey] {
			domains = append(domains, domainID)
		}
		
		// 在machine名称后显示domain信息
		if len(domains) > 0 {
			domainsStr := fmt.Sprintf("Domains: %v", domains)
			machine.Name = fmt.Sprintf("%s - %s", machine.Name, domainsStr)
		}
		
		// 更新process节点的domain信息
		for i := range machine.Children {
			process := &machine.Children[i]
			processKey := process.ID
			
			// 收集该process涉及的domains
			processDomains := make([]int, 0)
			for domainID := range processDomainsMap[processKey] {
				processDomains = append(processDomains, domainID)
			}
			
			// 在process名称后显示domain信息
			if len(processDomains) > 0 {
				domainsStr := fmt.Sprintf("Domains: %v", processDomains)
				process.Name = fmt.Sprintf("%s - %s", process.Name, domainsStr)
			}
		}
	}
	
	// 更新节点计数（新的层次结构：machine → process → domain → participant → topic → writer/reader）
	for _, machine := range machineMap {
		machine.Count = len(machine.Children)
		for i := range machine.Children {
			process := &machine.Children[i]
			process.Count = len(process.Children)
			for j := range process.Children {
				domain := &process.Children[j]
				domain.Count = len(domain.Children)
				for k := range domain.Children {
					participant := &domain.Children[k]
					participant.Count = len(participant.Children)
					for l := range participant.Children {
						topic := &participant.Children[l]
						topic.Count = len(topic.Children)
					}
				}
			}
		}
		treeNodes = append(treeNodes, *machine)
	}
	
	return treeNodes
}

