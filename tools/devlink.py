#!/usr/bin/env python3
# devlink.py —— Coopa 卡 USB 串口联机工具(协议见 coopa-sdk/include/kit_devlink.h)。
# 用法:
#   devlink.py snap out.png          截屏存 PNG
#   devlink.py key up|down|ok        模拟按一下 ▲ / ▼ / ●
#   devlink.py stat                  读一行状态(ST/RP/CD...)
#   devlink.py seq "down ok snap shot1.png"  依次执行多条命令(key:/snap:/sleep:0.3)
# 只读画面、只模拟按键;绝不发送 RST*/RRST 等删档管理命令。
import os, sys, select, struct, termios, zlib, time, fcntl

PORT = "/dev/cu.usbmodem14101"  # macOS 上数据口要用 cu.*(tty.* 带载波检测会卡)
KEYS = {"up": b"KEYU", "down": b"KEYD", "ok": b"KEYO"}


class Dev:
    def __init__(self, path=PORT):
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        try:  # raw 模式 + 拉起 DTR/RTS(ESP32 的 USB 串口要这两根信号线才收数据)
            attrs = termios.tcgetattr(self.fd)
            attrs[0] = attrs[1] = attrs[3] = 0
            attrs[2] = termios.CS8
            termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
            bits = struct.pack("I", termios.TIOCM_DTR | termios.TIOCM_RTS)
            fcntl.ioctl(self.fd, termios.TIOCMBIS, bits)
        except (termios.error, OSError):
            pass
        self.buf = b""

    def _read(self, timeout):
        r, _, _ = select.select([self.fd], [], [], timeout)
        if not r:
            return b""
        try:
            return os.read(self.fd, 65536)
        except OSError:
            return b""

    def drain(self, sec=0.3):
        end = time.time() + sec
        while time.time() < end:
            got = self._read(0.05)
            if got:
                end = time.time() + 0.1
        self.buf = b""

    def send(self, cmd: bytes):
        end = time.time() + 2.0
        while time.time() < end:  # 非阻塞写可能 EAGAIN:等缓冲空了重试
            try:
                os.write(self.fd, cmd)
                return
            except BlockingIOError:
                select.select([], [self.fd], [], 0.05)
        raise IOError("串口写不进去(设备没接好或被占用?)")

    def read_line(self, timeout=2.0):
        end = time.time() + timeout
        while time.time() < end:
            i = self.buf.find(b"\n")
            if i >= 0:
                line, self.buf = self.buf[:i], self.buf[i + 1:]
                return line
            self.buf += self._read(max(0.01, end - time.time()))
        return None

    def read_exact(self, n, timeout=5.0):
        end = time.time() + timeout
        while len(self.buf) < n and time.time() < end:
            self.buf += self._read(max(0.01, end - time.time()))
        if len(self.buf) < n:
            raise IOError("串口超时:要 %d 字节只有 %d" % (n, len(self.buf)))
        out, self.buf = self.buf[:n], self.buf[n:]
        return out

    def snap(self):
        self.drain(0.15)
        self.send(b"SNAP")
        head = self.read_line()
        if not head or not head.startswith(b"GSHOT1"):
            raise IOError("没等到 GSHOT1,收到 %r" % head)
        parts = head.split()
        w, h = int(parts[1]), int(parts[2])
        raw = bytearray(w * h * 2)
        total = 0
        while True:
            line = self.read_line()
            if line is None:
                raise IOError("截屏中断:收了 %d 字节" % total)
            if line == b"END":
                break
            if not line.startswith(b"S "):
                continue
            x1, y1, x2, y2 = (int(v) for v in line.split()[1:5])
            cw, ch = x2 - x1 + 1, y2 - y1 + 1
            data = self.read_exact(cw * ch * 2)
            for row in range(ch):
                off = ((y1 + row) * w + x1) * 2
                raw[off:off + cw * 2] = data[row * cw * 2:(row + 1) * cw * 2]
            total += cw * ch * 2
        return w, h, raw


def png_write(path, w, h, rgb565):
    def chunk(tag, data):
        c = tag + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c))
    rows = bytearray()
    for y in range(h):
        rows.append(0)
        for x in range(w):
            v = struct.unpack_from("<H", rgb565, (y * w + x) * 2)[0]
            rows += bytes(((v & 0xF800) >> 8, (v & 0x07E0) >> 3, (v & 0x001F) << 3))
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(rows))) + chunk(b"IEND", b""))


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    d = Dev()
    cmd, args = argv[1], argv[2:]
    if cmd == "snap":
        w, h, raw = d.snap()
        png_write(args[0], w, h, raw)
        print("已存 %s (%dx%d)" % (args[0], w, h))
    elif cmd == "key":
        d.drain(0.1)
        d.send(KEYS[args[0]])
        time.sleep(0.15)
    elif cmd == "stat":
        d.drain(0.1)
        d.send(b"STAT")
        print((d.read_line() or b"(no reply)").decode(errors="replace"))
    elif cmd == "seq":
        for step in " ".join(args).split():
            if step.startswith("key:"):
                d.drain(0.1); d.send(KEYS[step[4:]]); time.sleep(0.15)
            elif step.startswith("sleep:"):
                time.sleep(float(step[6:]))
            elif step.startswith("snap:"):
                w, h, raw = d.snap()
                png_write(step[5:], w, h, raw)
                print("已存", step[5:])
            elif step.startswith("stat:"):
                d.send(b"STAT"); print((d.read_line() or b"(no reply)").decode(errors="replace"))
            else:
                raise SystemExit("不认识的步骤:" + step)
    else:
        raise SystemExit("不认识的命令:" + cmd)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
