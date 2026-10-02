from pathlib import Path
p = Path('installer/src/main.c')
s = p.read_text()

old_notify = '''static void notify(const char *msg) {\n    if (g_notify) {\n        char b[512];\n        snprintf(b, sizeof(b), "%s", msg);\n        g_notify(222, b);\n    }\n    sceKernelDebugOutText(0, "%s\\n", msg);\n}\n'''
new_notify = '''static void notify(const char *msg) {\n    int fd = open("/data/TrophyUnlocker1352.log", O_WRONLY | O_CREAT | O_APPEND, 0666);\n    if (fd >= 0) {\n        (void)write(fd, msg, strlen(msg));\n        (void)write(fd, "\\n", 1);\n        (void)fsync(fd);\n        close(fd);\n    }\n    if (g_notify) {\n        char b[512];\n        snprintf(b, sizeof(b), "%s", msg);\n        g_notify(222, b);\n    }\n    sceKernelDebugOutText(0, "%s\\n", msg);\n}\n'''
if old_notify not in s:
    raise SystemExit('notify block not found')
s = s.replace(old_notify, new_notify, 1)

marker = 'static int copy_file(const char *src, const char *dst) {'
helper = '''static __attribute__((noreturn)) void idle_forever(void) {\n    /* Returning from main may hit _exit/SIGSYS on PS4/OpenOrbis and surface as CE-34878-0. */\n    for (;;) sleep(60);\n}\n\n'''
if marker not in s:
    raise SystemExit('copy_file marker not found')
s = s.replace(marker, helper + marker, 1)

repls = {
    'notify("Trophy Unlocker 13.52: preparando plugin GoldHEN...");': 'notify("Trophy Unlocker 13.52 V4: preparando plugin GoldHEN...");',
    'sleep(5); return 1;': 'sleep(5); idle_forever();',
    'if (!games) { notify("Sem memoria para listar jogos."); sleep(4); return 2; }': 'if (!games) { notify("Sem memoria para listar jogos."); sleep(4); idle_forever(); }',
    'sleep(6); return 3;': 'sleep(6); idle_forever();',
    'sleep(6); return 4;': 'sleep(6); idle_forever();',
    'free(games); sleep(6); return 5;': 'free(games); sleep(6); idle_forever();',
    'free(games); sleep(7); return 0;': 'free(games); (void)scePadClose(pad); sleep(2); idle_forever();',
    'sleep(3); return 0;': '(void)scePadClose(pad); sleep(2); idle_forever();',
}
for a, b in repls.items():
    if a not in s:
        raise SystemExit(f'expected text not found: {a}')
    s = s.replace(a, b, 1)

main = s[s.index('int main(void) {'):]
if 'return ' in main:
    raise SystemExit('return still present in main')
if 'idle_forever' not in main:
    raise SystemExit('idle helper not used')
p.write_text(s)
print('V4 patch applied; main no longer returns; runtime log enabled')
