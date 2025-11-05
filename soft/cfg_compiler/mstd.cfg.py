import re
import tomllib

from dataclasses import dataclass, field
from typing import *
from struct import unpack

@dataclass
class EnumField:
    name: str
    value: int
    comment: str = ''

    def __str__(self):
        return f'{self.name} = 0x{self.value:X}, // {self.comment}'

@dataclass
class EnumDef:
    name: str
    base_type: str
    body: list[EnumField]
    toc: dict[str, int] = field(init=False)
    masks: dict[str, int] = field(init=False)
    short_toc: dict[str, str] = field(init=False)

    def __str__(self):
        return f'enum class {self.name} : {self.base_type} {{\n' + ''.join(f'  {x}\n' for x in self.body) + '};\n'

    def post_init(self):
        self.masks = {}
        self.toc = {}
        self.short_toc = {}
        for item in self.body:
            if item.name.endswith('_MASK'):
                self.masks[item.name.removesuffix('_MASK')] = item.value
            else:
                self.toc[item.name] = item.value
                pref, dlm, rest = item.name.partition('_')
                if dlm:
                    if rest in self.short_toc:
                        self.short_toc[rest] = None
                    else:
                        self.short_toc[rest] = item.name

    def int2str(self, val: int) -> str:
        result = []
        processed = 0
        for name, value in self.toc.items():
            if value & val:
                pref, _, short = name.partition('_')
                if pref in self.masks:
                    if (val & self.masks[pref]) != value:
                        continue
                processed |= value
                if self.short_toc.get(short, None):
                    result.append(short)
                else:
                    result.append(f'{pref}.{short}')
        val &= ~processed
        if val:
            result.append(f'#0x{val:X}')
        return '|'.join(result)

    def str2int(self, val: str) -> int:
        result = 0
        for item in re.split(r'[\s,|]+', val):
            if re.match(r'_|::|.', item):
                item = re.sub(r'_|::|\.', '_', item)
                assert item in self.toc, f'Enum item  "{item}" not valid for Enum "{self.name}" (valid values are {list(self.toc.keys())})'
                result |= self.toc[item]
            elif item in self.short_toc:
                v = self.short_toc[item]
                assert v, f'Enum item  "{item}" is ambigous in Enum "{self.name}" (enum items are {list(self.toc.keys())})'
                result |= self.toc[v]
            else:
                assert False, f'Short Enum item "{item}" is unknown for Enum "{self.name}" (valid values are {list(self.toc.keys())})'
        return result            

@dataclass
class CfgField:
    name: str
    size: int
    shift: int
    val_default: Optional[int]
    val_type: str
    enum_ref: Optional[EnumDef] = None
    comment: str = ''

    def __str__(self):
        return f'{self.val_type} {self.name} = {self.val_default}; // size={self.size}, shift={self.shift} {self.comment}'

    @property
    def tp(self) -> str:
        if self.enum_ref:
            return self.enum_ref.base_type
        else:
            return  self.val_type

    @property
    def is_signed(self) -> bool:
        return re.match(r'int\d+_t$', self.tp)

    @property
    def is_filler(self) -> bool:
        return self.val_type == 'dummy'

BASE_SIZES = {
    'int8_t' : 1, 'uint8_t':  1,
    'int16_t': 2, 'uint16_t': 2,
    'int32_t': 4, 'uint32_t': 4,
    'int64_t': 8, 'uint64_t': 8
}

class Config:
    def __init__(self, fname: str):
        self.version = 0
        self.lc_version = 0
        self.max_cfg_size = 4096
        self.lnum = 0
        self.cfg_struct : list[CfgField] = []
        self.enums : dict[str, EnumDef] = {}
        self.last_enum : Optional[EnumDef] = None
        self.size = 0

        in_struct = False

        with open(fname, "rt") as f:
            for line in f:
                self.lnum += 1
                line = line.strip()
                if in_struct and line == '};':
                    break
                if self.last_enum and line == '};':
                    self.last_enum.post_init()
                    self.last_enum = None
                    continue
                if in_struct:
                    self.process_struct_line(line)
                elif self.last_enum:
                    self.process_enum_line(line)
                elif mtch := re.search(r'MAX_CFG_SIZE\s*=\s*(\d+)\s*;', line):
                    self.max_cfg_size = int(mtch.group(1))
                elif mtch := re.search(r'\bConfigVersion\s*=\s*(\d+)\s*;', line):
                    self.version = int(mtch.group(1))
                elif mtch := re.search(r'\bLC_ConfigVersion\s*=\s*(\d+)\s*;', line):
                    self.lc_version = int(mtch.group(1))
                elif mtch := re.match(r'struct\s+Config_V(\d+)\s*{', line):
                    v = int(mtch.group(1))
                    assert v == self.version, f'Version of Config ({v}) not equal of version from header (ConfigVersion={self.version})'
                    in_struct = True
                elif mtch := re.match(r'enum\s+([\w\d]+)\s*:\s*([\w\d]+)\s*{', line):
                    self.last_enum = EnumDef(*mtch.groups(), [])
                    self.enums[self.last_enum.name] = self.last_enum
            assert self.lc_version, 'LC_ConfigVersion not specified (or zero) in config'
            assert self.version, 'ConfigVersion not specified (or zero) in config'
            assert self.lc_version <= self.version, f'LC_ConfigVersion ({self.lc_version}) is greater than ConfigVersion ({self.version})'
            assert self.size % 4 == 0, f'Config size (self.size) is not aligned to 4'

    def __str__(self):
        lines = [
            f'MAX_CFG_SIZE = {self.max_cfg_size}\n',
            f'LC_Version = {self.lc_version}\n',
            f'Cfg Size = {self.size} ({self.size//4}*4)\n'
        ]
        for e in self.enums.values():
            lines.append(str(e))
        lines.append(f'struct Config_V{self.version} {{\n')
        for l in self.cfg_struct:
            lines.append(f'  {l}\n')
        lines.append('}\n')
        return ''.join(lines)

    def process_struct_line(self, line: str):
        # uint32_t crc;   // crc from next field to end of config record
        # uint8_t oled_contrast = 0xCF;
        # Options1 options1 = WFOP_Auto;
        line, _, comment = line.partition('//')
        comment = comment.strip()
        if mtch := re.match(r'^([\d\w]+)\s+([\d\w]+)\s*(=.*)?;', line):
            val_type, val_id, val_def = mtch.groups()
            enum_ref = None
            if val_type in BASE_SIZES:
                size = BASE_SIZES[val_type]
            else:
                assert val_type in self.enums, f'Value type {val_type} is not integer and not any Enum (enums: {list(self.enums.keys())})'
                enum_ref = self.enums[val_type]
                size = BASE_SIZES[enum_ref.base_type]
            assert self.size % size == 0, f'Field alignment missmatch. Field "{line}", align/size = {size}, shift = {self.size}'
            if val_id.startswith('reserved'):
                self.cfg_struct.append(CfgField(val_id, size, self.size, 0, 'dummy'))
            else:
                if val_def and val_def.startswith('='):
                    val_def = val_def[1:].strip()
                    if enum_ref:
                        val_def = enum_ref.str2int(val_def)
                    else:
                        val_def = int(val_def, 0)
                self.cfg_struct.append(CfgField(val_id, size, self.size, val_def, val_type, enum_ref, comment))
            self.size += size
        # char ssid[33] = {0};
        elif mtch := re.match(r'([\d\w]+)\s+([\d\w]+)\s*\[\s*(\d+)\s*\]', line):
            val_type, val_id, size = mtch.groups()
            assert val_type == 'char', f'Only strings supported: ({line})'
            size = int(size)
            self.cfg_struct.append(CfgField(val_id, size, self.size, None, val_type, None, comment))
            self.size += size;

    def process_enum_line(self, line: str):
        line, _, comment = line.partition('//')
        comment = comment.strip()
        # WFOP_No   = 0x00,  // WiFi turned off
        if mtch := re.match(r'([\d\w]+)\s*=\s*((0x)?[0-9a-fA-F]+)', line):
            val_id, val_val, _ = mtch.groups()
            assert '_' in val_id, f'Enum item name should include "_" ({val_id})'
            self.last_enum.body.append(EnumField(val_id, int(val_val,0), comment))

@dataclass
class DataSlot:
    value: Optional[bytes|int]
    fld: CfgField

class ConfigData:
    def __init__(self, cfg: Config):
        self.cfg = cfg
        self.data = {}
        for fld in cfg.cfg_struct:
            self.data[fld.name] = DataSlot(fld.val_default, fld)

    def has_field(self, name: str) -> bool:
        return name in self.data

    def get_toml_value(self, name: str) -> Optional[str|int]:
        fld = self.data[name]
        result = fld.value
        fld = fld.fld
        if isinstance(result, bytes):
            result = result.decode(errors='replace')
        elif isinstance(result, int) and fld.enum_ref:
            result = fld.enum_ref.int2str(result)
        elif result is None:
            return '' if fld.val_type == 'char' else 0
        return result

    def get_binary_value(self, name: str) -> bytes:
        fld = self.data[name]
        result = fld.value
        fld = fld.fld
        if fld.val_type != 'char':  # Not a string - integer
            return (result or 0).to_bytes(fld.size, byteorder='little', signed=fld.is_signed)
        if result is None:
            result = b''
        assert len(result) <= fld.size
        return result.ljust(fld.size, b'\0')

    def get_full_binary(self) -> bytes:
        result = []
        for fld in self.cfg.cfg_struct:
            if fld.is_filler:
                result.append(fld.size * b'\0')
            else:
                result.append(self.get_binary_value(fld.name))
        return b''.join(result)

    def patch_binary_image(self, set_crc: bool = True):
        self.set_toml_value('version', self.cfg.version)
        bin_img = self.get_full_binary()
        assert len(bin_img) % 4 == 0
        size = len(bin_img)//4-1
        self.set_toml_value('size', size)
        if set_crc:
            crc = eval_crc(bin_img[4:])
            self.set_toml_value('crc', crc)

    def get_full_toml(self, with_hidden_fields: bool = False) -> str:
        result = []
        for fld in self.cfg.cfg_struct:
            if not fld.is_filler and (with_hidden_fields or fld.name not in ('crc', 'size', 'version')):
                if fld.comment:
                    result.append(f'# {fld.comment}\n')
                result.append(f'{fld.name} = {toml_repr(self.get_toml_value(fld.name))}\n')
        return ''.join(result)

    def set_toml_value(self, name: str, val: int|str):
        fld = self.data[name]
        if fld.fld.enum_ref:
            # We are enum. Only symbolic one supported
            assert isinstance(val, str), f'Field "{name}" is a enum. String expected, but found {val}'
            val = fld.fld.enum_ref.str2int(val)
        if fld.fld.val_type == 'char':
            # This is a string
            assert isinstance(val, str), f'Field "{name}" is a string, but found {val}'
            valb = val.encode()
            if len(valb) > fld.fld.size:
                print(f'WARNING: Field "{name}" overflow. Field size is {fld.fld.size}, but value ({val}) length is {len(valb)}. Truncated')
                valb = valb[:fld.fld.size-1]
            elif len(valb) == fld.fld.size:
                print(f'WARNING: Field "{name}" overflow - no place for terminated Zero.')
        else:
            valb = val.to_bytes(fld.fld.size, byteorder='little', signed=fld.fld.is_signed) # Will rize OverflowError if integer can't be represented in given field size
        fld.value = valb

    def set_binary_value(self, name: str, val: bytes):
        fld = self.data[name]
        if fld.fld.val_type == 'char':
            fld.value = val
        else:
            fld.value = val.from_bytes(byteorder='little', signed=fld.fld.is_signed)

    def set_full_toml(self, toml: dict[str, int|str], allow_unknown: bool):
        for key, val in toml.items():
            if key not in self.data:
                if allow_unknown:
                    print(f'WARNING: Unknown field "{key}", ignored')
                else:
                    assert False, f'Unknown field "{key}"'
            else:
                self.set_toml_value(key, val)

    def set_full_binary(self, val: bytes, allow_errors: bool):
        for fld in self.cfg.cfg_struct:
            if fld.is_filler:
                zval = val[:fld.size]
            else:
                self.set_binary_value(fld.name, val[:fld.size])
            val = val[fld.size:]

def toml_repr(data: int|str) -> str:
    if isinstance(data, int):
        return str(data)
    if "'" not in data and '\n' not in data:
        return f"'{data}'"
    if "'''" not in data:
        return f"'''{data}'''"
    ENC = {
        '\b': r'\b',
        '\t': r'\t',
        '\n': r'\n',
        '\f': r'\f',
        '\r': r'\r',
        '"':  r'\"',
        '\\': r'\\' 
    }
    return '"' + re.sub('\b|\t|\n|\f|\r|"\\', lambda x: ENC[x.group(0)], data) + '"'

CRC32_LE_TABLE = [
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
]

def eval_crc(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for b in data:
        crc = CRC32_LE_TABLE[(crc ^ b) & 0xff] ^ (crc >> 8)
    return crc ^ 0xFFFFFFFF

@dataclass
class BinHeader:
    crc: int        # 4 bytes
    size: int       # 2 bytes
    version: int    # 1 byte

def decode_header(val: bytes) -> BinHeader:
    crc, size, version = unpack('<IHB', val[:7])
    return BinHeader(crc, size, version)

#################################################################################################

c = Config('setup_data.h')
cd = ConfigData(c)

print(cd.get_full_toml(True))

