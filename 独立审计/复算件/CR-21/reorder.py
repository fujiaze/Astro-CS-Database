import io, re, sys
sys.stdout.reconfigure(encoding='utf-8')
p = r"独立审计/证据/通读-CR-21.md"
s = io.open(p, encoding='utf-8').read()
# strip all progress markers
s = re.sub(r'<!-- PROGRESS: \d/6 -->\n?', '', s)
parts = re.split(r'\n---\n\n(?=## \d\. )', s)
head = parts[0]
body = parts[1:]
def num(b):
    m = re.match(r'## (\d)\.', b.strip())
    return int(m.group(1)) if m else 99
body.sort(key=num)
out = head + "".join("\n---\n\n" + b.rstrip() + "\n" for b in body)
out = out.rstrip("\n") + "\n\n<!-- PROGRESS: 4/6 -->\n"
io.open(p, 'w', encoding='utf-8').write(out)
print("sections order:", [num(b) for b in body])
