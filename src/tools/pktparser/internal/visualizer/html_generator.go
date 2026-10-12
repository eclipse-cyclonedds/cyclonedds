package visualizer

import (
	"encoding/json"
	"fmt"
	"os"
	"strings"
)

// GenerateStandaloneHTML creates a self-contained HTML file with embedded data and scripts
func GenerateStandaloneHTML(data *VisualizationData, outputFile string) error {
	// Convert data to JSON
	jsonData, err := json.MarshalIndent(data, "", "  ")
	if err != nil {
		return fmt.Errorf("failed to marshal visualization data: %v", err)
	}

	// Generate the complete HTML content
	htmlContent := generateHTMLTemplate(string(jsonData))

	// Write to file
	file, err := os.Create(outputFile)
	if err != nil {
		return fmt.Errorf("failed to create output file: %v", err)
	}
	defer file.Close()

	_, err = file.WriteString(htmlContent)
	if err != nil {
		return fmt.Errorf("failed to write HTML content: %v", err)
	}

	return nil
}

// generateHTMLTemplate creates the complete HTML template with embedded CSS and JavaScript
func generateHTMLTemplate(jsonData string) string {
	return fmt.Sprintf(`<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="Cache-Control" content="no-cache, no-store, must-revalidate">
    <meta http-equiv="Pragma" content="no-cache">
    <meta http-equiv="Expires" content="0">
    <title>RTPS Packet Analysis Visualization</title>
    <style>
        %s
    </style>
</head>
<body>
    <div class="container">
        <header class="header">
            <h1>🔍 RTPS Packet Analysis Dashboard</h1>
            <div class="header-stats" id="header-stats">
                <div class="stat-item">
                    <span class="stat-value" id="total-processes">-</span>
                    <span class="stat-label">Processes</span>
                </div>
                <div class="stat-item">
                    <span class="stat-value" id="total-participants">-</span>
                    <span class="stat-label">Participants</span>
                </div>
                <div class="stat-item">
                    <span class="stat-value" id="total-topics">-</span>
                    <span class="stat-label">Topics</span>
                </div>
                <div class="stat-item">
                    <span class="stat-value" id="total-writers">-</span>
                    <span class="stat-label">Writers</span>
                </div>
                <div class="stat-item">
                    <span class="stat-value" id="total-readers">-</span>
                    <span class="stat-label">Readers</span>
                </div>
                <div class="stat-item">
                    <span class="stat-value" id="total-packets">-</span>
                    <span class="stat-label">Statistics</span>
                </div>
            </div>
        </header>

        <nav class="tab-nav">
            <button class="tab-btn active" onclick="showTab('overview')">Overview</button>
            <button class="tab-btn" onclick="showTab('topology')">Network Topology</button>
            <button class="tab-btn" onclick="showTab('nodes')">Nodes Details</button>
            <button class="tab-btn" onclick="showTab('processes')">Processes</button>
            <button class="tab-btn" onclick="showTab('participants')">Participants</button>
            <button class="tab-btn" onclick="showTab('topics')">Topics</button>
            <button class="tab-btn" onclick="showTab('writers')">Writers</button>
            <button class="tab-btn" onclick="showTab('readers')">Readers</button>
        </nav>

        <main class="main-content">
            <!-- Overview Tab -->
            <div id="overview-tab" class="tab-content active">
                <div class="overview-grid">
                    <div class="overview-card">
                        <h3>📊 Data Summary</h3>
                        <div id="summary-content">
                            <div class="summary-item">
                                <strong>Analysis Time:</strong> <span id="analysis-time">-</span>
                            </div>
                            <div class="summary-item">
                                <strong>Network Nodes:</strong> <span id="network-nodes">-</span>
                            </div>
                            <div class="summary-item">
                                <strong>Network Edges:</strong> <span id="network-edges">-</span>
                            </div>
                            <div class="summary-item">
                                <strong>Message Types:</strong> <span id="message-types">-</span>
                            </div>
                        </div>
                    </div>
                    
                    <div class="overview-card">
                        <h3>🌐 Network Overview</h3>
                        <div id="network-summary">
                            <div class="network-item">
                                <strong>Unique IPs:</strong> <span id="unique-ips">-</span>
                            </div>
                            <div class="network-item">
                                <strong>Port Distribution:</strong> <span id="port-count">-</span>
                            </div>
                            <div class="network-item">
                                <strong>Domains:</strong> <span id="domain-count">-</span>
                            </div>
                        </div>
                    </div>
                    
                    <div class="overview-card">
                        <h3>📋 Quick Actions</h3>
                        <div class="quick-actions">
                            <button class="action-button" onclick="showTab('topology')">🌐 View Network</button>
                            <button class="action-button" onclick="showTab('processes')">⚙️ View Processes</button>
                            <button class="action-button" onclick="showTab('topics')">📋 View Topics</button>
                            <button class="action-button" onclick="refreshData()">🔄 Refresh Data</button>
                        </div>
                    </div>
                </div>
                
                <!-- Statistics Section - 2 statistics per row layout -->
                <div class="content-header" style="margin-top: 30px;">
                    <h2>📈 Detailed Statistics</h2>
                    <div class="controls">
                        <button class="control-btn" onclick="refreshStatistics()">🔄 Refresh Statistics</button>
                    </div>
                </div>
                <div class="stats-grid">
                    <div class="stat-card">
                        <h3>Domain Statistics <span id="domain-stats-total" class="submessage-count">(Total: -)</span></h3>
                        <canvas id="domain-stats-chart" width="400" height="300"></canvas>
                    </div>
                    <div class="stat-card">
                        <h3>Network Traffic <span id="network-traffic-total" class="submessage-count">(Total: -)</span></h3>
                        <canvas id="network-traffic-chart" width="400" height="300"></canvas>
                    </div>
                    <div class="stat-card">
                        <h3>Message Types Distribution <span id="submessage-total" class="submessage-count">(Total: -)</span></h3>
                        <canvas id="message-types-chart" width="400" height="300"></canvas>
                    </div>
                    <div class="stat-card">
                        <h3>Participant Activity</h3>
                        <canvas id="participant-activity-chart" width="400" height="300"></canvas>
                    </div>
                </div>
            </div>

            <!-- Participants Tab -->
            <div id="participants-tab" class="tab-content">
                <div class="content-header">
                    <h2>👥 Participants</h2>
                    <div class="controls">
                        <input type="text" id="participant-search" placeholder="🔍 Search participants..." onkeyup="filterParticipants()">
                        <button class="control-btn" onclick="refreshParticipants()">🔄 Refresh</button>
                    </div>
                </div>
                <div id="participant-list" class="list-container">
                    <!-- Participant items will be inserted here -->
                </div>
            </div>

            <!-- Writers Tab -->
            <div id="writers-tab" class="tab-content">
                <div class="content-header">
                    <h2>✍️ Writers</h2>
                    <div class="controls">
                        <input type="text" id="writer-search" placeholder="🔍 Search writers..." onkeyup="filterWriters()">
                        <button class="control-btn" onclick="refreshWriters()">🔄 Refresh</button>
                    </div>
                </div>
                <div id="writer-list" class="list-container">
                    <!-- Writer items will be inserted here -->
                </div>
            </div>

            <!-- Readers Tab -->
            <div id="readers-tab" class="tab-content">
                <div class="content-header">
                    <h2>👀 Readers</h2>
                    <div class="controls">
                        <input type="text" id="reader-search" placeholder="🔍 Search readers..." onkeyup="filterReaders()">
                        <button class="control-btn" onclick="refreshReaders()">🔄 Refresh</button>
                    </div>
                </div>
                <div id="reader-list" class="list-container">
                    <!-- Reader items will be inserted here -->
                </div>
            </div>

            <!-- Network Topology Tab -->
            <div id="topology-tab" class="tab-content">
                <div class="content-header">
                    <h2>🌐 Network Topology</h2>
                    <div class="controls">
                        <button class="control-btn" onclick="expandAllNodes()">📖 Expand All</button>
                        <button class="control-btn" onclick="collapseAllNodes()">📕 Collapse All</button>
                        <input type="text" id="topology-search" placeholder="🔍 Search nodes..." onkeyup="filterTopologyNodes()">
                        <button class="control-btn" onclick="resetTopologyView()">🔄 Reset</button>
                    </div>
                </div>
                <div id="topology-container" class="tree-container">
                    <div id="topology-tree"></div>
                </div>
                <div class="topology-info">
                    <div class="info-panel">
                        <h4>Hierarchy Legend</h4>
                        <div class="legend-item"><span class="tree-icon">🖥️</span> Machine/Node</div>
                        <div class="legend-item"><span class="tree-icon">⚙️</span> Process</div>
                        <div class="legend-item"><span class="tree-icon">🌐</span> Domain</div>
                        <div class="legend-item"><span class="tree-icon">👥</span> Participant</div>
                        <div class="legend-item"><span class="tree-icon">📋</span> Topic</div>
                        <div class="legend-item"><span class="tree-icon">✍️</span> Writer</div>
                        <div class="legend-item"><span class="tree-icon">👀</span> Reader</div>
                    </div>
                </div>
            </div>

            <!-- Nodes Details Tab -->
            <div id="nodes-tab" class="tab-content">
                <div class="content-header">
                    <h2>🏗️ Nodes Details</h2>
                    <div class="controls">
                        <button class="control-btn" onclick="refreshNodes()">🔄 Refresh</button>
                    </div>
                </div>
                <div id="nodes-list" class="list-container">
                    <!-- Nodes statistics will be inserted here -->
                </div>
            </div>

            <!-- Processes Tab -->
            <div id="processes-tab" class="tab-content">
                <div class="content-header">
                    <h2>⚙️ Processes</h2>
                    <div class="controls">
                        <input type="text" id="process-search" placeholder="🔍 Search processes..." onkeyup="filterProcesses()">
                        <button class="control-btn" onclick="refreshProcesses()">🔄 Refresh</button>
                    </div>
                </div>
                <div id="process-list" class="list-container">
                    <!-- Process items will be inserted here -->
                </div>
            </div>

            <!-- Topics Tab -->
            <div id="topics-tab" class="tab-content">
                <div class="content-header">
                    <h2>📋 Topics</h2>
                    <div class="controls">
                        <input type="text" id="topic-search" placeholder="🔍 Search topics..." onkeyup="filterTopics()">
                        <button class="control-btn" onclick="refreshTopics()">🔄 Refresh</button>
                    </div>
                </div>
                <div id="topic-list" class="list-container">
                    <!-- Topic items will be inserted here -->
                </div>
            </div>
        </main>
    </div>

    <!-- Include vis.js for network visualization -->
    <script src="https://unpkg.com/vis-network/standalone/umd/vis-network.min.js"></script>
    <!-- Include Chart.js for statistics -->
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <!-- Include Chart.js datalabels plugin -->
    <script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-datalabels@2"></script>

    <!-- QoS Details Modal -->
    <div id="qos-modal" class="modal">
        <div class="modal-content">
            <div class="modal-header">
                <h2 id="modal-title" class="modal-title">QoS Details</h2>
                <button class="modal-close" onclick="closeQoSModal()">&times;</button>
            </div>
            <div class="modal-body" id="modal-body">
                <!-- QoS content will be dynamically populated here -->
            </div>
        </div>
    </div>

    <script>
        %s
    </script>
</body>
</html>`, getEmbeddedCSS(), getEmbeddedJavaScript(jsonData))
}

// getEmbeddedCSS returns the complete CSS styles
func getEmbeddedCSS() string {
	return `
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Roboto', sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            color: #333;
        }

        .container {
            max-width: 1400px;
            margin: 0 auto;
            padding: 20px;
            background: white;
            min-height: 100vh;
            box-shadow: 0 0 30px rgba(0,0,0,0.1);
        }

        .header {
            text-align: center;
            margin-bottom: 30px;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border-radius: 15px;
        }

        .header h1 {
            font-size: 2.5em;
            margin-bottom: 15px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }

        .header-stats {
            display: flex;
            justify-content: center;
            gap: 30px;
            flex-wrap: wrap;
        }

        .stat-item {
            text-align: center;
            background: rgba(255,255,255,0.2);
            padding: 15px 20px;
            border-radius: 10px;
            backdrop-filter: blur(10px);
        }

        .stat-value {
            display: block;
            font-size: 2em;
            font-weight: bold;
            margin-bottom: 5px;
        }

        .stat-label {
            font-size: 0.9em;
            opacity: 0.9;
        }

        .tab-nav {
            display: flex;
            gap: 5px;
            margin-bottom: 20px;
            background: #f8f9fa;
            padding: 5px;
            border-radius: 10px;
            overflow-x: auto;
        }

        .tab-btn {
            flex: 1;
            min-width: 150px;
            padding: 12px 20px;
            border: none;
            background: transparent;
            border-radius: 8px;
            cursor: pointer;
            font-weight: 500;
            transition: all 0.3s ease;
            white-space: nowrap;
        }

        .tab-btn:hover {
            background: #e9ecef;
            transform: translateY(-2px);
        }

        .tab-btn.active {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            box-shadow: 0 4px 15px rgba(102, 126, 234, 0.4);
        }

        .tab-content {
            display: none;
            animation: fadeIn 0.5s ease;
        }

        .tab-content.active {
            display: block;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(20px); }
            to { opacity: 1; transform: translateY(0); }
        }

        .content-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 20px;
            padding-bottom: 15px;
            border-bottom: 2px solid #e9ecef;
        }

        .content-header h2 {
            color: #495057;
            font-size: 1.8em;
        }

        .controls {
            display: flex;
            gap: 10px;
            align-items: center;
        }

        .control-btn {
            padding: 8px 16px;
            border: none;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border-radius: 8px;
            cursor: pointer;
            font-weight: 500;
            transition: all 0.3s ease;
        }

        .control-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 15px rgba(102, 126, 234, 0.4);
        }

        .visualization-container {
            background: white;
            border-radius: 15px;
            padding: 20px;
            box-shadow: 0 4px 20px rgba(0,0,0,0.1);
            min-height: 500px;
        }

        /* Tree Container Styles */
        .tree-container {
            background: white;
            border-radius: 15px;
            padding: 20px;
            box-shadow: 0 4px 20px rgba(0,0,0,0.1);
            min-height: 500px;
            max-height: 600px;
            overflow-y: auto;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }

        .tree-node {
            margin: 2px 0;
            position: relative;
        }

        .tree-node-header {
            display: flex;
            align-items: center;
            padding: 8px 12px;
            border-radius: 6px;
            cursor: pointer;
            transition: all 0.2s ease;
            border-left: 3px solid transparent;
        }

        .tree-node-header:hover {
            background-color: #f8f9fa;
            border-left-color: #007bff;
        }

        .tree-node-header.active {
            background-color: #e3f2fd;
            border-left-color: #2196f3;
        }

        .tree-expand-icon {
            width: 16px;
            height: 16px;
            margin-right: 8px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 12px;
            cursor: pointer;
            transition: transform 0.2s ease;
        }

        .tree-expand-icon.expanded {
            transform: rotate(90deg);
        }

        .tree-node-icon {
            margin-right: 8px;
            font-size: 16px;
        }

        .tree-node-label {
            flex: 1;
            font-weight: 500;
            color: #333;
        }

        .tree-node-count {
            background: #6c757d;
            color: white;
            padding: 2px 6px;
            border-radius: 10px;
            font-size: 11px;
            margin-left: 8px;
        }

        .tree-node-status {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            margin-left: 8px;
        }

        .tree-node-status.active {
            background-color: #28a745;
        }

        .tree-node-status.inactive {
            background-color: #dc3545;
        }

        .tree-children {
            margin-left: 20px;
            border-left: 1px dashed #dee2e6;
            padding-left: 10px;
            display: none;
        }

        .tree-children.expanded {
            display: block;
        }

        /* Entity type specific colors - each entity has unique color scheme */
        .tree-node[data-type="machine"] .tree-node-header {
            background: linear-gradient(45deg, #667eea 0%, #764ba2 100%) !important;
            color: white !important;
            font-weight: bold !important;
            border-left-color: #667eea !important;
        }
        .tree-node[data-type="process"] .tree-node-header {
            background: #d4edda !important;
            border-left-color: #28a745 !important;
            color: #155724 !important;
        }
        .tree-node[data-type="domain"] .tree-node-header {
            background: #e7f3ff !important;
            border-left-color: #17a2b8 !important;
            color: #0c5460 !important;
        }
        .tree-node[data-type="participant"] .tree-node-header {
            background: #cce5ff !important;
            border-left-color: #007bff !important;
            color: #003f7f !important;
        }
        .tree-node[data-type="topic"] .tree-node-header {
            background: #f3e5f5 !important;
            border-left-color: #6f42c1 !important;
            color: #3a1e63 !important;
        }
        .tree-node[data-type="writer"] .tree-node-header {
            background: #ffe6cc !important;
            border-left-color: #fd7e14 !important;
            color: #8b4513 !important;
        }
        .tree-node[data-type="reader"] .tree-node-header {
            background: #ffebee !important;
            border-left-color: #e91e63 !important;
            color: #880e4f !important;
        }
        /* Fallback for nodes without specific type (use level-based coloring) */
        .tree-node[data-level="0"] .tree-node-header {
            background: linear-gradient(45deg, #667eea 0%, #764ba2 100%);
            color: white;
            font-weight: bold;
        }
        .tree-node[data-level="1"] .tree-node-header {
            background: #f8f9fa;
            border-left-color: #6c757d;
        }
        .tree-node[data-level="2"] .tree-node-header {
            background: #e9ecef;
            border-left-color: #6c757d;
        }
        .tree-node[data-level="3"] .tree-node-header {
            background: #f8f9fa;
            border-left-color: #6c757d;
        }
        .tree-node[data-level="4"] .tree-node-header {
            background: #e9ecef;
            border-left-color: #6c757d;
        }

        .tree-icon {
            display: inline-block;
            margin-right: 4px;
        }

        /* 跳转按钮样式 */
        .tree-jump-btn {
            background: #007bff;
            border: none;
            border-radius: 4px;
            color: white;
            padding: 4px 8px;
            margin-left: auto;
            margin-right: 8px;
            cursor: pointer;
            font-size: 12px;
            transition: all 0.2s ease;
            display: flex;
            align-items: center;
            gap: 4px;
            opacity: 0;
            transform: scale(0.8);
        }

        .tree-node-header:hover .tree-jump-btn {
            opacity: 1;
            transform: scale(1);
        }

        .tree-jump-btn:hover {
            background: #0056b3;
            transform: scale(1.05);
        }

        .tree-jump-btn .jump-icon {
            font-size: 10px;
        }

        /* 高亮显示样式 */
        .list-item.highlighted {
            background-color: #fff3cd !important;
            border: 2px solid #ffc107 !important;
            border-radius: 8px;
            animation: highlightPulse 1s ease-in-out;
        }

        @keyframes highlightPulse {
            0% { box-shadow: 0 0 0 0 rgba(255, 193, 7, 0.7); }
            70% { box-shadow: 0 0 0 10px rgba(255, 193, 7, 0); }
            100% { box-shadow: 0 0 0 0 rgba(255, 193, 7, 0); }
        }

        /* Search highlighting */
        .tree-node-highlight {
            background-color: #fff3cd !important;
            border-color: #ffc107 !important;
        }

        .overview-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
        }

        .overview-card {
            background: white;
            border-radius: 15px;
            padding: 20px;
            box-shadow: 0 4px 20px rgba(0,0,0,0.1);
            border-left: 4px solid #667eea;
        }

        .overview-card h3 {
            color: #495057;
            margin-bottom: 15px;
            font-size: 1.2em;
        }

        .summary-item, .network-item {
            padding: 8px 0;
            border-bottom: 1px solid #f1f3f4;
        }

        .summary-item:last-child, .network-item:last-child {
            border-bottom: none;
        }

        .quick-actions {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
        }

        .action-button {
            padding: 12px;
            border: none;
            background: #f8f9fa;
            border-radius: 8px;
            cursor: pointer;
            font-weight: 500;
            transition: all 0.3s ease;
            border: 2px solid transparent;
        }

        .action-button:hover {
            background: #e9ecef;
            border-color: #667eea;
            transform: translateY(-2px);
        }

        .stats-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 25px;
            margin-bottom: 30px;
        }

        .stat-card {
            background: white;
            border-radius: 15px;
            padding: 20px;
            box-shadow: 0 4px 20px rgba(0,0,0,0.1);
        }

        .stat-card h3 {
            color: #495057;
            margin-bottom: 15px;
            text-align: center;
        }

        .submessage-count {
            font-size: 0.8em;
            color: #6c757d;
            font-weight: normal;
        }

        .list-container {
            max-height: 600px;
            overflow-y: auto;
        }

        .list-item {
            background: white;
            border-radius: 10px;
            padding: 20px;
            margin-bottom: 15px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            border-left: 4px solid #667eea;
            transition: all 0.3s ease;
        }

        .list-item:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 20px rgba(0,0,0,0.15);
        }

        /* Nodes Details simple styles */
        .nodes-stats-list {
            max-height: 600px;
            overflow-y: auto;
            padding: 20px 0;
            display: flex;
            flex-direction: column;
            gap: 8px;
            text-align: left !important;
        }

        .node-stats-item {
            background: white;
            border-radius: 8px;
            padding: 20px;
            margin-bottom: 15px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            font-family: 'Courier New', 'Roboto Mono', monospace;
            font-size: 18px;
            font-weight: bold;
            border-left: 4px solid #667eea;
            display: grid;
            grid-template-columns: 220px 120px 140px 180px 180px 180px 180px;
            gap: 10px;
            align-items: center;
            text-align: left !important;
            overflow-x: auto;
        }


        .stat-line {
            color: #495057;
            font-size: 1.4em;
            font-weight: bold;
            padding: 8px 0;
            padding-left: 20px;
            border-left: 3px solid #bdc3c7;
            transition: all 0.2s ease;
            text-align: left !important;
        }

        .stat-line:hover {
            border-left-color: #667eea;
            padding-left: 25px;
            color: #2c3e50;
        }

        .info-panel {
            background: white;
            border-radius: 10px;
            padding: 15px;
            margin-top: 20px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }

        .legend-item {
            display: flex;
            align-items: center;
            padding: 5px 0;
        }

        .legend-color {
            width: 20px;
            height: 20px;
            border-radius: 50%;
            margin-right: 10px;
        }

        .legend-color.participant { background: #3498db; }
        .legend-color.domain { background: #e74c3c; }
        .legend-color.connection { background: #95a5a6; }
        .legend-color.discovery { background: #f39c12; }
        .legend-color.data { background: #2ecc71; }
        .legend-color.heartbeat { background: #e67e22; }

        /* Participant-specific styles */
        .participant-item {
            border-left: 4px solid #3498db;
        }

        .participant-header {
            display: flex;
            justify-content: space-between;
            align-items: flex-start;
            margin-bottom: 15px;
        }

        .participant-badges {
            display: flex;
            gap: 8px;
            flex-wrap: wrap;
        }

        .badge-children {
            background: #e8f5e8;
            color: #2ecc71;
        }

        .children-section {
            margin: 15px 0;
            padding: 15px;
            background: #f8f9fa;
            border-radius: 8px;
        }

        .children-list {
            margin-top: 10px;
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
        }

        .child-tag {
            background: #e9ecef;
            color: #495057;
            padding: 4px 8px;
            border-radius: 6px;
            font-size: 0.8em;
            display: inline-flex;
            align-items: center;
            gap: 4px;
        }

        .child-tag.writer {
            background: #fff3cd;
            color: #856404;
            border: 1px solid #ffeaa7;
        }

        .child-tag.reader {
            background: #d1ecf1;
            color: #0c5460;
            border: 1px solid #b8daff;
        }

        .child-tag.more {
            background: #dee2e6;
            color: #6c757d;
        }

        /* Writer and Reader specific styles */
        .writer-item {
            border-left: 4px solid #f39c12;
        }

        .reader-item {
            border-left: 4px solid #1abc9c;
        }

        .entity-header {
            display: flex;
            justify-content: space-between;
            align-items: flex-start;
            margin-bottom: 15px;
        }

        .entity-badges {
            display: flex;
            gap: 8px;
            flex-wrap: wrap;
        }

        .badge-writer {
            background: #fff3cd;
            color: #856404;
        }

        .badge-reader {
            background: #d1ecf1;
            color: #0c5460;
        }

        .badge-type {
            background: #f3e5f5;
            color: #7b1fa2;
        }

        .relationship-info {
            margin: 15px 0;
            padding: 15px;
            background: #e3f2fd;
            border-radius: 8px;
            border-left: 4px solid #2196f3;
        }

        .relationship-item {
            margin-top: 8px;
            font-size: 0.9em;
            color: #1565c0;
            font-family: monospace;
        }

        .locator-item {
            margin-bottom: 8px;
            font-size: 0.9em;
            color: #1565c0;
        }

        .advanced-info {
            margin: 15px 0;
            padding: 15px;
            background: #f3e5f5;
            border-radius: 8px;
            border-left: 4px solid #9c27b0;
        }

        .advanced-item {
            margin-bottom: 8px;
            font-size: 0.9em;
            color: #6a1b9a;
        }

        .entity-actions {
            display: flex;
            gap: 10px;
            margin-top: 15px;
            padding-top: 15px;
            border-top: 1px solid #e9ecef;
        }

        input[type="text"] {
            padding: 10px 15px;
            border: 2px solid #e9ecef;
            border-radius: 8px;
            font-size: 14px;
            min-width: 200px;
            transition: all 0.3s ease;
        }

        input[type="text"]:focus {
            outline: none;
            border-color: #667eea;
            box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
        }

        .notification {
            position: fixed;
            top: 20px;
            right: 20px;
            padding: 15px 20px;
            border-radius: 8px;
            color: white;
            font-weight: 500;
            z-index: 1000;
            animation: slideIn 0.3s ease;
        }

        .notification.success { background: #2ecc71; }
        .notification.warning { background: #f39c12; }
        .notification.error { background: #e74c3c; }

        @keyframes slideIn {
            from { transform: translateX(100%); opacity: 0; }
            to { transform: translateX(0); opacity: 1; }
        }

        /* Modal Styles */
        .modal {
            display: none;
            position: fixed;
            z-index: 1000;
            left: 0;
            top: 0;
            width: 100%;
            height: 100%;
            background-color: rgba(0, 0, 0, 0.5);
            animation: fadeIn 0.3s ease;
        }

        .modal-content {
            background-color: #fefefe;
            margin: 5% auto;
            padding: 0;
            border-radius: 12px;
            width: 90%;
            max-width: 800px;
            max-height: 80vh;
            overflow: hidden;
            box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
            animation: slideInDown 0.3s ease;
        }

        .modal-header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px 30px;
            border-radius: 12px 12px 0 0;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .modal-title {
            font-size: 1.5em;
            font-weight: 600;
            margin: 0;
        }

        .modal-close {
            background: none;
            border: none;
            color: white;
            font-size: 24px;
            cursor: pointer;
            padding: 0;
            width: 30px;
            height: 30px;
            border-radius: 50%;
            display: flex;
            align-items: center;
            justify-content: center;
            transition: background-color 0.3s ease;
        }

        .modal-close:hover {
            background-color: rgba(255, 255, 255, 0.2);
        }

        .modal-body {
            padding: 30px;
            max-height: 60vh;
            overflow-y: auto;
        }

        .qos-section {
            margin-bottom: 25px;
        }

        .qos-section h3 {
            color: #333;
            font-size: 1.2em;
            margin-bottom: 15px;
            padding-bottom: 8px;
            border-bottom: 2px solid #667eea;
        }

        .qos-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 15px;
        }

        .qos-item {
            background: #f8f9fa;
            border: 1px solid #e9ecef;
            border-radius: 8px;
            padding: 15px;
            transition: all 0.3s ease;
        }

        .qos-item:hover {
            background: #e9ecef;
            transform: translateY(-2px);
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
        }

        .qos-label {
            font-weight: 600;
            color: #495057;
            font-size: 0.9em;
            margin-bottom: 5px;
        }

        .qos-value {
            color: #667eea;
            font-family: monospace;
            font-size: 0.95em;
            word-break: break-all;
        }

        @keyframes fadeIn {
            from { opacity: 0; }
            to { opacity: 1; }
        }

        @keyframes slideInDown {
            from { transform: translateY(-50px); opacity: 0; }
            to { transform: translateY(0); opacity: 1; }
        }

        @media (max-width: 768px) {
            .container { padding: 10px; }
            .header h1 { font-size: 1.8em; }
            .header-stats { gap: 15px; }
            .content-header { flex-direction: column; gap: 15px; align-items: stretch; }
            .controls { justify-content: center; }
            .tab-nav { flex-direction: column; }
            .tab-btn { min-width: auto; }
            .overview-grid, .stats-grid { grid-template-columns: 1fr; }
            .modal-content { width: 95%; margin: 10% auto; }
            .modal-body { padding: 20px; }
            .qos-grid { grid-template-columns: 1fr; }
        }
        
        @media (max-width: 1024px) and (min-width: 769px) {
            .stats-grid { grid-template-columns: repeat(2, 1fr); }
        }
    `
}

// getEmbeddedJavaScript returns the complete JavaScript code with embedded data
func getEmbeddedJavaScript(jsonData string) string {
	// Remove backticks from jsonData to prevent template literal issues
	jsonData = strings.ReplaceAll(jsonData, "`", "'")
	
	return fmt.Sprintf(`
        // Embedded visualization data
        const globalData = %s;
        
        // Global variables
        let topologyNetwork = null;
        let charts = {};
        
        // Initialize the application
        document.addEventListener('DOMContentLoaded', function() {
            initializeApp();
        });
        
        function initializeApp() {
            console.log('🚀 Initializing RTPS Visualization...');
            
            try {
                updateHeaderStats();
                initializeTopology();
                initializeNodes();
                initializeParticipants();
                initializeWriters();
                initializeReaders();
                initializeProcesses();
                initializeTopics();
                // 只在首次加载时初始化 overview（因为它是默认激活的标签）
                initializeOverview();
                
                console.log('✅ Application initialized successfully');
                showNotification('Visualization loaded successfully!', 'success');
            } catch (error) {
                console.error('❌ Error initializing application:', error);
                showNotification('Error loading visualization: ' + error.message, 'error');
            }
        }
        
        function updateHeaderStats() {
            const stats = globalData.statisticsData;
            const network = globalData.networkTopology;
            const processes = globalData.processData;
            const topics = globalData.topicData;
            const participants = globalData.participantData;
            const writers = globalData.writerData;
            const readers = globalData.readerData;
            
            document.getElementById('total-processes').textContent = processes.processes.length;
            document.getElementById('total-participants').textContent = participants.participants.length;
            document.getElementById('total-topics').textContent = topics.topics.length;
            document.getElementById('total-writers').textContent = writers.writers.length;
            document.getElementById('total-readers').textContent = readers.readers.length;
            document.getElementById('total-packets').textContent = stats.messageStats.totalPackets;
        }
        
        function initializeOverview() {
            console.log('📊 Initializing overview...');
            
            // Update summary
            document.getElementById('analysis-time').textContent = new Date(globalData.generatedAt).toLocaleString();
            document.getElementById('network-nodes').textContent = globalData.networkTopology.nodes.length;
            document.getElementById('network-edges').textContent = globalData.networkTopology.edges.length;
            document.getElementById('message-types').textContent = Object.keys(globalData.statisticsData.messageStats.messageTypes).length;
            
            // Update network summary
            document.getElementById('unique-ips').textContent = globalData.statisticsData.networkStats.uniqueIPs.length;
            document.getElementById('port-count').textContent = Object.keys(globalData.statisticsData.networkStats.portDistribution).length;
            document.getElementById('domain-count').textContent = globalData.statisticsData.domainStats.length;
            
            // Initialize statistics charts in overview
            initializeStatistics();
        }
        
        function initializeTopology() {
            console.log('🌐 Initializing topology tree...');
            
            const treeNodes = globalData.networkTopology.treeNodes;
            const container = document.getElementById('topology-tree');
            
            if (!treeNodes || treeNodes.length === 0) {
                container.innerHTML = '<div class="text-center text-muted">No topology data available</div>';
                return;
            }
            
            container.innerHTML = buildTreeHTML(treeNodes);
            
            // 添加事件监听器
            addTreeEventListeners();
            
            console.log('✅ Topology tree initialized with', treeNodes.length, 'root nodes');
        }
        
        function buildTreeHTML(nodes) {
            let html = '';
            
            nodes.forEach(node => {
                html += buildTreeNodeHTML(node);
            });
            
            return html;
        }
        
        function buildTreeNodeHTML(node) {
            const hasChildren = node.children && node.children.length > 0;
            const expandIcon = hasChildren ? '▶' : '';
            const countBadge = hasChildren ? '<span class="tree-node-count">' + node.count + '</span>' : '';
            
            // 生成跳转按钮 - 为指定类型的节点添加
            let jumpButton = '';
            if (['process', 'participant', 'topic', 'writer', 'reader'].includes(node.type)) {
                const buttonText = getJumpButtonText(node.type);
                jumpButton = '<button class="tree-jump-btn" onclick="jumpToDetails(\'' + node.type + '\', \'' + node.id + '\', event)" title="跳转到' + buttonText + '详细信息">' +
                           '<i class="jump-icon">🔗</i>' +
                           '</button>';
            }
            
            let html = '<div class="tree-node" data-level="' + node.level + '" data-id="' + node.id + '" data-type="' + node.type + '">' +
                       '<div class="tree-node-header" onclick="toggleTreeNode(\'' + node.id + '\')">' +
                       '<span class="tree-expand-icon">' + expandIcon + '</span>' +
                       '<span class="tree-node-icon">' + node.icon + '</span>' +
                       '<span class="tree-node-label">' + node.name + '</span>' +
                       countBadge +
                       '<span class="tree-node-status ' + node.status + '"></span>' +
                       jumpButton +
                       '</div>';
            
            if (hasChildren) {
                html += '<div class="tree-children" id="children-' + node.id + '">';
                node.children.forEach(child => {
                    html += buildTreeNodeHTML(child);
                });
                html += '</div>';
            }
            
            html += '</div>';
            return html;
        }
        
        function addTreeEventListeners() {
            // 节点点击事件已通过onclick添加
            console.log('Tree event listeners added');
        }
        
        function getJumpButtonText(nodeType) {
            const typeMap = {
                'process': '进程',
                'participant': '参与者',
                'topic': '主题',
                'writer': '发布者',
                'reader': '订阅者'
            };
            return typeMap[nodeType] || nodeType;
        }
        
        function jumpToDetails(nodeType, nodeId, event) {
            // 阻止事件冒泡，避免触发节点展开/折叠
            event.stopPropagation();
            
            console.log('Jumping to', nodeType, 'details for', nodeId);
            
            // 根据节点类型跳转到对应的详细信息页面
            switch(nodeType) {
                case 'process':
                    showTab('processes');
                    highlightListItem('process-list', nodeId);
                    break;
                case 'participant':
                    showTab('participants');
                    highlightListItem('participant-list', nodeId);
                    break;
                case 'topic':
                    showTab('topics');
                    highlightListItem('topic-list', nodeId);
                    break;
                case 'writer':
                    showTab('writers');
                    highlightListItem('writer-list', nodeId);
                    break;
                case 'reader':
                    showTab('readers');
                    highlightListItem('reader-list', nodeId);
                    break;
                default:
                    console.warn('Unknown node type:', nodeType);
            }
        }
        
        function highlightListItem(containerId, itemId) {
            // 移除所有高亮
            const container = document.getElementById(containerId);
            if (!container) return;
            
            const allItems = container.querySelectorAll('.list-item');
            allItems.forEach(item => item.classList.remove('highlighted'));
            
            // 高亮匹配的项目
            let targetItem = null;
            
            // 首先尝试精确匹配data-id
            targetItem = Array.from(allItems).find(item => {
                return item.getAttribute('data-id') === itemId;
            });
            
            // 如果没找到，尝试其他匹配方式
            if (!targetItem) {
                targetItem = Array.from(allItems).find(item => {
                    const dataId = item.getAttribute('data-id');
                    // 对于进程节点，可能需要匹配processID或PID
                    if (containerId === 'process-list' && itemId.includes('_proc_')) {
                        const processIdMatch = itemId.match(/_proc_(\d+)$/);
                        if (processIdMatch) {
                            const processId = processIdMatch[1];
                            return item.textContent.includes('PID: ' + processId);
                        }
                    }
                    // 其他类型的匹配
                    return item.textContent.includes(itemId) || 
                           item.querySelector('[data-id="' + itemId + '"]') ||
                           (dataId && dataId.includes(itemId));
                });
            }
            
            if (targetItem) {
                targetItem.classList.add('highlighted');
                // 滚动到目标项目
                targetItem.scrollIntoView({ behavior: 'smooth', block: 'center' });
                
                // 3秒后移除高亮
                setTimeout(() => {
                    targetItem.classList.remove('highlighted');
                }, 3000);
                
                console.log('✅ Highlighted item:', itemId, 'in', containerId);
            } else {
                console.warn('❌ Could not find item with ID:', itemId, 'in container:', containerId);
            }
        }
        
        function toggleTreeNode(nodeId) {
            const childrenContainer = document.getElementById('children-' + nodeId);
            const expandIcon = document.querySelector('[data-id="' + nodeId + '"] .tree-expand-icon');
            
            if (childrenContainer) {
                if (childrenContainer.classList.contains('expanded')) {
                    childrenContainer.classList.remove('expanded');
                    expandIcon.classList.remove('expanded');
                    expandIcon.textContent = '▶';
                } else {
                    childrenContainer.classList.add('expanded');
                    expandIcon.classList.add('expanded');
                    expandIcon.textContent = '▼';
                }
            }
            
            // 显示节点详细信息
            showNodeDetails(nodeId);
        }
        
        function showNodeDetails(nodeId) {
            // 查找节点数据
            const nodeData = findNodeInTree(globalData.networkTopology.treeNodes, nodeId);
            if (nodeData) {
                const details = nodeData.details || {};
                let detailsText = nodeData.type + ': ' + nodeData.name;
                
                if (Object.keys(details).length > 0) {
                    detailsText += '\\n' + Object.entries(details)
                        .map(function(entry) { return entry[0] + ': ' + entry[1]; })
                        .join('\\n');
                }
                
                showNotification(detailsText, 'info');
            }
        }
        
        function findNodeInTree(nodes, targetId) {
            for (let i = 0; i < nodes.length; i++) {
                const node = nodes[i];
                if (node.id === targetId) {
                    return node;
                }
                if (node.children) {
                    const found = findNodeInTree(node.children, targetId);
                    if (found) return found;
                }
            }
            return null;
        }
        
        function initializeStatistics() {
            console.log('📈 Initializing statistics...');
            
            createMessageTypesChart();
            createDomainStatsChart();
            createParticipantActivityChart();
            createNetworkTrafficChart();
        }
        
        function createMessageTypesChart() {
            // 销毁之前的图表实例
            if (charts.messageTypes) {
                charts.messageTypes.destroy();
            }
            
            const ctx = document.getElementById('message-types-chart').getContext('2d');
            const messageTypes = globalData.statisticsData.messageStats.messageTypes;
            
            // 计算submessage总数
            const totalSubmessages = Object.values(messageTypes).reduce((sum, count) => sum + count, 0);
            
            // 更新标题中的总数显示
            const submessageTotalElement = document.getElementById('submessage-total');
            if (submessageTotalElement) {
                submessageTotalElement.textContent = '(Total: ' + totalSubmessages.toLocaleString() + ')';
            }
            
            // 映射消息类型标签，将 HEARTBEAT 改为 HB
            const mappedLabels = Object.keys(messageTypes).map(label => {
                if (label.toUpperCase() === 'HEARTBEAT') {
                    return 'HB';
                }
                return label;
            });
            
            charts.messageTypes = new Chart(ctx, {
                type: 'pie',
                data: {
                    labels: mappedLabels,
                    datasets: [{
                        data: Object.values(messageTypes),
                        backgroundColor: [
                            '#3498db', '#e74c3c', '#f39c12', '#2ecc71', '#9b59b6', '#ff6b35'
                        ]
                    }]
                },
                plugins: [ChartDataLabels],
                options: {
                    responsive: false,
                    maintainAspectRatio: true,
                    plugins: {
                        legend: {
                            position: 'bottom'
                        },
                        datalabels: {
                            display: false
                        }
                    }
                }
            });
        }
        
        function createDomainStatsChart() {
            // 销毁之前的图表实例
            if (charts.domainStats) {
                charts.domainStats.destroy();
            }
            
            const ctx = document.getElementById('domain-stats-chart').getContext('2d');
            const domainStats = globalData.statisticsData.domainStats;
            
            // 计算域总数
            const totalDomains = domainStats.length;
            
            // 更新标题中的域总数显示
            const domainStatsTotalElement = document.getElementById('domain-stats-total');
            if (domainStatsTotalElement) {
                domainStatsTotalElement.textContent = '(Total: ' + totalDomains.toLocaleString() + ' domains)';
            }
            
            charts.domainStats = new Chart(ctx, {
                type: 'pie',
                plugins: [ChartDataLabels],
                data: {
                    labels: domainStats.map(d => ` + "`" + `Domain ${d.domainID}` + "`" + `),
                    datasets: [{
                        data: domainStats.map(d => d.participantCount),
                        backgroundColor: ['#e74c3c', '#f39c12', '#2ecc71', '#9b59b6']
                    }]
                },
                options: {
                    responsive: false,
                    maintainAspectRatio: true,
                    plugins: {
                        legend: {
                            position: 'bottom'
                        },
                        datalabels: {
                            display: false
                        }
                    }
                }
            });
        }
        
        function createParticipantActivityChart() {
            // 销毁之前的图表实例
            if (charts.participantActivity) {
                charts.participantActivity.destroy();
            }
            
            const ctx = document.getElementById('participant-activity-chart').getContext('2d');
            const participantStats = globalData.statisticsData.participantStats;
            
            // 合并发送和接收消息数据用于饼状图
            const combinedData = [];
            const combinedLabels = [];
            participantStats.forEach(p => {
                if (p.messagesSent > 0) {
                    combinedData.push(p.messagesSent);
                    combinedLabels.push(p.processName + ' (Sent)');
                }
                if (p.messagesReceived > 0) {
                    combinedData.push(p.messagesReceived);
                    combinedLabels.push(p.processName + ' (Received)');
                }
            });
            
            charts.participantActivity = new Chart(ctx, {
                type: 'pie',
                plugins: [ChartDataLabels],
                data: {
                    labels: combinedLabels,
                    datasets: [{
                        data: combinedData,
                        backgroundColor: ['#2ecc71', '#3498db', '#e74c3c', '#f39c12', '#9b59b6', '#1abc9c']
                    }]
                },
                options: {
                    responsive: false,
                    maintainAspectRatio: true,
                    plugins: {
                        legend: {
                            position: 'bottom'
                        },
                        datalabels: {
                            display: false
                        }
                    }
                }
            });
        }
        
        function createNetworkTrafficChart() {
            // 销毁之前的图表实例
            if (charts.networkTraffic) {
                charts.networkTraffic.destroy();
            }
            
            const ctx = document.getElementById('network-traffic-chart').getContext('2d');
            
            // 计算总包个数
            const unicastCount = globalData.statisticsData.networkStats.unicastTraffic;
            const multicastCount = globalData.statisticsData.networkStats.multicastTraffic;
            const totalPackets = unicastCount + multicastCount;
            
            // 更新标题中的总包个数显示
            const networkTrafficTotalElement = document.getElementById('network-traffic-total');
            if (networkTrafficTotalElement) {
                networkTrafficTotalElement.textContent = '(Total: ' + totalPackets.toLocaleString() + ' packets)';
            }
            
            charts.networkTraffic = new Chart(ctx, {
                type: 'pie',
                plugins: [ChartDataLabels],
                data: {
                    labels: ['Unicast', 'Multicast'],
                    datasets: [{
                        data: [unicastCount, multicastCount],
                        backgroundColor: ['#f39c12', '#3498db']
                    }]
                },
                options: {
                    responsive: false,
                    maintainAspectRatio: true,
                    plugins: {
                        legend: {
                            position: 'bottom'
                        },
                        datalabels: {
                            display: false
                        }
                    }
                }
            });
        }
        
        
        
        
        
        function initializeNodes() {
            console.log('🏗️ Initializing nodes details...');
            displayNodes();
        }
        
        function displayNodes() {
            const container = document.getElementById('nodes-list');
            
            // 基于Network Topology构建hostname统计
            const nodeStats = buildNodeStatsFromTopology();
            
            if (!nodeStats || nodeStats.length === 0) {
                container.innerHTML = '<div class="text-center text-muted">No nodes data available</div>';
                return;
            }
            
            let html = '<div class="nodes-stats-list">';
            
            nodeStats.forEach(node => {
                // 使用CSS Grid布局，每个字段作为独立的div元素
                const hostname = node.hostname;
                const domain = 'domain: ' + node.domainCount;
                const process = 'process: ' + node.processCount;
                const participant = 'participant: ' + node.participantCount;
                const topic = 'topic: ' + node.topicCount;
                const writer = 'writer: ' + node.writerCount;
                const reader = 'reader: ' + node.readerCount;
                
                html += ` + "`" + `
                    <div class="node-stats-item">
                        <div>` + "`" + ` + hostname + ` + "`" + `</div>
                        <div>` + "`" + ` + domain + ` + "`" + `</div>
                        <div>` + "`" + ` + process + ` + "`" + `</div>
                        <div>` + "`" + ` + participant + ` + "`" + `</div>
                        <div>` + "`" + ` + topic + ` + "`" + `</div>
                        <div>` + "`" + ` + writer + ` + "`" + `</div>
                        <div>` + "`" + ` + reader + ` + "`" + `</div>
                    </div>
                ` + "`" + `;
            });
            
            html += '</div>';
            container.innerHTML = html;
        }
        
        function buildNodeStatsFromTopology() {
            // 从Network Topology的TreeNodes中提取统计信息
            const treeNodes = globalData.networkTopology?.treeNodes || [];
            
            // 递归遍历TreeNodes来统计信息
            function traverseTreeNodes(nodes, hostname = '', stats = null) {
                nodes.forEach(node => {
                    switch (node.type) {
                        case 'machine':
                            // 从machine节点提取hostname
                            const machineHostname = node.name.split(' ')[0]; // 提取 "DESKTOP-8AGNBVI" 部分
                            const newStats = {
                                hostname: machineHostname,
                                domainSet: new Set(),
                                processCount: 0,
                                participantCount: 0,
                                topicSet: new Set(),
                                writerCount: 0,
                                readerCount: 0
                            };
                            
                            // 递归处理子节点
                            if (node.children) {
                                traverseTreeNodes(node.children, machineHostname, newStats);
                            }
                            
                            // 添加到结果中
                            results.push({
                                hostname: newStats.hostname,
                                domainCount: newStats.domainSet.size,
                                processCount: newStats.processCount,
                                participantCount: newStats.participantCount,
                                topicCount: newStats.topicSet.size,
                                writerCount: newStats.writerCount,
                                readerCount: newStats.readerCount
                            });
                            break;
                            
                        case 'process':
                            // 统计进程实例数，而不是唯一进程名数
                            if (stats) {
                                stats.processCount++; // 直接计数进程节点
                            }
                            
                            // 递归处理子节点
                            if (node.children) {
                                traverseTreeNodes(node.children, hostname, stats);
                            }
                            break;
                            
                        case 'domain':
                            // 提取domain ID
                            const domainMatch = node.name.match(/Domain (\d+)/);
                            if (domainMatch && stats) {
                                stats.domainSet.add(parseInt(domainMatch[1]));
                            }
                            
                            // 递归处理子节点
                            if (node.children) {
                                traverseTreeNodes(node.children, hostname, stats);
                            }
                            break;
                            
                        case 'participant':
                            // 统计participant
                            if (stats) {
                                stats.participantCount++;
                            }
                            
                            // 递归处理子节点
                            if (node.children) {
                                traverseTreeNodes(node.children, hostname, stats);
                            }
                            break;
                            
                        case 'topic':
                            // 统计topic - 使用全局去重而不是按hostname去重
                            if (stats && node.name) {
                                // 为了与TopicData保持一致，我们使用全局主题列表而非TreeNodes中的重复统计
                                // 暂时先统计，后续可能需要改为从TopicData获取每个hostname的主题
                                stats.topicSet.add(node.name);
                            }
                            
                            // 递归处理子节点
                            if (node.children) {
                                traverseTreeNodes(node.children, hostname, stats);
                            }
                            break;
                            
                        case 'writer':
                            // 统计writer
                            if (stats) {
                                stats.writerCount++;
                            }
                            break;
                            
                        case 'reader':
                            // 统计reader
                            if (stats) {
                                stats.readerCount++;
                            }
                            break;
                    }
                });
            }
            
            const results = [];
            traverseTreeNodes(treeNodes);
            
            // 修正主题统计：基于实际TopicData按hostname分组
            const allTopics = globalData.topicData?.topics || [];
            const writers = globalData.writerData?.writers || [];
            const readers = globalData.readerData?.readers || [];
            const participants = globalData.participantData?.participants || [];
            
            // 建立participantGUID到hostname的映射
            const guidToHostname = new Map();
            participants.forEach(p => {
                if (p.hostname && p.participantGUID) {
                    guidToHostname.set(p.participantGUID, p.hostname);
                }
            });
            
            // 为每个hostname收集实际的topics
            const hostnameTopics = new Map();
            
            // 从writers收集topics
            writers.forEach(w => {
                if (w.participantGUID && w.topicName) {
                    const hostname = guidToHostname.get(w.participantGUID);
                    if (hostname) {
                        if (!hostnameTopics.has(hostname)) {
                            hostnameTopics.set(hostname, new Set());
                        }
                        hostnameTopics.get(hostname).add(w.topicName);
                    }
                }
            });
            
            // 从readers收集topics
            readers.forEach(r => {
                if (r.participantGUID && r.topicName) {
                    const hostname = guidToHostname.get(r.participantGUID);
                    if (hostname) {
                        if (!hostnameTopics.has(hostname)) {
                            hostnameTopics.set(hostname, new Set());
                        }
                        hostnameTopics.get(hostname).add(r.topicName);
                    }
                }
            });
            
            // 更新结果中的topic count
            results.forEach(result => {
                const topicSet = hostnameTopics.get(result.hostname);
                if (topicSet) {
                    result.topicCount = topicSet.size;
                }
            });
            
            return results;
        }
        
        function refreshNodes() {
            displayNodes();
            showNotification('Nodes refreshed', 'success');
        }

        function initializeParticipants() {
            console.log('👥 Initializing participants...');
            displayParticipants();
        }
        
        function initializeWriters() {
            console.log('✍️ Initializing writers...');
            displayWriters();
        }
        
        function initializeReaders() {
            console.log('👀 Initializing readers...');
            displayReaders();
        }
        
        
        function initializeProcesses() {
            console.log('⚙️ Initializing processes...');
            displayProcesses();
        }
        
        function displayProcesses() {
            const container = document.getElementById('process-list');
            const processes = globalData.processData.processes;
            
            if (!processes || processes.length === 0) {
                container.innerHTML = '<div class="text-center text-muted">No processes found</div>';
                return;
            }
            
            let html = '';
            processes.forEach(process => {
                html += ` + "`" + `
                    <div class="list-item" data-id="${process.processGUID}">
                        <h4>${process.processName} (PID: ${process.processID})</h4>
                        <div class="metadata">
                            <div><strong>GUID:</strong> ${process.processGUID}</div>
                            <div><strong>Hostname:</strong> ${process.hostname}</div>
                            <div><strong>Domain:</strong> ${process.domainID}</div>
                            <div><strong>Vendor:</strong> ${process.vendorID}</div>
                            <div><strong>Default Locator:</strong> ${process.defaultLocator}</div>
                            <div><strong>Meta Locator:</strong> ${process.metaLocator}</div>
                            <div><strong>Participants:</strong> ${process.participants.length}</div>
                        </div>
                    </div>
                ` + "`" + `;
            });
            
            container.innerHTML = html;
        }
        
        function initializeTopics() {
            console.log('📋 Initializing topics...');
            displayTopics();
        }
        
        function displayParticipants() {
            const container = document.getElementById('participant-list');
            const participants = globalData.participantData.participants;
            
            if (!participants || participants.length === 0) {
                container.innerHTML = '<div class="text-center text-muted">No participants found</div>';
                return;
            }
            
            let html = '';
            participants.forEach(participant => {
                const childrenCount = participant.children.length;
                const writersCount = participant.writers.length;
                const readersCount = participant.readers.length;
                
                html += ` + "`" + `
                    <div class="list-item participant-item" data-id="${participant.participantGUID}">
                        <div class="participant-header">
                            <h4>👥 ${participant.processName}</h4>
                            <div class="participant-badges">
                                <span class="badge badge-domain">Domain ${participant.domainID}</span>
                                <span class="badge badge-children">${childrenCount} entities</span>
                            </div>
                        </div>
                        
                        <div class="metadata">
                            <div><strong>GUID:</strong> ${truncateText(participant.participantGUID, 50)}</div>
                            <div><strong>GUID Prefix:</strong> ${participant.guidPrefix}</div>
                            <div><strong>Domain ID:</strong> ${participant.domainID}</div>
                            <div><strong>Process ID:</strong> ${participant.processID}</div>
                            <div><strong>Hostname:</strong> ${participant.hostname}</div>
                            <div><strong>Vendor:</strong> ${participant.vendorID}</div>
                            <div><strong>Protocol:</strong> ${participant.protocolVersion}</div>
                            <div><strong>Default Unicast:</strong> ${participant.defaultUnicastLoc}</div>
                            <div><strong>Default Multicast:</strong> ${participant.defaultMulticastLoc}</div>
                            <div><strong>Meta Unicast:</strong> ${participant.metaUnicastLoc}</div>
                            <div><strong>Meta Multicast:</strong> ${participant.metaMulticastLoc}</div>
                            <div><strong>Lease Duration:</strong> ${participant.leaseDuration}</div>
                            <div><strong>Builtin Endpoints:</strong> ${participant.builtinEndpoints}</div>
                            <div><strong>Expects Inline QoS:</strong> ${participant.expectsInlineQoS ? 'Yes' : 'No'}</div>
                            <div><strong>AutoCore Code:</strong> ${participant.autoCoreCode && participant.autoCoreCode.length > 0 ? 
                                (() => {
                                    const bytes = Array.from(atob(participant.autoCoreCode), c => c.charCodeAt(0));
                                    return '[' + bytes.join(' ') + '] (' + bytes.length + ' bytes)';
                                })() : 'Not available'}</div>
                            ${participant.userData ? '<div><strong>User Data:</strong> ' + (participant.userData.length > 50 ? participant.userData.substring(0, 50) + '...' : participant.userData) + '</div>' : ''}
                            ${participant.groupData ? '<div><strong>Group Data:</strong> ' + (participant.groupData.length > 50 ? participant.groupData.substring(0, 50) + '...' : participant.groupData) + '</div>' : ''}
                            ${participant.topicData ? '<div><strong>Topic Data:</strong> ' + (participant.topicData.length > 50 ? participant.topicData.substring(0, 50) + '...' : participant.topicData) + '</div>' : ''}
                            ${participant.manualLivelinessCount > 0 ? '<div><strong>Manual Liveliness Count:</strong> ' + participant.manualLivelinessCount + '</div>' : ''}
                            ${participant.staticDiscoveryData && participant.staticDiscoveryData.length > 0 ? '<div><strong>Static Discovery Data:</strong> ' + 
                                (() => {
                                    const bytes = Array.from(atob(participant.staticDiscoveryData), c => c.charCodeAt(0));
                                    const hexStr = bytes.map(b => b.toString(16).padStart(2, '0')).join(' ');
                                    return '[' + hexStr + '] (' + bytes.length + ' bytes)';
                                })() + '</div>' : ''}
                            ${participant.propertyList && participant.propertyList.length > 0 ? '<div><strong>Property List:</strong><br>' + participant.propertyList.map(prop => {
                                const cleanProp = prop.replace(/[\x00-\x1F\x7F-\x9F]/g, '').trim();
                                if (cleanProp.startsWith('ProcessName')) {
                                    return '<div style="margin-left: 20px;"><strong>Process Name:</strong> ' + cleanProp.replace('ProcessName', '') + '</div>';
                                } else if (cleanProp.startsWith('Pid')) {
                                    return '<div style="margin-left: 20px;"><strong>Pid:</strong> ' + cleanProp.replace('Pid', '') + '</div>';
                                } else if (cleanProp.startsWith('Hostname')) {
                                    return '<div style="margin-left: 20px;"><strong>Hostname:</strong> ' + cleanProp.replace('Hostname', '') + '</div>';
                                } else if (cleanProp.startsWith('SHMLocator')) {
                                    return '<div style="margin-left: 20px;"><strong>SHM Locator:</strong> ' + cleanProp.replace('SHMLocator', '') + '</div>';
                                } else if (cleanProp.startsWith('SHMSize')) {
                                    return '<div style="margin-left: 20px;"><strong>SHM Size:</strong> ' + cleanProp.replace('SHMSize', '') + '</div>';
                                } else if (cleanProp.startsWith('AutocoreCode')) {
                                    return '<div style="margin-left: 20px;"><strong>AutoCore Code:</strong> ' + cleanProp.replace('AutocoreCode', '') + '</div>';
                                } else if (cleanProp) {
                                    return '<div style="margin-left: 20px;"><strong>Property:</strong> ' + cleanProp + '</div>';
                                }
                                return '';
                            }).filter(item => item !== '').join('') + '</div>' : ''}
                        </div>
                        
                        <div class="children-section">
                            <strong>📝 Children Entities (${childrenCount}):</strong>
                            <div class="children-list">
                ` + "`" + `;
                
                if (childrenCount > 0) {
                    participant.children.forEach((child, index) => {
                        if (index < 5) { // Show first 5 children
                            const icon = child.type === 'writer' ? '✍️' : '👀';
                            html += ` + "`" + `<span class="child-tag ${child.type}">${icon} ${child.topicName}</span>` + "`" + `;
                        } else if (index === 5) {
                            html += ` + "`" + `<span class="child-tag more">+${childrenCount - 5} more</span>` + "`" + `;
                        }
                    });
                } else {
                    html += '<span class="text-muted">No children entities</span>';
                }
                
                html += ` + "`" + `
                            </div>
                        </div>
                        
                        <div class="participant-actions">
                            <button class="action-btn" onclick="highlightParticipantInTopology('${participant.participantGUID}')">
                                🔗 Show in Topology
                            </button>
                        </div>
                    </div>
                ` + "`" + `;
            });
            
            container.innerHTML = html;
        }
        
        function displayWriters() {
            const container = document.getElementById('writer-list');
            const writers = globalData.writerData.writers;
            
            if (!writers || writers.length === 0) {
                container.innerHTML = '<div class="text-center text-muted">No writers found</div>';
                return;
            }
            
            let html = '';
            writers.forEach(writer => {
                html += ` + "`" + `
                    <div class="list-item writer-item" data-id="${writer.writerGUID}">
                        <div class="entity-header">
                            <h4>✍️ ${writer.topicName}</h4>
                            <div class="entity-badges">
                                <span class="badge badge-writer">Writer</span>
                                <span class="badge badge-type">${writer.typeName}</span>
                            </div>
                        </div>
                        
                        <div class="metadata">
                            <div><strong>Writer GUID:</strong> ${truncateText(writer.writerGUID, 50)}</div>
                            <div><strong>Topic Name:</strong> ${writer.topicName}</div>
                            <div><strong>Type Name:</strong> ${writer.typeName}</div>
                        </div>
                        ` + "`" + `;
                
                // Add QoS information directly in the table
                if (writer.qosProfile && Object.keys(writer.qosProfile).length > 0) {
                    html += ` + "`" + `
                        <div class="qos-section">
                            <h4>⚙️ Quality of Service (QoS) Parameters</h4>
                            <div class="metadata">
                    ` + "`" + `;
                    
                    for (const [key, value] of Object.entries(writer.qosProfile)) {
                        html += ` + "`" + `
                                <div><strong>${key}:</strong> ${value}</div>
                        ` + "`" + `;
                    }
                    
                    html += ` + "`" + `
                            </div>
                        </div>
                    ` + "`" + `;
                } else {
                    html += ` + "`" + `
                        <div class="qos-section">
                            <h4>⚙️ Quality of Service (QoS) Parameters</h4>
                            <div class="text-center text-muted">No QoS parameters available</div>
                        </div>
                    ` + "`" + `;
                }
                
                html += ` + "`" + `
                        <div class="entity-actions">
                            <button class="action-btn" onclick="showParentParticipant('${writer.participantGUID}')">
                                👥 Show Parent
                            </button>
                        </div>
                    </div>
                ` + "`" + `;
            });
            
            container.innerHTML = html;
        }
        
        function displayReaders() {
            const container = document.getElementById('reader-list');
            const readers = globalData.readerData.readers;
            
            if (!readers || readers.length === 0) {
                container.innerHTML = '<div class="text-center text-muted">No readers found</div>';
                return;
            }
            
            let html = '';
            readers.forEach(reader => {
                html += ` + "`" + `
                    <div class="list-item reader-item" data-id="${reader.readerGUID}">
                        <div class="entity-header">
                            <h4>👀 ${reader.topicName}</h4>
                            <div class="entity-badges">
                                <span class="badge badge-reader">Reader</span>
                                <span class="badge badge-type">${reader.typeName}</span>
                            </div>
                        </div>
                        
                        <div class="metadata">
                            <div><strong>Reader GUID:</strong> ${truncateText(reader.readerGUID, 50)}</div>
                            <div><strong>Topic Name:</strong> ${reader.topicName}</div>
                            <div><strong>Type Name:</strong> ${reader.typeName}</div>
                        </div>
                        ` + "`" + `;
                
                // Add QoS information directly in the table
                if (reader.qosProfile && Object.keys(reader.qosProfile).length > 0) {
                    html += ` + "`" + `
                        <div class="qos-section">
                            <h4>⚙️ Quality of Service (QoS) Parameters</h4>
                            <div class="metadata">
                    ` + "`" + `;
                    
                    for (const [key, value] of Object.entries(reader.qosProfile)) {
                        html += ` + "`" + `
                                <div><strong>${key}:</strong> ${value}</div>
                        ` + "`" + `;
                    }
                    
                    html += ` + "`" + `
                            </div>
                        </div>
                    ` + "`" + `;
                } else {
                    html += ` + "`" + `
                        <div class="qos-section">
                            <h4>⚙️ Quality of Service (QoS) Parameters</h4>
                            <div class="text-center text-muted">No QoS parameters available</div>
                        </div>
                    ` + "`" + `;
                }
                
                html += ` + "`" + `
                        <div class="entity-actions">
                            <button class="action-btn" onclick="showParentParticipant('${reader.participantGUID}')">
                                👥 Show Parent
                            </button>
                        </div>
                    </div>
                ` + "`" + `;
            });
            
            container.innerHTML = html;
        }
        
        function displayTopics() {
            const container = document.getElementById('topic-list');
            const topics = globalData.topicData.topics;
            
            if (!topics || topics.length === 0) {
                container.innerHTML = '<div class="text-center text-muted">No topics found</div>';
                return;
            }
            
            let html = '';
            topics.forEach(topic => {
                html += ` + "`" + `
                    <div class="list-item" data-id="${topic.topicName}">
                        <h4>${topic.topicName}</h4>
                        <div class="metadata">
                            <div><strong>Type:</strong> ${topic.topicType}</div>
                            <div><strong>Publishers:</strong> ${topic.publishers.length}</div>
                            <div><strong>Subscribers:</strong> ${topic.subscribers.length}</div>
                        </div>
                        <div class="participants">
                            <strong>Publishers:</strong> ${topic.publishers.slice(0, 3).join(', ')}${topic.publishers.length > 3 ? '...' : ''}
                            <br>
                            <strong>Subscribers:</strong> ${topic.subscribers.slice(0, 3).join(', ')}${topic.subscribers.length > 3 ? '...' : ''}
                        </div>
                    </div>
                ` + "`" + `;
            });
            
            container.innerHTML = html;
        }
        
        // Tab management
        function showTab(tabName) {
            // Hide all tabs
            const allTabs = document.querySelectorAll('.tab-content');
            const allBtns = document.querySelectorAll('.tab-btn');
            
            allTabs.forEach(tab => tab.classList.remove('active'));
            allBtns.forEach(btn => btn.classList.remove('active'));
            
            // Show selected tab
            document.getElementById(tabName + '-tab').classList.add('active');
            
            // Find and activate the corresponding button safely
            const targetBtn = Array.from(allBtns).find(btn => {
                const onclick = btn.getAttribute('onclick');
                return onclick && onclick.includes("'" + tabName + "'");
            });
            if (targetBtn) {
                targetBtn.classList.add('active');
            }
            
            // 延迟初始化特定标签页的内容，避免重复初始化
            setTimeout(() => {
                if (tabName === 'overview') {
                    initializeOverview();
                }
            }, 100);
            
            console.log(` + "`" + `📑 Switched to ${tabName} tab` + "`" + `);
        }
        
        // Control functions
        function fitNetwork() {
            if (topologyNetwork) {
                topologyNetwork.fit();
                showNotification('Network view fitted', 'success');
            }
        }
        
        function resetNetwork() {
            if (topologyNetwork) {
                topologyNetwork.redraw();
                topologyNetwork.fit();
                showNotification('Network view reset', 'success');
            }
        }
        
        function refreshData() {
            showNotification('Data refreshed', 'success');
        }
        
        // Topology Tree Control Functions
        function expandAllNodes() {
            const allChildrenContainers = document.querySelectorAll('.tree-children');
            const allExpandIcons = document.querySelectorAll('.tree-expand-icon');
            
            allChildrenContainers.forEach(container => {
                container.classList.add('expanded');
            });
            
            allExpandIcons.forEach(icon => {
                if (icon.textContent === '▶') {
                    icon.classList.add('expanded');
                    icon.textContent = '▼';
                }
            });
            
            showNotification('All nodes expanded', 'success');
        }
        
        function collapseAllNodes() {
            const allChildrenContainers = document.querySelectorAll('.tree-children');
            const allExpandIcons = document.querySelectorAll('.tree-expand-icon');
            
            allChildrenContainers.forEach(container => {
                container.classList.remove('expanded');
            });
            
            allExpandIcons.forEach(icon => {
                if (icon.textContent === '▼') {
                    icon.classList.remove('expanded');
                    icon.textContent = '▶';
                }
            });
            
            showNotification('All nodes collapsed', 'success');
        }
        
        function filterTopologyNodes() {
            const searchTerm = document.getElementById('topology-search').value.toLowerCase();
            const allNodes = document.querySelectorAll('.tree-node');
            
            // 清除之前的高亮
            allNodes.forEach(node => {
                node.querySelector('.tree-node-header').classList.remove('tree-node-highlight');
            });
            
            if (!searchTerm) {
                // 如果搜索框为空，显示所有节点
                allNodes.forEach(node => {
                    node.style.display = 'block';
                });
                return;
            }
            
            let matchCount = 0;
            allNodes.forEach(node => {
                const nodeLabel = node.querySelector('.tree-node-label').textContent.toLowerCase();
                const nodeType = node.getAttribute('data-type').toLowerCase();
                const nodeId = node.getAttribute('data-id').toLowerCase();
                
                if (nodeLabel.includes(searchTerm) || nodeType.includes(searchTerm) || nodeId.includes(searchTerm)) {
                    node.style.display = 'block';
                    node.querySelector('.tree-node-header').classList.add('tree-node-highlight');
                    
                    // 展开匹配节点的父节点
                    expandParentNodes(node);
                    matchCount++;
                } else {
                    node.style.display = 'none';
                }
            });
            
            if (matchCount > 0) {
                showNotification('Found ' + matchCount + ' matching nodes', 'info');
            } else {
                showNotification('No matching nodes found', 'warning');
            }
        }
        
        function expandParentNodes(node) {
            let currentNode = node;
            while (currentNode && currentNode.classList.contains('tree-node')) {
                const parentTreeChildren = currentNode.closest('.tree-children');
                if (parentTreeChildren) {
                    parentTreeChildren.classList.add('expanded');
                    const parentNodeId = parentTreeChildren.id.replace('children-', '');
                    const parentExpandIcon = document.querySelector('[data-id="' + parentNodeId + '"] .tree-expand-icon');
                    if (parentExpandIcon) {
                        parentExpandIcon.classList.add('expanded');
                        parentExpandIcon.textContent = '▼';
                    }
                    currentNode = parentTreeChildren.closest('.tree-node');
                } else {
                    break;
                }
            }
        }
        
        function resetTopologyView() {
            // 清除搜索
            document.getElementById('topology-search').value = '';
            
            // 显示所有节点
            const allNodes = document.querySelectorAll('.tree-node');
            allNodes.forEach(node => {
                node.style.display = 'block';
                node.querySelector('.tree-node-header').classList.remove('tree-node-highlight');
            });
            
            // 折叠所有节点
            collapseAllNodes();
            
            showNotification('Topology view reset', 'success');
        }

        function refreshStatistics() {
            // 重新初始化统计图表
            initializeStatistics();
            showNotification('Statistics refreshed', 'success');
        }
        
        
        function refreshProcesses() {
            displayProcesses();
            showNotification('Processes refreshed', 'success');
        }
        
        function refreshParticipants() {
            displayParticipants();
            showNotification('Participants refreshed', 'success');
        }
        
        function refreshWriters() {
            displayWriters();
            showNotification('Writers refreshed', 'success');
        }
        
        function refreshReaders() {
            displayReaders();
            showNotification('Readers refreshed', 'success');
        }
        
        function refreshTopics() {
            displayTopics();
            showNotification('Topics refreshed', 'success');
        }
        
        // Search and filter functions
        function filterParticipants() {
            const searchTerm = document.getElementById('participant-search').value.toLowerCase().trim();
            const participants = globalData.participantData.participants;
            
            if (!searchTerm) {
                displayParticipants();
                return;
            }
            
            const filteredParticipants = participants.filter(participant =>
                participant.processName.toLowerCase().includes(searchTerm) ||
                participant.participantGUID.toLowerCase().includes(searchTerm) ||
                participant.hostname.toLowerCase().includes(searchTerm) ||
                participant.processID.includes(searchTerm) ||
                participant.domainID.toString().includes(searchTerm)
            );
            
            displayFilteredParticipants(filteredParticipants);
        }
        
        function filterWriters() {
            const searchTerm = document.getElementById('writer-search').value.toLowerCase().trim();
            const writers = globalData.writerData.writers;
            
            if (!searchTerm) {
                displayWriters();
                return;
            }
            
            const filteredWriters = writers.filter(writer =>
                writer.topicName.toLowerCase().includes(searchTerm) ||
                writer.typeName.toLowerCase().includes(searchTerm) ||
                writer.writerGUID.toLowerCase().includes(searchTerm)
            );
            
            displayFilteredWriters(filteredWriters);
        }
        
        function filterReaders() {
            const searchTerm = document.getElementById('reader-search').value.toLowerCase().trim();
            const readers = globalData.readerData.readers;
            
            if (!searchTerm) {
                displayReaders();
                return;
            }
            
            const filteredReaders = readers.filter(reader =>
                reader.topicName.toLowerCase().includes(searchTerm) ||
                reader.typeName.toLowerCase().includes(searchTerm) ||
                reader.readerGUID.toLowerCase().includes(searchTerm)
            );
            
            displayFilteredReaders(filteredReaders);
        }
        
        // Detail view functions
        
        
        function showParentParticipant(participantGUID) {
            showTab('participants');
            setTimeout(() => {
                const searchInput = document.getElementById('participant-search');
                if (searchInput) {
                    searchInput.value = participantGUID.substring(0, 20);
                    filterParticipants();
                }
                showNotification('Navigated to parent participant', 'success');
            }, 500);
        }
        
        function highlightParticipantInTopology(participantGUID) {
            showTab('topology');
            showNotification(` + "`" + `Highlighted participant in topology: ${truncateText(participantGUID, 30)}` + "`" + `, 'success');
        }
        
        // Helper functions for filtered display
        function displayFilteredParticipants(participants) {
            // Similar to displayParticipants but with filtered data
            const container = document.getElementById('participant-list');
            
            if (!participants || participants.length === 0) {
                container.innerHTML = '<div class="text-center text-muted">No participants found matching search</div>';
                return;
            }
            
            let html = '';
            participants.forEach(participant => {
                const childrenCount = participant.children.length;
                const writersCount = participant.writers.length;
                const readersCount = participant.readers.length;
                
                html += ` + "`" + `
                    <div class="list-item participant-item" data-id="${participant.participantGUID}">
                        <div class="participant-header">
                            <h4>👥 ${participant.processName}</h4>
                            <div class="participant-badges">
                                <span class="badge badge-domain">Domain ${participant.domainID}</span>
                                <span class="badge badge-children">${childrenCount} entities</span>
                            </div>
                        </div>
                        
                        <div class="metadata">
                            <div><strong>GUID:</strong> ${truncateText(participant.participantGUID, 50)}</div>
                            <div><strong>GUID Prefix:</strong> ${participant.guidPrefix}</div>
                            <div><strong>Domain ID:</strong> ${participant.domainID}</div>
                            <div><strong>Process ID:</strong> ${participant.processID}</div>
                            <div><strong>Hostname:</strong> ${participant.hostname}</div>
                            <div><strong>Vendor:</strong> ${participant.vendorID}</div>
                            <div><strong>Protocol:</strong> ${participant.protocolVersion}</div>
                            <div><strong>Default Unicast:</strong> ${participant.defaultUnicastLoc}</div>
                            <div><strong>Default Multicast:</strong> ${participant.defaultMulticastLoc}</div>
                            <div><strong>Meta Unicast:</strong> ${participant.metaUnicastLoc}</div>
                            <div><strong>Meta Multicast:</strong> ${participant.metaMulticastLoc}</div>
                            <div><strong>Lease Duration:</strong> ${participant.leaseDuration}</div>
                            <div><strong>Builtin Endpoints:</strong> ${participant.builtinEndpoints}</div>
                            <div><strong>Expects Inline QoS:</strong> ${participant.expectsInlineQoS ? 'Yes' : 'No'}</div>
                            <div><strong>AutoCore Code:</strong> ${participant.autoCoreCode && participant.autoCoreCode.length > 0 ? 
                                (() => {
                                    const bytes = Array.from(atob(participant.autoCoreCode), c => c.charCodeAt(0));
                                    return '[' + bytes.join(' ') + '] (' + bytes.length + ' bytes)';
                                })() : 'Not available'}</div>
                            ${participant.userData ? '<div><strong>User Data:</strong> ' + (participant.userData.length > 50 ? participant.userData.substring(0, 50) + '...' : participant.userData) + '</div>' : ''}
                            ${participant.groupData ? '<div><strong>Group Data:</strong> ' + (participant.groupData.length > 50 ? participant.groupData.substring(0, 50) + '...' : participant.groupData) + '</div>' : ''}
                            ${participant.topicData ? '<div><strong>Topic Data:</strong> ' + (participant.topicData.length > 50 ? participant.topicData.substring(0, 50) + '...' : participant.topicData) + '</div>' : ''}
                            ${participant.manualLivelinessCount > 0 ? '<div><strong>Manual Liveliness Count:</strong> ' + participant.manualLivelinessCount + '</div>' : ''}
                            ${participant.staticDiscoveryData && participant.staticDiscoveryData.length > 0 ? '<div><strong>Static Discovery Data:</strong> ' + 
                                (() => {
                                    const bytes = Array.from(atob(participant.staticDiscoveryData), c => c.charCodeAt(0));
                                    const hexStr = bytes.map(b => b.toString(16).padStart(2, '0')).join(' ');
                                    return '[' + hexStr + '] (' + bytes.length + ' bytes)';
                                })() + '</div>' : ''}
                            ${participant.propertyList && participant.propertyList.length > 0 ? '<div><strong>Property List:</strong><br>' + participant.propertyList.map(prop => {
                                const cleanProp = prop.replace(/[\x00-\x1F\x7F-\x9F]/g, '').trim();
                                if (cleanProp.startsWith('ProcessName')) {
                                    return '<div style="margin-left: 20px;"><strong>Process Name:</strong> ' + cleanProp.replace('ProcessName', '') + '</div>';
                                } else if (cleanProp.startsWith('Pid')) {
                                    return '<div style="margin-left: 20px;"><strong>Pid:</strong> ' + cleanProp.replace('Pid', '') + '</div>';
                                } else if (cleanProp.startsWith('Hostname')) {
                                    return '<div style="margin-left: 20px;"><strong>Hostname:</strong> ' + cleanProp.replace('Hostname', '') + '</div>';
                                } else if (cleanProp.startsWith('SHMLocator')) {
                                    return '<div style="margin-left: 20px;"><strong>SHM Locator:</strong> ' + cleanProp.replace('SHMLocator', '') + '</div>';
                                } else if (cleanProp.startsWith('SHMSize')) {
                                    return '<div style="margin-left: 20px;"><strong>SHM Size:</strong> ' + cleanProp.replace('SHMSize', '') + '</div>';
                                } else if (cleanProp.startsWith('AutocoreCode')) {
                                    return '<div style="margin-left: 20px;"><strong>AutoCore Code:</strong> ' + cleanProp.replace('AutocoreCode', '') + '</div>';
                                } else if (cleanProp) {
                                    return '<div style="margin-left: 20px;"><strong>Property:</strong> ' + cleanProp + '</div>';
                                }
                                return '';
                            }).filter(item => item !== '').join('') + '</div>' : ''}
                        </div>
                        
                        <div class="children-section">
                            <strong>📝 Children Entities (${childrenCount}):</strong>
                            <div class="children-list">
                ` + "`" + `;
                
                if (childrenCount > 0) {
                    participant.children.forEach((child, index) => {
                        if (index < 5) { // Show first 5 children
                            const icon = child.type === 'writer' ? '✍️' : '👀';
                            html += ` + "`" + `<span class="child-tag ${child.type}">${icon} ${child.topicName}</span>` + "`" + `;
                        } else if (index === 5) {
                            html += ` + "`" + `<span class="child-tag more">+${childrenCount - 5} more</span>` + "`" + `;
                        }
                    });
                } else {
                    html += '<span class="text-muted">No children entities</span>';
                }
                
                html += ` + "`" + `
                            </div>
                        </div>
                        
                        <div class="participant-actions">
                            <button class="action-btn" onclick="highlightParticipantInTopology('${participant.participantGUID}')">
                                🔗 Show in Topology
                            </button>
                        </div>
                    </div>
                ` + "`" + `;
            });
            
            container.innerHTML = html;
        }
        
        function displayFilteredWriters(writers) {
            const container = document.getElementById('writer-list');
            
            if (!writers || writers.length === 0) {
                container.innerHTML = '<div class="text-center text-muted">No writers found matching search</div>';
                return;
            }
            
            let html = '';
            writers.forEach(writer => {
                html += ` + "`" + `
                    <div class="list-item writer-item" data-id="${writer.writerGUID}">
                        <div class="entity-header">
                            <h4>✍️ ${writer.topicName}</h4>
                            <div class="entity-badges">
                                <span class="badge badge-writer">Writer</span>
                                <span class="badge badge-type">${writer.typeName}</span>
                            </div>
                        </div>
                        
                        <div class="metadata">
                            <div><strong>Writer GUID:</strong> ${truncateText(writer.writerGUID, 50)}</div>
                            <div><strong>Topic Name:</strong> ${writer.topicName}</div>
                            <div><strong>Type Name:</strong> ${writer.typeName}</div>
                        </div>
                        ` + "`" + `;
                
                // Add QoS information directly in the table
                if (writer.qosProfile && Object.keys(writer.qosProfile).length > 0) {
                    html += ` + "`" + `
                        <div class="qos-section">
                            <h4>⚙️ Quality of Service (QoS) Parameters</h4>
                            <div class="metadata">
                    ` + "`" + `;
                    
                    for (const [key, value] of Object.entries(writer.qosProfile)) {
                        html += ` + "`" + `
                                <div><strong>${key}:</strong> ${value}</div>
                        ` + "`" + `;
                    }
                    
                    html += ` + "`" + `
                            </div>
                        </div>
                    ` + "`" + `;
                } else {
                    html += ` + "`" + `
                        <div class="qos-section">
                            <h4>⚙️ Quality of Service (QoS) Parameters</h4>
                            <div class="text-center text-muted">No QoS parameters available</div>
                        </div>
                    ` + "`" + `;
                }
                
                html += ` + "`" + `
                        <div class="entity-actions">
                            <button class="action-btn" onclick="showParentParticipant('${writer.participantGUID}')">
                                👥 Show Parent
                            </button>
                        </div>
                    </div>
                ` + "`" + `;
            });
            
            container.innerHTML = html;
        }
        
        function displayFilteredReaders(readers) {
            const container = document.getElementById('reader-list');
            
            if (!readers || readers.length === 0) {
                container.innerHTML = '<div class="text-center text-muted">No readers found matching search</div>';
                return;
            }
            
            let html = '';
            readers.forEach(reader => {
                html += ` + "`" + `
                    <div class="list-item reader-item" data-id="${reader.readerGUID}">
                        <div class="entity-header">
                            <h4>👀 ${reader.topicName}</h4>
                            <div class="entity-badges">
                                <span class="badge badge-reader">Reader</span>
                                <span class="badge badge-type">${reader.typeName}</span>
                            </div>
                        </div>
                        
                        <div class="metadata">
                            <div><strong>Reader GUID:</strong> ${truncateText(reader.readerGUID, 50)}</div>
                            <div><strong>Topic Name:</strong> ${reader.topicName}</div>
                            <div><strong>Type Name:</strong> ${reader.typeName}</div>
                        </div>
                        ` + "`" + `;
                
                // Add QoS information directly in the table
                if (reader.qosProfile && Object.keys(reader.qosProfile).length > 0) {
                    html += ` + "`" + `
                        <div class="qos-section">
                            <h4>⚙️ Quality of Service (QoS) Parameters</h4>
                            <div class="metadata">
                    ` + "`" + `;
                    
                    for (const [key, value] of Object.entries(reader.qosProfile)) {
                        html += ` + "`" + `
                                <div><strong>${key}:</strong> ${value}</div>
                        ` + "`" + `;
                    }
                    
                    html += ` + "`" + `
                            </div>
                        </div>
                    ` + "`" + `;
                } else {
                    html += ` + "`" + `
                        <div class="qos-section">
                            <h4>⚙️ Quality of Service (QoS) Parameters</h4>
                            <div class="text-center text-muted">No QoS parameters available</div>
                        </div>
                    ` + "`" + `;
                }
                
                html += ` + "`" + `
                        <div class="entity-actions">
                            <button class="action-btn" onclick="showParentParticipant('${reader.participantGUID}')">
                                👥 Show Parent
                            </button>
                        </div>
                    </div>
                ` + "`" + `;
            });
            
            container.innerHTML = html;
        }
        
        
        // Search functions
        function filterProcesses() {
            const searchTerm = document.getElementById('process-search').value.toLowerCase();
            const processes = globalData.processData.processes;
            
            if (!searchTerm) {
                displayProcesses();
                return;
            }
            
            const filteredProcesses = processes.filter(process =>
                process.processName.toLowerCase().includes(searchTerm) ||
                process.processID.includes(searchTerm) ||
                process.hostname.toLowerCase().includes(searchTerm)
            );
            
            const container = document.getElementById('process-list');
            if (filteredProcesses.length === 0) {
                container.innerHTML = '<div class="text-center text-muted">No processes found matching search</div>';
                return;
            }
            
            let html = '';
            filteredProcesses.forEach(process => {
                html += ` + "`" + `
                    <div class="list-item">
                        <h4>${process.processName} (PID: ${process.processID})</h4>
                        <div class="metadata">
                            <div><strong>GUID:</strong> ${process.processGUID}</div>
                            <div><strong>Hostname:</strong> ${process.hostname}</div>
                            <div><strong>Domain:</strong> ${process.domainID}</div>
                            <div><strong>Vendor:</strong> ${process.vendorID}</div>
                            <div><strong>Default Locator:</strong> ${process.defaultLocator}</div>
                            <div><strong>Meta Locator:</strong> ${process.metaLocator}</div>
                            <div><strong>Participants:</strong> ${process.participants.length}</div>
                        </div>
                    </div>
                ` + "`" + `;
            });
            
            container.innerHTML = html;
        }
        
        function filterTopics() {
            const searchTerm = document.getElementById('topic-search').value.toLowerCase();
            const topics = globalData.topicData.topics;
            
            if (!searchTerm) {
                displayTopics();
                return;
            }
            
            const filteredTopics = topics.filter(topic =>
                topic.topicName.toLowerCase().includes(searchTerm) ||
                topic.topicType.toLowerCase().includes(searchTerm)
            );
            
            const container = document.getElementById('topic-list');
            if (filteredTopics.length === 0) {
                container.innerHTML = '<div class="text-center text-muted">No topics found matching search</div>';
                return;
            }
            
            let html = '';
            filteredTopics.forEach(topic => {
                html += ` + "`" + `
                    <div class="list-item">
                        <h4>${topic.topicName}</h4>
                        <div class="metadata">
                            <div><strong>Type:</strong> ${topic.topicType}</div>
                            <div><strong>Publishers:</strong> ${topic.publishers.length}</div>
                            <div><strong>Subscribers:</strong> ${topic.subscribers.length}</div>
                        </div>
                        <div class="participants">
                            <strong>Publishers:</strong> ${topic.publishers.slice(0, 3).join(', ')}${topic.publishers.length > 3 ? '...' : ''}
                            <br>
                            <strong>Subscribers:</strong> ${topic.subscribers.slice(0, 3).join(', ')}${topic.subscribers.length > 3 ? '...' : ''}
                        </div>
                    </div>
                ` + "`" + `;
            });
            
            container.innerHTML = html;
        }
        
        // Utility functions
        function showNotification(message, type = 'info') {
            const notification = document.createElement('div');
            notification.className = ` + "`" + `notification ${type}` + "`" + `;
            notification.textContent = message;
            
            document.body.appendChild(notification);
            
            setTimeout(() => {
                notification.remove();
            }, 3000);
        }
        
        function formatNumber(num) {
            return num.toLocaleString();
        }
        
        function truncateText(text, maxLength) {
            return text.length > maxLength ? text.substring(0, maxLength) + '...' : text;
        }
        
        function handleError(error, context) {
            console.error(` + "`" + `Error ${context}:` + "`" + `, error);
            showNotification(` + "`" + `Error ${context}: ${error.message}` + "`" + `, 'error');
        }
        
        
        // Modal functions
        function closeQoSModal() {
            document.getElementById('qos-modal').style.display = 'none';
        }
        
        // Close modal when clicking outside of it
        window.onclick = function(event) {
            const modal = document.getElementById('qos-modal');
            if (event.target === modal) {
                closeQoSModal();
            }
        }
        
        // Close modal with Escape key
        document.addEventListener('keydown', function(event) {
            if (event.key === 'Escape') {
                closeQoSModal();
            }
        });
    `, jsonData)
}