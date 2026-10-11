#!/usr/bin/env python3
# 生成《库巴的拖鞋工坊》角色配音(微软神经语音,和平台 kit_lines 同源同款):
# edge-tts 合成 → miniaudio 解码 16kHz 单声道 → 修剪静音 → 峰值归一 → IMA ADPCM
# → 生成 src/slipperworks_voices.c / .h。
# 用法: /tmp/ttsenv/bin/python3 gen_voices.py   (需网络;缓存到 build/ 里,重复跑不重花钱)
import struct, subprocess, sys, os, math

HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(HERE, 'build')
RAW = os.path.join(HERE, 'raw')  # 真实录音放这里(mp3/m4a/wav),可整段替换 TTS
os.makedirs(CACHE, exist_ok=True)

# 声音:一比一克隆《库巴传炸弹》tools/gen_lines.py 的官方参数(和平台 kit_lines 同源同款)
GRANNY = dict(voice='zh-CN-shaanxi-XiaoniNeural', rate='+0%', pitch='-10Hz')
KUBA = dict(voice='zh-CN-YunxiaNeural', rate='+5%', pitch='+0Hz')

LINES = [
    ('G_O2', GRANNY, '小样儿,看你躲到啥时候!'),
    ('K_O3', KUBA, '奶奶!我都躲了三百下,你一次都没打中!'),
    ('G_PRAISE', GRANNY, '大孙子真棒!'),
    ('G_ENOUGH', GRANNY, '原料管够!'),
    ('G_T_DROP', GRANNY, '来,先丢一只试试!'),
    ('G_RAIN', GRANNY, '拖鞋雨!'),
    ('G_GRAT1', GRANNY, '大孙子,看你把工坊做成这样,奶奶心里真美!'),
    ('G_GRAT2', GRANNY, '奶奶没啥能帮你的了,就把奶奶拖传给你吧!'),
    ('K_GRAT', KUBA, '谢谢奶奶!谢谢奶奶一路的鼓励和支持!'),
    ('K_OPEN', KUBA, '奶奶!我要开拖鞋工坊!'),
    ('K_DEAL', KUBA, '一言为定,开张!'),
]

# ---- IMA ADPCM 表(标准,和 ffmpeg 一致) ----
STEP = [7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,
        118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,
        876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,
        4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,
        18500,20350,22385,24623,27086,29794,32767]
IDX = [(-1, -1, -1, -1, 2, 4, 6, 8), (-1, -1, -1, -1, 2, 4, 6, 8)]
BLOCK = 1024
BLOCK_SAMPLES = 1 + (BLOCK - 4) * 2


def synth(name, cfg, text):
    mp3 = os.path.join(CACHE, name + '.mp3')
    if not os.path.exists(mp3):
        cmd = ['/tmp/ttsenv/bin/edge-tts', '--voice', cfg['voice'], '--text', text,
               '--rate=' + cfg['rate'], '--pitch=' + cfg['pitch'], '--write-media', mp3]
        subprocess.run(cmd, check=True, capture_output=True)
    return mp3


def decode16k(mp3):
    import miniaudio
    raw = miniaudio.decode_file(mp3, nchannels=1, sample_rate=16000,
                                output_format=miniaudio.SampleFormat.SIGNED16)
    return raw.samples


def trim(samples, threshold=600, pad=700):
    lo, hi = 0, len(samples) - 1
    while lo < len(samples) and abs(samples[lo]) < threshold:
        lo += 1
    while hi > lo and abs(samples[hi]) < threshold:
        hi -= 1
    if hi <= lo:
        return []
    lo = max(0, lo - pad)
    hi = min(len(samples) - 1, hi + pad)
    return samples[lo:hi + 1]


def normalize(samples, target=27000):
    peak = max(1, max(abs(s) for s in samples))
    gain = min(4.0, target / peak)
    return [max(-32768, min(32767, int(s * gain))) for s in samples]


def encode_ima(samples):
    out = bytearray()
    pred, idx = 0, 0
    for blk_start in range(0, len(samples), BLOCK_SAMPLES):
        blk = samples[blk_start:blk_start + BLOCK_SAMPLES]
        hdr_pred = blk[0] if blk else pred
        blk_idx = max(0, min(88, idx))
        out += struct.pack('<hBB', hdr_pred, blk_idx, 0)
        pred, idx = hdr_pred, blk_idx
        nibbles = []
        for i in range(1, len(blk)):
            diff = blk[i] - pred
            sign = 0
            if diff < 0:
                sign = 8
                diff = -diff
            step = STEP[idx]
            code = 0
            if diff >= step:
                code = 4
                diff -= step
            if diff >= step >> 1:
                code |= 2
                diff -= step >> 1
            if diff >= step >> 2:
                code |= 1
            nibble = code | sign
            nibbles.append(nibble)
            delta = step >> 3
            if code & 4: delta += step
            if code & 2: delta += step >> 1
            if code & 1: delta += step >> 2
            pred += -delta if sign else delta
            pred = max(-32768, min(32767, pred))
            idx = max(0, min(88, idx + IDX[sign >> 3][code]))
        if len(nibbles) % 2:
            nibbles.append(0)
        for i in range(0, len(nibbles), 2):
            out.append(nibbles[i] | (nibbles[i + 1] << 4))
    return bytes(out)


def decode_ima(data, n_samples):
    out = []
    pos = 0
    pred, idx = 0, 0
    in_block = 0
    while len(out) < n_samples:
        if in_block == 0 and pos + 4 <= len(data):
            pred, idx = struct.unpack_from('<hB', data, pos)
            pos += 4
            in_block = (BLOCK - 4) * 2
            out.append(pred)
            continue
        if pos >= len(data):
            break
        b = data[pos]
        pos += 1
        for nib in (b & 0xf, b >> 4):
            if in_block == 0:
                break
            step = STEP[idx]
            delta = step >> 3
            if nib & 4: delta += step
            if nib & 2: delta += step >> 1
            if nib & 1: delta += step >> 2
            pred += -delta if nib & 8 else delta
            pred = max(-32768, min(32767, pred))
            idx = max(0, min(88, idx + IDX[nib >> 3][nib & 7]))
            out.append(pred)
            in_block -= 1
    return out[:n_samples]


def main():
    chunks, decls, total = [], [], 0
    for name, cfg, text in LINES:
        mp3 = synth(name, cfg, text)
        samples = trim(decode16k(mp3))
        if not samples:
            sys.exit(f'{name}: 合成为空')
        samples = normalize(samples)
        adpcm = encode_ima(samples)
        back = decode_ima(adpcm, len(samples))
        rms = math.sqrt(sum(s * s for s in back) / len(back))
        assert rms > 800, f'{name}: 解码能量异常 rms={rms}'
        assert len(back) == len(samples)
        total += len(adpcm)
        var = 'SW_ADPCM_' + name
        toks = [f'0x{b:02x}' for b in adpcm]
        lines, cur = [], ''
        for tok in toks:
            if len(cur) + len(tok) + 1 > 74:
                lines.append(cur + ',')
                cur = tok
            else:
                cur = cur + ',' + tok if cur else tok
        if cur:
            lines.append(cur)
        chunks.append(f'// {name}: "{text}" {len(samples)/16000:.1f}s\n'
                      f'static const uint8_t {var}[] = {{\n'
                      + '\n'.join('    ' + l for l in lines) + '\n};\n'
                      f'const kit_line_t slipperworks_voice_{name} = {{ {var}, {len(samples)}, "SW_{name}" }};')
        decls.append(f'extern const kit_line_t slipperworks_voice_{name};')
        print(f'{name}: {len(samples)/16000:.2f}s {len(adpcm)}B rms={rms:.0f}')

    with open(os.path.join(HERE, '../../src/slipperworks_voices.h'), 'w') as f:
        f.write('// src/slipperworks_voices.h —— 角色配音(tools/voices/gen_voices.py 生成,勿手改)。\n'
                '// 声音:库巴 = zh-CN-YunxiNeural,奶奶 = zh-CN-XiaoxiaoNeural(压低音高语速)。\n'
                '#pragma once\n\n#include "kit_lines.h"\n\n'
                + '\n'.join(decls) + '\n')
    with open(os.path.join(HERE, '../../src/slipperworks_voices.c'), 'w') as f:
        f.write('// src/slipperworks_voices.c —— 角色配音数据(tools/voices/gen_voices.py 生成,勿手改)。\n'
                '// IMA ADPCM,16kHz 单声道,格式同 kit_voice_dsp.h。\n'
                '#include "slipperworks_voices.h"\n\n'
                + '\n'.join(chunks) + '\n')
    print(f'共 {len(LINES)} 句,{total/1024:.0f} KB ADPCM')


if __name__ == '__main__':
    main()
