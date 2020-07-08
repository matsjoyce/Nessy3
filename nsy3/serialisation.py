import enum
import struct


class Type(enum.Enum):
    int, float, string, dict, set, list, bytes, true, false, none = range(10)


def serialise(obj):
    if isinstance(obj, int):
        return struct.pack("<Bi", Type.int.value, obj)
    elif isinstance(obj, float):
        return struct.pack("<Bd", Type.float.value, obj)
    elif isinstance(obj, str):
        return struct.pack("<BI", Type.string.value, len(obj)) + obj.encode()
    elif isinstance(obj, dict):
        return struct.pack("<BI", Type.dict.value, len(obj)) + b"".join(serialise(k) + serialise(v) for k, v in obj.items())
    elif isinstance(obj, set):
        return struct.pack("<BI", Type.set.value, len(obj)) + b"".join(serialise(x) for x in obj)
    elif isinstance(obj, list):
        return struct.pack("<BI", Type.list.value, len(obj)) + b"".join(serialise(x) for x in obj)
    elif isinstance(obj, bytes):
        return struct.pack("<BI", Type.bytes.value, len(obj)) + obj
    elif obj is True:
        return struct.pack("<B", Type.true.value)
    elif obj is False:
        return struct.pack("<B", Type.false.value)
    elif obj is None:
        return struct.pack("<B", Type.none.value)
    else:
        raise RuntimeError(f"Can't serialise {type(obj)}")


def deserialise(bytes, pos=0):
    type = Type(bytes[pos])
    if type is Type.int:
        return struct.unpack_from("<i", bytes, pos + 1)[0], pos + 5
    elif type is Type.float:
        return struct.unpack_from("<d", bytes, pos + 1)[0], pos + 9
    elif type is Type.string:
        len, = struct.unpack_from("<I", bytes, pos + 1)
        return bytes[pos + 5:pos + 5 + len].decode(), pos + 5 + len
    elif type is Type.dict:
        len, = struct.unpack_from("<I", bytes, pos + 1)
        obj, pos = {}, pos + 5
        for _ in range(len):
            k, pos = deserialise(bytes, pos)
            v, pos = deserialise(bytes, pos)
            obj[k] = v
        return obj, pos
    elif type is Type.set:
        len, = struct.unpack_from("<I", bytes, pos + 1)
        obj, pos = set(), pos + 5
        for _ in range(len):
            x, pos = deserialise(bytes, pos)
            obj.add(x)
        return obj, pos
    elif type is Type.list:
        len, = struct.unpack_from("<I", bytes, pos + 1)
        obj, pos = [], pos + 5
        for _ in range(len):
            x, pos = deserialise(bytes, pos)
            obj.append(x)
        return obj, pos
    elif type is Type.bytes:
        len, = struct.unpack_from("<I", bytes, pos + 1)
        return bytes[pos + 5:pos + 5 + len], pos + 5 + len
    elif type is Type.true:
        return True, pos + 1
    elif type is Type.false:
        return False, pos + 1
    elif type is Type.none:
        return None, pos + 1
