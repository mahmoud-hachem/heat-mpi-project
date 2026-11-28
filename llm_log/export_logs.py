"""Export Claude Code + Codex session logs into llm_log/ (redacted raw JSONL + readable Markdown)."""
import json, re, os, sys, glob
from pathlib import Path

REPO = Path(r"C:\Users\User\Desktop\heat-mpi-project")
OUT = REPO / "llm_log"
HOME = Path(os.path.expanduser("~"))
EMAIL = os.environ.get("REDACT_EMAIL", "nobody@invalid")  # set REDACT_EMAIL to the address to hide

CLAUDE = [HOME / ".claude/projects/C--Users-User-Desktop-heat-mpi-project/e4d92ab6-f37a-4d95-87ef-8303ceea2544.jsonl"]
CX = HOME / ".codex/sessions/2026/11"
CODEX = [
    ("main", next(CX.glob("24/rollout-2026-11-24T20-24-58-*.jsonl"))),
    ("guardian", next(CX.glob("24/rollout-2026-11-24T21-12-13-*.jsonl"))),
    ("guardian", next(CX.glob("25/rollout-2026-11-25T14-32-14-*.jsonl"))),
    ("guardian", next(CX.glob("28/rollout-2026-11-28T12-31-49-*.jsonl"))),
    ("guardian", next(CX.glob("28/rollout-2026-11-28T15-52-58-*.jsonl"))),
    ("guardian", next(CX.glob("28/rollout-2026-11-28T15-54-15-*.jsonl"))),
]

SECRET_PATS = [
    (re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----.*?-----END [A-Z ]*PRIVATE KEY-----", re.S), "<REDACTED PRIVATE KEY>"),
    (re.compile(r"tskey-[A-Za-z0-9_-]{8,}"), "<REDACTED TAILSCALE KEY>"),
    (re.compile(r"gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}"), "<REDACTED GITHUB TOKEN>"),
    (re.compile(r"\bsk-(?:ant-)?[A-Za-z0-9_-]{20,}"), "<REDACTED API KEY>"),
    (re.compile(re.escape(EMAIL), re.I), "<user-email>"),
    (re.compile(re.escape(EMAIL.split("@")[0]), re.I), "<user-email>"),
    (re.compile(r"\buser-[A-Za-z0-9]{20,}"), "<user-id>"),
    (re.compile(r"(?<=\"creator_account_id\": \")[0-9a-f-]{36}"), "<account-id>"),
]
stats = {"redactions": 0, "blobs": 0}


def scrub_str(s):
    for p, rep in SECRET_PATS:
        s, n = p.subn(rep, s)
        stats["redactions"] += n
    return s


def scrub(o):
    """Recursively redact strings; drop opaque binary blobs (base64 images, encrypted reasoning, signatures)."""
    if isinstance(o, dict):
        r = {}
        for k, v in o.items():
            if k in ("encrypted_content", "signature") and isinstance(v, str) and len(v) > 40:
                r[k] = "<omitted: opaque encrypted blob>"; stats["blobs"] += 1
            elif k == "data" and isinstance(v, str) and len(v) > 500 and o.get("type") == "base64":
                r[k] = "<omitted: base64 image>"; stats["blobs"] += 1
            elif k in ("image_url", "url") and isinstance(v, str) and v.startswith("data:") and len(v) > 500:
                r[k] = "<omitted: base64 image>"; stats["blobs"] += 1
            else:
                r[k] = scrub(v)
        return r
    if isinstance(o, list):
        return [scrub(x) for x in o]
    if isinstance(o, str):
        return scrub_str(o)
    return o


def load(path):
    rows = []
    for line in open(path, encoding="utf8", errors="replace"):
        line = line.strip()
        if line:
            try:
                rows.append(scrub(json.loads(line)))
            except json.JSONDecodeError:
                rows.append({"_unparsed": scrub_str(line)})
    return rows


def write_jsonl(rows, path):
    with open(path, "w", encoding="utf8", newline="\n") as f:
        for r in rows:
            f.write(json.dumps(r, ensure_ascii=False) + "\n")


def fence(text, lang=""):
    text = text if isinstance(text, str) else json.dumps(text, ensure_ascii=False, indent=1)
    n = max([3] + [len(m) + 1 for m in re.findall(r"`{3,}", text)])
    return f"{'`' * n}{lang}\n{text}\n{'`' * n}\n"


def clip(text, n=6000):
    if not isinstance(text, str):
        text = json.dumps(text, ensure_ascii=False, indent=1)
    return text if len(text) <= n else text[:n] + f"\n… [{len(text) - n} more characters in the raw .jsonl]"


# ---------------- Claude Code ----------------
def claude_md(rows, title):
    out = [f"# {title}\n",
           "Readable rendering of the Claude Code session log. Every user prompt and assistant reply is shown in full; "
           "tool inputs are shown in full and tool outputs are clipped at 6,000 characters (the complete text is in the "
           "`.jsonl` next to this file). Internal reasoning blocks are marked *thinking*.\n"]
    for r in rows:
        t = r.get("type")
        ts = (r.get("timestamp") or "")[:19].replace("T", " ")
        if t == "summary" or r.get("isCompactSummary"):
            pass
        if t not in ("user", "assistant"):
            continue
        msg = r.get("message", {})
        content = msg.get("content")
        if isinstance(content, str):
            content = [{"type": "text", "text": content}]
        for c in content or []:
            ct = c.get("type")
            if t == "user" and ct == "text":
                txt = c["text"]
                if r.get("isCompactSummary"):
                    out.append(f"\n---\n\n## Context compaction summary ({ts})\n\n{txt}\n")
                elif r.get("isMeta") or txt.startswith("<system-reminder>") or txt.startswith("<local-command"):
                    out.append(f"\n<details><summary>Harness/meta message ({ts})</summary>\n\n{fence(clip(txt, 3000))}</details>\n")
                else:
                    out.append(f"\n---\n\n## 🧑 User ({ts})\n\n{txt}\n")
            elif t == "user" and ct == "tool_result":
                cc = c.get("content")
                if isinstance(cc, list):
                    cc = "\n".join(x.get("text", "[image]") if x.get("type") == "text" else "[image]" for x in cc)
                out.append(f"\n<details><summary>Tool result{' (error)' if c.get('is_error') else ''}</summary>\n\n"
                           f"{fence(clip(cc or ''))}</details>\n")
            elif t == "user" and ct == "image":
                out.append("\n*[image attached by user]*\n")
            elif t == "assistant" and ct == "text":
                out.append(f"\n### 🤖 Claude ({ts})\n\n{c['text']}\n")
            elif t == "assistant" and ct == "thinking":
                if c.get("thinking"):
                    out.append(f"\n<details><summary>thinking</summary>\n\n{c['thinking']}\n</details>\n")
            elif t == "assistant" and ct == "tool_use":
                out.append(f"\n**Tool call: `{c.get('name')}`**\n\n{fence(json.dumps(c.get('input'), ensure_ascii=False, indent=1), 'json')}")
    return "".join(out)


# ---------------- Codex ----------------
def codex_md(rows, title):
    out = [f"# {title}\n",
           "Readable rendering of a Codex Desktop rollout log. User prompts and assistant messages are in full; tool calls "
           "in full; tool outputs clipped at 6,000 characters (complete text in the `.jsonl`). Developer/system "
           "instructions injected by Codex are collapsed. Reasoning is shown only where Codex stored a readable "
           "summary (the rest is encrypted by OpenAI and cannot be exported).\n"]
    for r in rows:
        if r.get("type") == "session_meta":
            p = r["payload"]
            out.append(f"\n- session id: `{p.get('id')}`\n- parent thread: `{p.get('parent_thread_id', '—')}`\n"
                       f"- source: `{json.dumps(p.get('source'))}`\n- cwd: `{p.get('cwd')}`\n- started: {p.get('timestamp')}\n")
        if r.get("type") != "response_item":
            continue
        p = r["payload"]
        ts = (r.get("timestamp") or "")[:19].replace("T", " ")
        pt = p.get("type")
        if pt == "message":
            texts = [c.get("text", "") if "text" in c else "[image]" for c in p.get("content", [])]
            txt = "\n".join(texts)
            role = p.get("role")
            if role in ("developer", "system") or (role == "user" and txt.lstrip().startswith(("<environment_context", "<permissions", "# AGENTS.md", "<user_instructions"))):
                out.append(f"\n<details><summary>{role} context ({ts})</summary>\n\n{fence(clip(txt, 3000))}</details>\n")
            elif role == "user":
                out.append(f"\n---\n\n## 🧑 User ({ts})\n\n{txt}\n")
            else:
                out.append(f"\n### 🤖 Codex ({ts})\n\n{txt}\n")
        elif pt == "reasoning":
            s = "\n".join(x.get("text", "") for x in p.get("summary", []) or [])
            if s.strip():
                out.append(f"\n<details><summary>reasoning summary</summary>\n\n{s}\n</details>\n")
        elif pt in ("function_call", "custom_tool_call", "local_shell_call", "web_search_call"):
            arg = p.get("arguments") or p.get("input") or p.get("action") or ""
            try:
                arg = json.dumps(json.loads(arg), ensure_ascii=False, indent=1) if isinstance(arg, str) else json.dumps(arg, ensure_ascii=False, indent=1)
            except Exception:
                pass
            out.append(f"\n**Tool call: `{p.get('name', pt)}`**\n\n{fence(arg)}")
        elif pt in ("function_call_output", "custom_tool_call_output"):
            o = p.get("output")
            if isinstance(o, dict):
                o = o.get("content") or o.get("output") or o
            if isinstance(o, list):
                o = "\n".join(x.get("text", "[image]") if isinstance(x, dict) else str(x) for x in o)
            out.append(f"\n<details><summary>Tool output</summary>\n\n{fence(clip(o if isinstance(o, str) else json.dumps(o, ensure_ascii=False)))}</details>\n")
    return "".join(out)


def main():
    (OUT / "claude_code").mkdir(parents=True, exist_ok=True)
    (OUT / "codex").mkdir(parents=True, exist_ok=True)
    (OUT / "chatgpt").mkdir(parents=True, exist_ok=True)
    index = []
    for p in CLAUDE:
        rows = load(p)
        base = OUT / "claude_code" / ("session_" + p.stem)
        write_jsonl(rows, base.with_suffix(".jsonl"))
        base.with_suffix(".md").write_text(claude_md(rows, f"Claude Code session {p.stem}"), encoding="utf8")
        ts = [r.get("timestamp") for r in rows if r.get("timestamp")]
        index.append(("claude_code", base.name, ts[0], ts[-1], len(rows)))
    for kind, p in CODEX:
        rows = load(p)
        name = p.stem.replace("rollout-", "") + ("" if kind == "main" else "_guardian")
        base = OUT / "codex" / name
        write_jsonl(rows, base.with_suffix(".jsonl"))
        t = "Codex main thread" if kind == "main" else "Codex guardian (approval-reviewer) subagent"
        base.with_suffix(".md").write_text(codex_md(rows, f"{t}: {p.stem}"), encoding="utf8")
        ts = [r.get("timestamp") for r in rows if r.get("timestamp")]
        index.append(("codex", base.name, ts[0], ts[-1], len(rows)))
    for i in index:
        print(i)
    print(stats)


main()
