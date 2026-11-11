package rtps

import (
	"encoding/binary"
	"fmt"
	"net"
	"os"
)

const (
	// from the spec
	FRUDP_PORT_PB = 7400
	FRUDP_PORT_DG = 250
	FRUDP_PORT_PG = 2
	FRUDP_PORT_D0 = 0
	FRUDP_PORT_D1 = 10
	FRUDP_PORT_D2 = 1
	FRUDP_PORT_D3 = 11
)

var (
	DEFAULT_MCAST_GROUP_IP = net.IPv4(239, 255, 0, 1)
)

type udpConfig struct {
	guidPrefix    GUIDPrefix // does this belong here?
	participantID int
	domainID      uint32
	unicastAddr   net.IP
	iface         *net.Interface
	rxers         []udpCtx
}

type udpCtx struct {
	conn *net.UDPConn
	addr *net.UDPAddr
}

var (
	defaultUDPConfig = udpConfig{
		domainID:   0,
		guidPrefix: newGUIDPrefix(),
	}
)

func (uc *udpConfig) mcastBuiltinPort() uint16 {
	return uint16(FRUDP_PORT_PB + FRUDP_PORT_DG*uc.domainID + FRUDP_PORT_D0)
}

func (uc *udpConfig) ucastBuiltinPort() uint16 {
	return uint16(FRUDP_PORT_PB + FRUDP_PORT_DG*uc.domainID +
		FRUDP_PORT_D1 + FRUDP_PORT_PG*uint32(uc.participantID))
}

func (uc *udpConfig) mcastUserPort() uint16 {
	return uint16(FRUDP_PORT_PB + FRUDP_PORT_DG*uc.domainID + FRUDP_PORT_D2)
}

func (uc *udpConfig) ucastUserPort() uint16 {
	return uint16(FRUDP_PORT_PB + FRUDP_PORT_DG*uc.domainID +
		FRUDP_PORT_D3 + FRUDP_PORT_PG*uint32(uc.participantID))
}

func (uc *udpConfig) ip() net.IP {
	return uc.unicastAddr
}

func udpInit() error {

	iface, err := defaultInterface()
	if err != nil {
		return err
	}

	ip, err := defaultIP(iface)
	if err != nil {
		return err
	}

	println("found interface:", iface.Name, "MTU:", iface.MTU, "ip:", ip.String())
	defaultUDPConfig.iface = iface
	defaultUDPConfig.unicastAddr = ip

	// not sure about endianness here.
	defaultUDPConfig.guidPrefix[0] = MY_RTPS_VENDOR_ID >> 8
	defaultUDPConfig.guidPrefix[1] = MY_RTPS_VENDOR_ID & 0xff

	// MAC address
	copy(defaultUDPConfig.guidPrefix[2:8], iface.HardwareAddr)

	// 4 bytes left. let's use the process ID
	pid := os.Getpid()
	binary.BigEndian.PutUint32(defaultUDPConfig.guidPrefix[8:], uint32(pid))

	udpCreateParticipant()
	udpAddMcastRX(fmt.Sprintf("%s:%d", DEFAULT_MCAST_GROUP_IP.String(), defaultUDPConfig.mcastBuiltinPort()))
	udpAddMcastRX(fmt.Sprintf("%s:%d", DEFAULT_MCAST_GROUP_IP.String(), defaultUDPConfig.mcastUserPort()))
	udpAddUcastRX(fmt.Sprintf("%s:%d", defaultUDPConfig.unicastAddr.String(), defaultUDPConfig.ucastBuiltinPort()))
	udpAddUcastRX(fmt.Sprintf("%s:%d", defaultUDPConfig.unicastAddr.String(), defaultUDPConfig.ucastUserPort()))

	return nil
}

func defaultInterface() (*net.Interface, error) {
	ifaces, err := net.Interfaces()
	if err != nil {
		return nil, err
	}

	// XXX: probably want to return all valid interfaces
	// and determine how to select one
	mask := net.FlagUp | net.FlagBroadcast | net.FlagMulticast
	for _, ifi := range ifaces {
		if ifi.Flags&mask == mask {
			return &ifi, nil
		}
	}

	return nil, fmt.Errorf("couldn't find a valid interface :(")
}

func defaultIP(iface *net.Interface) (net.IP, error) {
	addrs, err := iface.Addrs()
	if err != nil {
		return nil, err
	}
	for _, addr := range addrs {
		if ifa, ok := addr.(*net.IPNet); ok {
			if ifa.IP.To4() != nil {
				return ifa.IP, nil
			}
		}
	}
	return nil, fmt.Errorf("couldn't find a valid address :(")
}

func udpCreateParticipant() bool {
	// FREERTPS_INFO("frudp_part_create() on domain_id %d\r\n", defaultUDPConfig.domainID)
	//g_frudp_config.domain_id = domain_id;
	if !udpInitParticipantID() {
		// printf("unable to initialize participant ID\r\n")
		return false
	}
	// g_frudp_participant_init_complete = true
	return true
}

func udpInitParticipantID() bool {
	// scan ports on our unicast address to find a free one
	// xxx: better way to determine if ports are open?

	for pid := 0; pid < 100; pid++ { // todo: hard upper bound is bad
		// see if we can open the port; if so, let's say we have a unique PID
		defaultUDPConfig.participantID = pid
		port := defaultUDPConfig.ucastBuiltinPort()
		if udpAddUcastRX(fmt.Sprintf("%s:%d", defaultUDPConfig.unicastAddr.String(), port)) == nil {
			return true
		}
	}
	return false // couldn't find an available PID
}

func udpAddUcastRX(addr string) error {
	udpaddr, err := net.ResolveUDPAddr("udp", addr)
	if err != nil {
		return err
	}

	// have we already added this ctx?
	for _, c := range defaultUDPConfig.rxers {
		if c.addr == udpaddr {
			return nil
		}
	}

	udpconn, err := net.ListenUDP("udp", udpaddr)
	if err != nil {
		return err
	}

	println("adding ucast:", addr)

	rxer := udpCtx{udpconn, udpaddr}
	defaultUDPConfig.rxers = append(defaultUDPConfig.rxers, rxer)

	go rxer.rx()
	return nil
}

func udpAddMcastRX(addr string) error {
	udpaddr, err := net.ResolveUDPAddr("udp", addr)
	if err != nil {
		return err
	}

	// have we already added this ctx?
	for _, c := range defaultUDPConfig.rxers {
		if c.addr == udpaddr {
			return nil
		}
	}

	// XXX: ability to specify interfaces
	// XXX: may need more sophisticated handling for multicast
	udpconn, err := net.ListenMulticastUDP("udp", nil, udpaddr)
	if err != nil {
		return err
	}

	println("adding mcast:", addr)

	rxer := udpCtx{udpconn, udpaddr}
	defaultUDPConfig.rxers = append(defaultUDPConfig.rxers, rxer)

	go rxer.rx()
	return nil
}

func (u *udpCtx) rx() {
	for {
		// would prefer to limit packet size to MTU to avoid losing packets
		// that lose a single fragment, but at least OpenSplice sends packets
		// larger than standard MTU, so use a large buffer for now
		buf := make([]byte, 4096)
		n, _, err := u.conn.ReadFromUDP(buf)
		if err != nil {
			println("ReadFromUDP failed:", err)
			continue
		}
		rxdispatch(buf[:n])
	}
}

// clean me

func udpTXAddr(data []byte, ip string, port uint16) error {
	return udpTXStr(data, fmt.Sprintf("%s:%d", ip, port))
}

func udpTXStr(data []byte, addr string) error {
	dest, err := net.ResolveUDPAddr("udp", addr)
	if err != nil {
		return err
	}
	return udpTX(data, dest)
}

func udpTX(data []byte, dest *net.UDPAddr) error {
	for _, rxer := range defaultUDPConfig.rxers {
		_, err := rxer.conn.WriteToUDP(data, dest)
		return err
	}
	return fmt.Errorf("no live sockets")
}
