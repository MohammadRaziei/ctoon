package binding

// Pure-Go binary "tape" format used to exchange a whole value tree with
// bridge.c in a single CGo call, instead of one call per tree node. See
// bridge.h for the full format description and rationale — the tags below
// MUST match bridge.c's TAPE_* enum exactly.
//
// Nothing in this file ever crosses the CGo boundary itself: encodeTape
// and tapeReader.decode are plain Go, safe to call as often as needed
// without any FFI cost.

import (
	"fmt"
	"math"
)

const (
	tapeNull  = 0
	tapeFalse = 1
	tapeTrue  = 2
	tapeSint  = 3
	tapeUint  = 4
	tapeReal  = 5
	tapeStr   = 6
	tapeArr   = 7
	tapeObj   = 8
)

func appendU32(buf []byte, v uint32) []byte {
	return append(buf, byte(v), byte(v>>8), byte(v>>16), byte(v>>24))
}

func appendU64(buf []byte, v uint64) []byte {
	return append(buf,
		byte(v), byte(v>>8), byte(v>>16), byte(v>>24),
		byte(v>>32), byte(v>>40), byte(v>>48), byte(v>>56))
}

func appendRawStr(buf []byte, s string) []byte {
	buf = appendU32(buf, uint32(len(s)))
	return append(buf, s...)
}

// encodeTape serialises v into the tape format, appending to buf (pass nil
// to start a new buffer). No CGo calls are made here — this is the whole
// point: the entire tree is built natively in Go, and only the finished
// byte slice crosses into C, once, via ctoon_go_bridge_encode.
func encodeTape(buf []byte, v interface{}) ([]byte, error) {
	switch t := v.(type) {
	case nil:
		return append(buf, tapeNull), nil

	case bool:
		if t {
			return append(buf, tapeTrue), nil
		}
		return append(buf, tapeFalse), nil

	case int:
		return appendU64(append(buf, tapeSint), uint64(int64(t))), nil
	case int8:
		return appendU64(append(buf, tapeSint), uint64(int64(t))), nil
	case int16:
		return appendU64(append(buf, tapeSint), uint64(int64(t))), nil
	case int32:
		return appendU64(append(buf, tapeSint), uint64(int64(t))), nil
	case int64:
		return appendU64(append(buf, tapeSint), uint64(t)), nil

	case uint:
		return appendU64(append(buf, tapeUint), uint64(t)), nil
	case uint8:
		return appendU64(append(buf, tapeUint), uint64(t)), nil
	case uint16:
		return appendU64(append(buf, tapeUint), uint64(t)), nil
	case uint32:
		return appendU64(append(buf, tapeUint), uint64(t)), nil
	case uint64:
		return appendU64(append(buf, tapeUint), t), nil

	case float32:
		return appendU64(append(buf, tapeReal), math.Float64bits(float64(t))), nil
	case float64:
		return appendU64(append(buf, tapeReal), math.Float64bits(t)), nil

	case string:
		return appendRawStr(append(buf, tapeStr), t), nil

	case []interface{}:
		buf = append(buf, tapeArr)
		buf = appendU32(buf, uint32(len(t)))
		var err error
		for _, elem := range t {
			buf, err = encodeTape(buf, elem)
			if err != nil {
				return nil, err
			}
		}
		return buf, nil

	case map[string]interface{}:
		buf = append(buf, tapeObj)
		buf = appendU32(buf, uint32(len(t)))
		var err error
		for k, elem := range t {
			buf = appendRawStr(buf, k) // no tag byte: always a string here
			buf, err = encodeTape(buf, elem)
			if err != nil {
				return nil, err
			}
		}
		return buf, nil

	default:
		return nil, fmt.Errorf("ctoon: unsupported Go type %T", v)
	}
}

// tapeReader decodes a tape buffer back into Go values. Also pure Go, no
// CGo calls.
type tapeReader struct {
	buf []byte
	pos int
}

func (r *tapeReader) readU8() (byte, error) {
	if r.pos+1 > len(r.buf) {
		return 0, fmt.Errorf("ctoon: tape underflow reading tag")
	}
	v := r.buf[r.pos]
	r.pos++
	return v, nil
}

func (r *tapeReader) readU32() (uint32, error) {
	if r.pos+4 > len(r.buf) {
		return 0, fmt.Errorf("ctoon: tape underflow reading length")
	}
	b := r.buf[r.pos : r.pos+4]
	r.pos += 4
	return uint32(b[0]) | uint32(b[1])<<8 | uint32(b[2])<<16 | uint32(b[3])<<24, nil
}

func (r *tapeReader) readU64() (uint64, error) {
	if r.pos+8 > len(r.buf) {
		return 0, fmt.Errorf("ctoon: tape underflow reading value")
	}
	b := r.buf[r.pos : r.pos+8]
	r.pos += 8
	return uint64(b[0]) | uint64(b[1])<<8 | uint64(b[2])<<16 | uint64(b[3])<<24 |
		uint64(b[4])<<32 | uint64(b[5])<<40 | uint64(b[6])<<48 | uint64(b[7])<<56, nil
}

func (r *tapeReader) readStr(n uint32) (string, error) {
	if r.pos+int(n) > len(r.buf) {
		return "", fmt.Errorf("ctoon: tape underflow reading string")
	}
	s := string(r.buf[r.pos : r.pos+int(n)])
	r.pos += int(n)
	return s, nil
}

func (r *tapeReader) decode() (interface{}, error) {
	tag, err := r.readU8()
	if err != nil {
		return nil, err
	}
	switch tag {
	case tapeNull:
		return nil, nil
	case tapeFalse:
		return false, nil
	case tapeTrue:
		return true, nil
	case tapeSint:
		v, err := r.readU64()
		if err != nil {
			return nil, err
		}
		return int64(v), nil
	case tapeUint:
		return r.readU64()
	case tapeReal:
		v, err := r.readU64()
		if err != nil {
			return nil, err
		}
		return math.Float64frombits(v), nil
	case tapeStr:
		n, err := r.readU32()
		if err != nil {
			return nil, err
		}
		return r.readStr(n)
	case tapeArr:
		n, err := r.readU32()
		if err != nil {
			return nil, err
		}
		out := make([]interface{}, n)
		for i := range out {
			out[i], err = r.decode()
			if err != nil {
				return nil, err
			}
		}
		return out, nil
	case tapeObj:
		n, err := r.readU32()
		if err != nil {
			return nil, err
		}
		out := make(map[string]interface{}, n)
		for i := uint32(0); i < n; i++ {
			klen, err := r.readU32()
			if err != nil {
				return nil, err
			}
			key, err := r.readStr(klen)
			if err != nil {
				return nil, err
			}
			out[key], err = r.decode()
			if err != nil {
				return nil, err
			}
		}
		return out, nil
	default:
		return nil, fmt.Errorf("ctoon: corrupt tape (unknown tag %d) — internal error, please report", tag)
	}
}
