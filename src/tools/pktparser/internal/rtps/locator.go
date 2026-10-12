package rtps

import (
	"encoding/binary"
	"fmt"
	"io"
	"net"
)

const (
	LOCATOR_KIND_INVALID  = -1
	LOCATOR_KIND_RESERVED = 0
	LOCATOR_KIND_UDPV4    = 1
	LOCATOR_KIND_UDPV6    = 2
	LOCATOR_KIND_TCPv4    = 4
	LOCATOR_KIND_TCPv6    = 8
	LOCATOR_PORT_INVALID  = 0
)

type locator struct {
	kind int32
	port uint32
	addr net.IP
}

func (loca locator) Compare(locb locator) bool {
	if loca.addr.String() == locb.addr.String() {
		return loca.port < locb.port
	}
	return loca.addr.String() < locb.addr.String()
}

func (loc locator) String() string {
	return fmt.Sprintf("locator{addr:%s, port:%d, kind:%d}", loc.addr.String(), loc.port, loc.kind)
}

func newUDPv4Loc(ip net.IP, port uint16) *locator {
	return &locator{
		kind: LOCATOR_KIND_UDPV4,
		port: uint32(port),
		addr: ip,
	}
}

func newUDPv4LocFromBytes(bin binary.ByteOrder, b []byte) (locator, error) {
	if len(b) < 4+4+16 {
		return locator{}, io.EOF
	}
	return locator{
		kind: int32(bin.Uint32(b[0:])),
		port: bin.Uint32(b[4:]),
		addr: net.IPv4(b[20], b[21], b[22], b[23]), // xxx: ipv6 support
	}, nil
}

func (loc *locator) Bytes() []byte {
	buf := make([]byte, 8+len(loc.addr))
	binary.LittleEndian.PutUint32(buf, uint32(loc.kind))
	binary.LittleEndian.PutUint32(buf[4:], loc.port)
	// XXX: net.IP instances keep non-zero bytes in the ipv6 portion of the address
	//      even for ipv4 addresses, so just copy out the last 4 bytes
	//      be smarter to support ipv6
	copy(buf[8+12:], loc.addr[12:])
	return buf
}

func (loc *locator) addrStr() string {
	return fmt.Sprintf("%s:%d", loc.addr.String(), loc.port)
}

func CopyLocator(loc locator) locator {
	l := locator{
		kind: loc.kind,
		port: loc.port,
	}
	l.addr = make(net.IP, len(loc.addr))
	copy(l.addr, loc.addr)
	return l
}
