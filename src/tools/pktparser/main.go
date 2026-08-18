package main

import (
	"fmt"
	"log"
	"os"
	"path/filepath"
	"strings"

	"autocore.ai/pktparser/internal/analyse"
	"autocore.ai/pktparser/internal/rtps"
	"autocore.ai/pktparser/internal/visualizer"
	"autocore.ai/pktparser/pkg/xlog"
	"github.com/gopacket/gopacket"
	"github.com/gopacket/gopacket/ip4defrag"
	"github.com/gopacket/gopacket/layers"
	"github.com/gopacket/gopacket/pcap"
)

var (
	pcapFile string = "15553.pcap"
	handle   *pcap.Handle
	err      error
)

func main() {

	args := os.Args

	if len(args) < 2 {
		fmt.Println("Usage: pktparser <pcap file> [--output OUTPUT_FILE] [--no-viz]")
		fmt.Println("  --output   Output HTML file path (default: <pcap_filename>_analysis.html)")
		fmt.Println("  --no-viz   Disable visualization HTML generation (only show console output)")
		return
	}

	pcapFile = args[1]
	
	// Parse command line arguments - visualization is enabled by default
	enableViz := true
	var outputFile string
	
	// Generate default output filename based on input pcap file
	baseName := strings.TrimSuffix(filepath.Base(pcapFile), filepath.Ext(pcapFile))
	outputFile = baseName + "_analysis.html"
	
	for i := 2; i < len(args); i++ {
		switch args[i] {
		case "--no-viz":
			enableViz = false
		case "--output":
			if i+1 < len(args) {
				outputFile = args[i+1]
				i++ // Skip next argument
			}
		}
	}

	handle, err = pcap.OpenOffline(pcapFile)
	if err != nil {
		fmt.Println(err.Error())
		log.Fatal(err)
	}
	defer handle.Close()

	doFilter(handle)

	p := doParse(handle)

	analyzer := doAnalyse(p)
	
	// Generate visualization HTML if requested
	if enableViz {
		generateVisualizationHTML(p, analyzer, outputFile)
	}
}

func doFilter(handle *pcap.Handle) {
	// Set filter
	var filter string = "udp"
	err = handle.SetBPFFilter(filter)
	if err != nil {
		log.Fatal(err)
	}
}

func doParse(handle *pcap.Handle) *rtps.Parser {
	// fmt.Printf("Only capturing RTPS packets.")

	// 创建IPv4Defragmenter
	defragger := ip4defrag.NewIPv4Defragmenter()

	// Loop through packets in file
	packetSource := gopacket.NewPacketSource(handle, handle.LinkType())
	cnt := 0
	// idx := 0
	fragmentCnt := 0
	fragmentErrCnt := 0
	DeFragCnt := 0
	parser := rtps.NewParser()
	parser.SetLogLevel(xlog.LOG_LEVEL_OFF)
	for packet := range packetSource.Packets() {
		// fmt.Println("========cnt======", cnt)
		// fmt.Println("========idx======", idx)
		// fmt.Println(packet)
		// idx++

		// 安全地获取payload，避免空指针解引用
		var payload []byte
		
		// 首先尝试从UDP层获取payload
		udpLayer := packet.Layer(layers.LayerTypeUDP)
		if udpLayer != nil {
			udp, _ := udpLayer.(*layers.UDP)
			payload = udp.Payload
		} else {
			// 如果没有UDP层，尝试从应用层获取
			appLayer := packet.ApplicationLayer()
			if appLayer != nil {
				payload = appLayer.Payload()
			} else {
				continue // 跳过没有应用层数据的包
			}
		}

		// // 解析IP层
		ipLayer := packet.Layer(layers.LayerTypeIPv4)
		if ipLayer != nil {
			ip, _ := ipLayer.(*layers.IPv4)

			// 检查是否为IP分片包
			if ip.Flags&layers.IPv4MoreFragments != 0 || ip.FragOffset != 0 {
				fragmentCnt++

				// 使用IPv4Defragmenter重新组装分片包
				newPacket, err := defragger.DefragIPv4(ip)
				if err != nil {
					fragmentErrCnt++
					continue
				}

				// 检查是否已经组装完成
				if newPacket != nil {
					DeFragCnt++
					// fmt.Printf("src ip: %s\n", newPacket.SrcIP)
					// fmt.Printf("dst ip: %s\n", newPacket.DstIP)
					// 处理完整的IP包
					// ...
					payload = newPacket.Payload[8:]
				} else {
					continue
				}
			}
		}

		if len(payload) > 4 {
			if payload[0] == 0x52 && payload[1] == 0x54 && payload[2] == 0x50 && payload[3] == 0x53 {
				cnt++
				
				// 提取网络层信息
				var srcIP, dstIP string
				var srcPort, dstPort uint16
				packetLength := len(packet.Data())
				
				// 获取IP层信息
				if ipLayer != nil {
					ip, _ := ipLayer.(*layers.IPv4)
					srcIP = ip.SrcIP.String()
					dstIP = ip.DstIP.String()
				}
				
				// 获取UDP端口信息
				if udpLayer != nil {
					udp, _ := udpLayer.(*layers.UDP)
					srcPort = uint16(udp.SrcPort)
					dstPort = uint16(udp.DstPort)
				}
				
				// 将网络包信息添加到统计中
				parser.AddNetworkPacketInfo(srcIP, dstIP, srcPort, dstPort, packetLength)
				
				// 解析RTPS内容
				parser.Parse(payload)
			}
		}
		// fmt.Println(packet)

	}
	// fmt.Printf("show %d packets.[fragment:%d DefragPkt:%d DefragErr:%d]\n", cnt+fragmentCnt-DeFragCnt, fragmentCnt, DeFragCnt, fragmentErrCnt)

	// fmt.Println("show all tables:")

	// fmt.Printf("partTable: %s\n", parser.GetPartTable())
	// fmt.Printf("writerTable: %s\n", parser.GetWriterTable())
	// fmt.Printf("readerTable: %s\n", parser.GetReaderTable())
	// fmt.Printf("userDataTable: %s\n", parser.GetUserDataTable())
	// fmt.Printf("processTable: %s\n", parser.GetProcessTable())
	// fmt.Printf("topicTable: %s\n", parser.GetTopicTable())
	// fmt.Printf("locatorTable: %s\n", parser.GetLocatorTable())

	return parser
}

func doAnalyse(p *rtps.Parser) *analyse.Analyzer {
	analyzer := analyse.NewAnalyzer(p)
	// analyzer.SetLogLevel(xlog.LOG_LEVEL_DEBUG)
	analyzer.DoAnalyseProcess()
	analyzer.DoAnalyseParticipant()
	analyzer.DoAnalyseWriter()
	analyzer.DoAnalyseReader()
	analyzer.DoAnalyseLocator()
	analyzer.DoAnalyseTopic()
	analyzer.DoAnalyseUserData()
	analyzerUD := analyse.NewAnalyzerUserData(analyzer)
	analyzerUD.AnalyzePerformance()
	analyzerUD.Result()
	return analyzer
}

// generateVisualizationHTML generates a standalone HTML file with embedded visualization
func generateVisualizationHTML(parser *rtps.Parser, analyzer *analyse.Analyzer, outputFile string) {
	// fmt.Printf("🎨 Generating visualization HTML file...\n")
	
	// Convert data for visualization
	converter := visualizer.NewDataConverter(parser, analyzer)
	visualizationData := converter.ConvertToVisualizationData()
	
	// fmt.Printf("📊 Visualization data ready:\n")
	// fmt.Printf("   - Network Topology: %d nodes, %d edges\n", 
	// 	len(visualizationData.NetworkTopology.Nodes), 
	// 	len(visualizationData.NetworkTopology.Edges))
	// fmt.Printf("   - Processes: %d\n", len(visualizationData.ProcessData.Processes))
	// fmt.Printf("   - Topics: %d\n", len(visualizationData.TopicData.Topics))
	// fmt.Printf("   - Timeline Events: %d\n", len(visualizationData.TimelineData.PacketTimeline))
	
	// Generate standalone HTML file
	err := visualizer.GenerateStandaloneHTML(visualizationData, outputFile)
	if err != nil {
		log.Fatalf("Failed to generate HTML file: %v", err)
	}
	
	// Get absolute path for user convenience
	absPath, _ := filepath.Abs(outputFile)
	
	fmt.Printf("\n✅ Visualization HTML generated successfully!\n")
	fmt.Printf("📁 File: %s\n", absPath)
	fmt.Printf("🌐 Open this file in your browser to view the visualization\n")
}
