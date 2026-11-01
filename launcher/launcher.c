/* Wreckless: Recompiled's launcher and first-run setup.

   The download holds no part of the game: this launcher, the translator (the toolkit's tools on a
   Python of their own), a C compiler (Zig), the runtime built ahead of time (lib/) with the headers
   the generated code includes, and the list of the disc's files (game.manifest.sha256). The first
   time it runs it finds the player's copy of the game (a disc image, or a folder of the disc's
   files), checks every file against the list, keeps them in the user folder (game/), translates
   default.xbe to C (build/gen), compiles that and links it with the runtime (bin/wreckless). After
   that it starts the game at once, until a new download (another VERSION) builds it again.

   Bundle (the folder holding VERSION; by default <launcher's folder>/../lib/wreckless-recompiled,
   or WRECKLESS_BUNDLE): VERSION, zig/, translate, python/, antbox/, config/, include/, lib/,
   game.manifest.sha256, GEN_SHA256, enhance.toml.
   User folder: the game's own (WRECKLESS_USER, else XDG_DATA_HOME or ~/.local/share, then
   /wreckless).

   Without a window (--setup-from <image or folder> [--no-launch]) it does the same in the
   terminal: for tests and for those who prefer it. */
#include <SDL3/SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <dlfcn.h>
#include <gnu/libc-version.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sha256.h"
#include "overlay/overlay_font_ui.h"

#define TARGET "x86_64-linux-gnu.2.38"   /* (the prebuilt runtime's glibc) */

static char bundle[1024], user[1024];
static char version[128];
static char release[32];                /* (VERSION's second word, as 0.1.0-alpha: shown by the title) */

/* ------------------------------------------------------------------ the shared state */
static SDL_Mutex *lock;
static char status[256], detail[1024];
static double progress = -1;            /* 0..1, or < 0 for none */
static volatile int task_done = 1, task_ok = 1;
static int headless;

static void say(const char *st, const char *de)
{
    SDL_LockMutex(lock);
    if (st)
        SDL_strlcpy(status, st, sizeof status);
    if (de)
        SDL_strlcpy(detail, de, sizeof detail);
    SDL_UnlockMutex(lock);
    if (headless)
        SDL_Log("%s%s%s", st ? st : "", st && de && *de ? ": " : "", de ? de : "");
}

static void set_progress(double p)
{
    SDL_LockMutex(lock);
    progress = p;
    SDL_UnlockMutex(lock);
}

/* ------------------------------------------------------------------ files */
static void path(char *out, size_t n, const char *a, const char *b)
{
    SDL_snprintf(out, n, "%s/%s", a, b);
}

static int exists(const char *p)
{
    SDL_PathInfo i;
    return SDL_GetPathInfo(p, &i);
}

static char *read_text(const char *p)
{
    size_t n;
    char *s = SDL_LoadFile(p, &n);
    return s;
}

static int write_text(const char *p, const char *s)
{
    return SDL_SaveFile(p, s, SDL_strlen(s));
}

static void make_dirs(const char *p)
{
    char t[1024];
    SDL_strlcpy(t, p, sizeof t);
    for (char *c = t + 1; *c; c++)
        if (*c == '/' || *c == '\\') {
            char k = *c;
            *c = 0;
            SDL_CreateDirectory(t);
            *c = k;
        }
    SDL_CreateDirectory(t);
}

/* a path under root whose components match the manifest's case-blind (the game's files keep whatever
   case the installer gave them) */
typedef struct { const char *want; char found[256]; } FindName;

static SDL_EnumerationResult find_cb(void *ud, const char *dir, const char *name)
{
    FindName *f = ud;
    if (!SDL_strcasecmp(name, f->want)) {
        SDL_strlcpy(f->found, name, sizeof f->found);
        return SDL_ENUM_SUCCESS;
    }
    return SDL_ENUM_CONTINUE;
}

static int resolve_ci(const char *root, const char *rel, char *out, size_t n)
{
    char cur[1024], part[256];
    SDL_strlcpy(cur, root, sizeof cur);
    const char *p = rel;
    while (*p) {
        size_t k = strcspn(p, "/\\");
        if (k >= sizeof part)
            return 0;
        SDL_memcpy(part, p, k);
        part[k] = 0;
        FindName f = {part, ""};
        SDL_EnumerateDirectory(cur, find_cb, &f);
        if (!f.found[0])
            return 0;
        SDL_strlcat(cur, "/", sizeof cur);
        SDL_strlcat(cur, f.found, sizeof cur);
        p += k;
        while (*p == '/' || *p == '\\')
            p++;
    }
    SDL_strlcpy(out, cur, n);
    return 1;
}

static int file_sha256(const char *p, char hex[65], Sint64 *size)
{
    SDL_IOStream *io = SDL_IOFromFile(p, "rb");
    if (!io)
        return 0;
    Sha256 s;
    sha256_init(&s);
    static uint8_t buf[1 << 16];
    size_t r;
    Sint64 total = 0;
    while ((r = SDL_ReadIO(io, buf, sizeof buf)) > 0) {
        sha256_update(&s, buf, r);
        total += (Sint64)r;
    }
    SDL_CloseIO(io);
    uint8_t d[32];
    sha256_final(&s, d);
    sha256_hex(d, hex);
    if (size)
        *size = total;
    return 1;
}

/* ------------------------------------------------------------------ running the tools */
/* Runs argv, waits; its output (stdout and stderr) goes to the setup log, and its last lines are
   kept for an error message. Returns its exit code (-1: not started). */
static char last_output[2048];

static int run(const char *const *argv)
{
    SDL_PropertiesID pr = SDL_CreateProperties();
    SDL_SetPointerProperty(pr, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)argv);
    SDL_SetNumberProperty(pr, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(pr, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    SDL_Process *p = SDL_CreateProcessWithProperties(pr);
    SDL_DestroyProperties(pr);
    if (!p) {
        SDL_snprintf(last_output, sizeof last_output, "cannot start %s: %s", argv[0], SDL_GetError());
        return -1;
    }
    size_t n = 0;
    int code = -1;
    char *out = SDL_ReadProcess(p, &n, &code);
    char log[1024];
    path(log, sizeof log, user, "setup.log");
    SDL_IOStream *lf = SDL_IOFromFile(log, "ab");
    if (lf) {
        SDL_IOprintf(lf, "$");
        for (int i = 0; argv[i]; i++)
            SDL_IOprintf(lf, " %s", argv[i]);
        SDL_IOprintf(lf, "\n%s\n[exit %d]\n", out ? out : "", code);
        SDL_CloseIO(lf);
    }
    last_output[0] = 0;
    if (out) {
        size_t k = n > sizeof last_output - 1 ? n - (sizeof last_output - 1) : 0;
        SDL_strlcpy(last_output, out + k, sizeof last_output);
        SDL_free(out);
    }
    SDL_DestroyProcess(p);
    return code;
}

/* ------------------------------------------------------------------ the steps */
typedef struct { char sha[65]; char rel[256]; } ManifestEntry;
static ManifestEntry *manifest;
static int nmanifest;

static int load_manifest(void)
{
    if (manifest)
        return 1;
    char p[1024];
    path(p, sizeof p, bundle, "game.manifest.sha256");
    char *s = read_text(p);
    if (!s)
        return 0;
    int cap = 128;
    manifest = SDL_calloc(cap, sizeof *manifest);
    char *save = NULL;
    for (char *line = SDL_strtok_r(s, "\r\n", &save); line; line = SDL_strtok_r(NULL, "\r\n", &save)) {
        if (SDL_strlen(line) < 67)
            continue;
        if (nmanifest == cap)
            manifest = SDL_realloc(manifest, (size_t)(cap *= 2) * sizeof *manifest);
        SDL_memcpy(manifest[nmanifest].sha, line, 64);
        manifest[nmanifest].sha[64] = 0;
        SDL_strlcpy(manifest[nmanifest].rel, line + 66, sizeof manifest[nmanifest].rel);
        nmanifest++;
    }
    SDL_free(s);
    return nmanifest > 0;
}

/* the folder holding default.xbe under root (itself or up to three levels down), into out */
typedef struct { char *out; size_t n; int depth; int found; } FindGame;

static SDL_EnumerationResult find_game_cb(void *ud, const char *dir, const char *name)
{
    FindGame *f = ud;
    char p[1024];
    SDL_snprintf(p, sizeof p, "%s%s", dir, name);
    if (!SDL_strcasecmp(name, "default.xbe")) {
        SDL_strlcpy(f->out, dir, f->n);
        size_t k = SDL_strlen(f->out);
        if (k && (f->out[k - 1] == '/' || f->out[k - 1] == '\\'))
            f->out[k - 1] = 0;
        f->found = 1;
        return SDL_ENUM_SUCCESS;
    }
    SDL_PathInfo i;
    if (f->depth < 3 && SDL_GetPathInfo(p, &i) && i.type == SDL_PATHTYPE_DIRECTORY) {
        f->depth++;
        SDL_EnumerateDirectory(p, find_game_cb, f);
        f->depth--;
        if (f->found)
            return SDL_ENUM_SUCCESS;
    }
    return SDL_ENUM_CONTINUE;
}

static int find_game(const char *root, char *out, size_t n)
{
    FindGame f = {out, n, 0, 0};
    SDL_EnumerateDirectory(root, find_game_cb, &f);
    return f.found;
}

/* checks the disc's files under src, then copies them into the user folder's game/ */
static int check_and_copy(const char *src)
{
    if (!load_manifest()) {
        say("The download is incomplete", "its list of the game's files is missing");
        return 0;
    }
    char dst_root[1024];
    path(dst_root, sizeof dst_root, user, "game");
    int same = !SDL_strcmp(src, dst_root);
    for (int i = 0; i < nmanifest; i++) {
        char p[1024], hex[65];
        set_progress((double)i / nmanifest);
        say("Checking your copy of Wreckless", manifest[i].rel);
        if (!resolve_ci(src, manifest[i].rel, p, sizeof p)) {
            char m[512];
            SDL_snprintf(m, sizeof m, "%s is missing: this needs the US Xbox disc of Wreckless", manifest[i].rel);
            say("This isn't the US disc of Wreckless", m);
            return 0;
        }
        if (!file_sha256(p, hex, NULL) || SDL_strcmp(hex, manifest[i].sha)) {
            char m[512];
            SDL_snprintf(m, sizeof m, "%s isn't the US disc's (another version, or damaged)", manifest[i].rel);
            say("This isn't the US disc of Wreckless", m);
            return 0;
        }
        if (!same) {
            char d[1024];
            path(d, sizeof d, dst_root, manifest[i].rel);
            char dir[1024];
            SDL_strlcpy(dir, d, sizeof dir);
            char *slash = SDL_strrchr(dir, '/');
            if (slash) {
                *slash = 0;
                make_dirs(dir);
            }
            if (!SDL_CopyFile(p, d)) {
                say("Could not copy the game's files", SDL_GetError());
                return 0;
            }
        }
    }
    char mark[1024];
    path(mark, sizeof mark, dst_root, ".disc-checked");
    write_text(mark, version);
    set_progress(1);
    return 1;
}

static SDL_EnumerationResult remove_entry(void *ud, const char *dir, const char *name);

static void remove_tree(const char *p)
{
    SDL_EnumerateDirectory(p, remove_entry, NULL);
    SDL_RemovePath(p);
}

static SDL_EnumerationResult remove_entry(void *ud, const char *dir, const char *name)
{
    (void)ud;
    char p[1024];
    SDL_PathInfo i;
    SDL_snprintf(p, sizeof p, "%s%s", dir, name);   /* (dir ends in a separator) */
    if (SDL_GetPathInfo(p, &i) && i.type == SDL_PATHTYPE_DIRECTORY)
        remove_tree(p);
    else
        SDL_RemovePath(p);
    return SDL_ENUM_CONTINUE;
}

/* ------------------------------------------------------------------ a disc image */
/* An Xbox disc's files are in XDVDFS: a volume descriptor at sector 32 of the game partition ("MICROSOFT*XBOX*MEDIA"
   at both ends), then directories as binary trees of entries (left, right in units of 4 bytes, sector, size, attributes,
   name length, name). A plain game image has the partition at its start; a whole-disc dump has it further on. */
static const Sint64 k_partitions[] = {0, 0x18300000, 0xFD90000, 0x2080000};
#define XISO_SECTOR 2048

static Sint64 xiso_total, xiso_done;

static int xiso_copy(SDL_IOStream *io, Sint64 base, Uint32 sector, Uint32 size, const char *to)
{
    static Uint8 buf[1 << 20];
    SDL_IOStream *o = SDL_IOFromFile(to, "wb");
    if (!o)
        return 0;
    if (SDL_SeekIO(io, base + (Sint64)sector * XISO_SECTOR, SDL_IO_SEEK_SET) < 0) {
        SDL_CloseIO(o);
        return 0;
    }
    Uint32 left = size;
    while (left) {
        size_t want = left < sizeof buf ? left : sizeof buf;
        size_t got = SDL_ReadIO(io, buf, want);
        if (!got || SDL_WriteIO(o, buf, got) != got) {
            SDL_CloseIO(o);
            return 0;
        }
        left -= (Uint32)got;
        xiso_done += (Sint64)got;
    }
    SDL_CloseIO(o);
    if (xiso_total)
        set_progress((double)xiso_done / (double)xiso_total);
    return 1;
}

/* the entry at offset in a directory's table, and those on either side of it. With to NULL it only adds up the sizes. */
static int xiso_walk(SDL_IOStream *io, Sint64 base, const Uint8 *tab, Uint32 len, Uint32 off, const char *to, int depth)
{
    if (off + 14 > len || depth > 200)
        return 0;
    const Uint8 *e = tab + off;
    Uint16 l = (Uint16)(e[0] | e[1] << 8), r = (Uint16)(e[2] | e[3] << 8);
    Uint32 sector = (Uint32)e[4] | (Uint32)e[5] << 8 | (Uint32)e[6] << 16 | (Uint32)e[7] << 24;
    Uint32 size = (Uint32)e[8] | (Uint32)e[9] << 8 | (Uint32)e[10] << 16 | (Uint32)e[11] << 24;
    Uint8 attr = e[12], nl = e[13];
    if (off + 14 + nl > len)
        return 0;
    char name[256], p[1024];
    SDL_memcpy(name, e + 14, nl);
    name[nl] = 0;
    if (SDL_strchr(name, '/') || !SDL_strcmp(name, "..") || !SDL_strcmp(name, "."))
        return 0;                       /* (a damaged image must not write outside its folder) */
    if (l && l != 0xFFFF && !xiso_walk(io, base, tab, len, (Uint32)l * 4, to, depth + 1))
        return 0;
    if (to)
        SDL_snprintf(p, sizeof p, "%s/%s", to, name);
    if (attr & 0x10) {
        Uint8 *sub = size ? SDL_malloc(size) : NULL;
        if (size) {
            if (!sub || SDL_SeekIO(io, base + (Sint64)sector * XISO_SECTOR, SDL_IO_SEEK_SET) < 0
                    || SDL_ReadIO(io, sub, size) != size) {
                SDL_free(sub);
                return 0;
            }
        }
        if (to)
            make_dirs(p);
        int ok = !size || xiso_walk(io, base, sub, size, 0, to ? p : NULL, depth + 1);
        SDL_free(sub);
        if (!ok)
            return 0;
    } else if (to) {
        say(NULL, name);
        if (!xiso_copy(io, base, sector, size, p))
            return 0;
    } else
        xiso_total += size;
    if (r && r != 0xFFFF && !xiso_walk(io, base, tab, len, (Uint32)r * 4, to, depth + 1))
        return 0;
    return 1;
}

/* the disc's files from the image into the user folder's game/ */
static int extract_image(const char *image)
{
    SDL_IOStream *io = SDL_IOFromFile(image, "rb");
    if (!io) {
        say("Can't open that file", image);
        return 0;
    }
    Sint64 base = -1;
    Uint8 vd[XISO_SECTOR];
    for (size_t i = 0; i < SDL_arraysize(k_partitions) && base < 0; i++)
        if (SDL_SeekIO(io, k_partitions[i] + 32 * XISO_SECTOR, SDL_IO_SEEK_SET) >= 0
                && SDL_ReadIO(io, vd, sizeof vd) == sizeof vd && !SDL_memcmp(vd, "MICROSOFT*XBOX*MEDIA", 20))
            base = k_partitions[i];
    if (base < 0) {
        SDL_CloseIO(io);
        say("That isn't an Xbox disc image",
            "if it's in a .7z or .zip, unpack it first and choose the image inside (.iso, .xiso)");
        return 0;
    }
    Uint32 root = (Uint32)vd[20] | (Uint32)vd[21] << 8 | (Uint32)vd[22] << 16 | (Uint32)vd[23] << 24;
    Uint32 len = (Uint32)vd[24] | (Uint32)vd[25] << 8 | (Uint32)vd[26] << 16 | (Uint32)vd[27] << 24;
    Uint8 *tab = len ? SDL_malloc(len) : NULL;
    int ok = tab && SDL_SeekIO(io, base + (Sint64)root * XISO_SECTOR, SDL_IO_SEEK_SET) >= 0 && SDL_ReadIO(io, tab, len) == len;
    char game[1024];
    path(game, sizeof game, user, "game");
    say("Reading your disc image", "only the game's files are copied out");
    xiso_total = xiso_done = 0;
    set_progress(0);
    if (ok) {
        remove_tree(game);
        make_dirs(game);
        ok = xiso_walk(io, base, tab, len, 0, NULL, 0) && xiso_walk(io, base, tab, len, 0, game, 0);
    }
    SDL_free(tab);
    SDL_CloseIO(io);
    if (!ok) {
        say("Could not read the disc image", "the file may be damaged, or the disk full");
        return 0;
    }
    return 1;
}

/* what the prebuilt runtime needs of the system, said before minutes of work */
static int check_system(void)
{
    const char *v = gnu_get_libc_version();
    int maj = 0, min = 0;
    SDL_sscanf(v, "%d.%d", &maj, &min);
    if (maj < 2 || (maj == 2 && min < 38)) {
        char m[256];
        SDL_snprintf(m, sizeof m, "It needs glibc 2.38 or newer (Linux from 2023 on: Ubuntu 24.04, SteamOS 3.6 and so on); this "
                                  "system has %s.", v);
        say("This Linux is too old for this build", m);
        return 0;
    }
    return 1;
}

/* the files of one folder into another (not its subfolders) */
typedef struct { const char *from, *to; int ok, keep; } CopyDir;

static SDL_EnumerationResult copy_cb(void *ud, const char *dir, const char *name)
{
    CopyDir *c = ud;
    char s[1024], d[1024];
    SDL_PathInfo i;
    (void)dir;
    path(s, sizeof s, c->from, name);
    path(d, sizeof d, c->to, name);
    if (SDL_GetPathInfo(s, &i) && i.type == SDL_PATHTYPE_FILE && !SDL_CopyFile(s, d)) {
        c->ok = 0;
        return SDL_ENUM_FAILURE;
    }
    return SDL_ENUM_CONTINUE;
}

/* the generated C files */
#define MAX_PARTS 64
static char parts[MAX_PARTS][64];
static int nparts;

static SDL_EnumerationResult gen_cb(void *ud, const char *dir, const char *name)
{
    (void)ud, (void)dir;
    size_t k = SDL_strlen(name);
    if (k > 2 && !SDL_strcmp(name + k - 2, ".c") && k < sizeof parts[0] && nparts < MAX_PARTS)
        SDL_strlcpy(parts[nparts++], name, sizeof parts[0]);
    return SDL_ENUM_CONTINUE;
}

static int part_cmp(const void *a, const void *b)
{
    return SDL_strcmp((const char *)a, (const char *)b);
}

/* default.xbe to C, then compiled (several at once) and linked with the runtime */
static int build_game(void)
{
    char zig[1024], gen[1024], obj[1024], bin[1024], tool[1024], xbe[1024], inc[1024], bdir[1024], lib[1024], so[1024];
    path(zig, sizeof zig, bundle, "zig/zig");
    path(bdir, sizeof bdir, user, "build");
    path(gen, sizeof gen, user, "build/gen");
    path(obj, sizeof obj, user, "build/obj");
    path(bin, sizeof bin, user, "bin");
    path(tool, sizeof tool, bundle, "translate");
    path(xbe, sizeof xbe, user, "game/default.xbe");
    path(inc, sizeof inc, bundle, "include");
    path(lib, sizeof lib, bundle, "lib");
    path(so, sizeof so, bundle, "lib/so");
    remove_tree(bdir);                  /* (what a stopped run left) */
    make_dirs(obj);
    make_dirs(bin);
    char cache[1024];
    path(cache, sizeof cache, user, "build/zig-cache");
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "ZIG_GLOBAL_CACHE_DIR", cache, true);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "ZIG_LOCAL_CACHE_DIR", cache, true);

    say("Translating the game", "default.xbe to C, on this computer (about half a minute)");
    set_progress(-1);
    const char *targv[] = {tool, xbe, bdir, NULL};
    int tc = run(targv);
    if (tc == 3) {
        say("The translation doesn't match this download", "please report it (setup.log in the user folder)");
        return 0;
    }
    if (tc != 0) {
        say("Translating failed", last_output);
        return 0;
    }

    nparts = 0;
    SDL_EnumerateDirectory(gen, gen_cb, NULL);
    SDL_qsort(parts, (size_t)nparts, sizeof parts[0], part_cmp);    /* (the same game whatever order the disk lists them) */
    if (!nparts) {
        say("Translating failed", "it made no files");
        return 0;
    }

    int jobs = SDL_GetNumLogicalCPUCores();
    jobs = jobs > 2 ? jobs - 1 : 1;
    int ram = SDL_GetSystemRAM() / 1500;  /* (a part takes up to about 1.5 GB to compile) */
    if (jobs > ram)
        jobs = ram;
    if (SDL_getenv("WRECKLESS_JOBS"))   /* (fewer, to keep the computer free for other things) */
        jobs = SDL_atoi(SDL_getenv("WRECKLESS_JOBS"));
    if (jobs < 1)
        jobs = 1;
    if (jobs > 16)
        jobs = 16;
    SDL_Process *procs[16] = {0};
    int which[16];
    int next = 0, done = 0, failed = 0;
    static char objs[MAX_PARTS][1024];
    char i_game[1024], i_recomp[1024], i_ab[1024], i_src[1024], i_kern[1024], i_plat[1024], i_enh[1024], force[1024];
    path(i_game, sizeof i_game, inc, "game");
    path(i_recomp, sizeof i_recomp, inc, "game/recomp");
    path(i_ab, sizeof i_ab, inc, "antbox/include");
    path(i_src, sizeof i_src, inc, "antbox/src");
    path(i_kern, sizeof i_kern, inc, "antbox/src/kernel");
    path(i_plat, sizeof i_plat, inc, "antbox/src/platform");
    path(i_enh, sizeof i_enh, inc, "antbox/src/enhance");
    path(force, sizeof force, inc, "antbox/src/platform/xr_log.h");
    say("Building the game", "compiling the translated code (a minute or two, once)");
    while (done < nparts && !failed) {
        for (int j = 0; j < jobs && next < nparts; j++) {
            if (procs[j])
                continue;
            char src[1024];
            path(src, sizeof src, gen, parts[next]);
            SDL_snprintf(objs[next], sizeof objs[0], "%s/%.*s.o", obj, (int)(SDL_strlen(parts[next]) - 2), parts[next]);
            /* (the flags the check runs on: CMakeLists.txt's, for the generated code) */
            const char *argv[] = {zig, "cc", "-target", TARGET, "-std=gnu11", "-O2", "-w", "-DNDEBUG", "-DNOMINMAX",
                                  "-DRECOMP_ENV_HAVE_ENHANCE_KEYS", "-DRECOMP_ENV_HAVE_GAME_KEYS",
                                  "-DWIN32_LEAN_AND_MEAN", "-DWR_HAVE_ENHANCE", "-DXBOXRECOMP_HAVE_OVERLAY=1",
                                  "-DXBOXRECOMP_HAVE_VULKAN=1", "-D_GNU_SOURCE", "-include", force,
                                  "-I", i_game, "-I", i_recomp, "-I", gen, "-I", i_ab, "-I", i_src, "-I", i_kern,
                                  "-I", i_plat, "-I", i_enh, "-c", src, "-o", objs[next], NULL};
            SDL_PropertiesID pr = SDL_CreateProperties();
            SDL_SetPointerProperty(pr, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)argv);
            SDL_SetNumberProperty(pr, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
            SDL_SetBooleanProperty(pr, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
            procs[j] = SDL_CreateProcessWithProperties(pr);
            SDL_DestroyProperties(pr);
            if (!procs[j]) {
                SDL_snprintf(last_output, sizeof last_output, "cannot start the compiler: %s", SDL_GetError());
                failed = 1;
                break;
            }
            which[j] = next++;
        }
        for (int j = 0; j < jobs; j++) {
            int code;
            if (procs[j] && SDL_WaitProcess(procs[j], false, &code)) {
                size_t n;
                char *out = SDL_ReadProcess(procs[j], &n, NULL);
                if (code != 0) {
                    SDL_snprintf(last_output, sizeof last_output, "%s: %s", parts[which[j]], out ? out : "");
                    failed = 1;
                }
                SDL_free(out);
                SDL_DestroyProcess(procs[j]);
                procs[j] = NULL;
                done++;
                set_progress(0.9 * done / nparts);
                char m[256];
                SDL_snprintf(m, sizeof m, "compiled %d of %d parts", done, nparts);
                say(NULL, m);
            }
        }
        SDL_Delay(50);
    }
    for (int j = 0; j < jobs; j++)
        if (procs[j]) {
            SDL_WaitProcess(procs[j], true, NULL);
            SDL_DestroyProcess(procs[j]);
        }
    if (failed) {
        say("Compiling failed", last_output);
        return 0;
    }

    say("Building the game", "linking it with the runtime");
    /* the libraries the download carries go beside the game, so it runs without the download mounted */
    char libs[1024], out[1024], toml[1024], btoml[1024];
    path(libs, sizeof libs, bin, "lib");
    make_dirs(libs);
    CopyDir cd = {so, libs, 1, 0};
    SDL_EnumerateDirectory(so, copy_cb, &cd);
    if (!cd.ok) {
        say("Could not copy the game's libraries", SDL_GetError());
        return 0;
    }
    path(out, sizeof out, bin, "wreckless");
    /* lib/link.txt: what comes after the translated code, in order; a line without a slash is a linker flag */
    char lp[1024];
    path(lp, sizeof lp, lib, "link.txt");
    char *lt = read_text(lp);
    if (!lt) {
        say("The download is incomplete", "lib/link.txt is missing");
        return 0;
    }
    static const char *largv[256];
    static char lpaths[128][1024];
    int k = 0, np = 0;
    largv[k++] = zig, largv[k++] = "cc", largv[k++] = "-target", largv[k++] = TARGET;
    largv[k++] = "-rdynamic", largv[k++] = "-o", largv[k++] = out;
    for (int i = 0; i < nparts; i++)
        largv[k++] = objs[i];
    char *save = NULL;
    for (char *w = SDL_strtok_r(lt, "\r\n", &save); w && k < 240 && np < 128; w = SDL_strtok_r(NULL, "\r\n", &save)) {
        if (w[0] == '-')
            largv[k++] = w;
        else {
            path(lpaths[np], sizeof lpaths[0], lib, w);
            largv[k++] = lpaths[np++];
        }
    }
    largv[k] = NULL;
    int lc = run(largv);
    SDL_free(lt);
    if (lc != 0) {
        say("Linking failed", last_output);
        return 0;
    }
    /* the default settings, beside the game; the player's are never overwritten */
    path(toml, sizeof toml, bin, "enhance.toml");
    path(btoml, sizeof btoml, bundle, "enhance.toml");
    if (!exists(toml))
        SDL_CopyFile(btoml, toml);
    /* the system has what the game's libraries need? Said here, as the game has no window to say it in */
    char sdl2[1024], dep[1024];
    static const char *const deps[] = {"libsamplerate.so.0", "libdecor-0.so.0"};    /* (SDL2's, from the download) */
    for (size_t i = 0; i < SDL_arraysize(deps); i++) {
        path(dep, sizeof dep, libs, deps[i]);
        dlopen(dep, RTLD_LAZY | RTLD_LOCAL);
    }
    path(sdl2, sizeof sdl2, libs, "libSDL2-2.0.so.0");
    void *h = dlopen(sdl2, RTLD_LAZY | RTLD_LOCAL);
    if (!h) {
        const char *e = dlerror();
        say("This system is missing a library the game needs", e ? e : "");
        return 0;
    }
    dlclose(h);
    remove_tree(bdir);                  /* (the translation is only needed to build) */
    char stamp[1024];
    path(stamp, sizeof stamp, bin, "VERSION");
    write_text(stamp, version);
    set_progress(1);
    say("Built", "Wreckless: Recompiled is ready");
    return 1;
}

static char source[1024];              /* what the player chose */

static int setup_task(void *unused)
{
    (void)unused;
    char dir[1024], game[1024], mark[1024];
    path(game, sizeof game, user, "game");
    path(mark, sizeof mark, game, ".disc-checked");
    int ok = check_system();
    if (ok && source[0]) {
        SDL_PathInfo i;
        if (!SDL_GetPathInfo(source, &i)) {
            say("Can't find that", source);
            ok = 0;
        } else if (i.type == SDL_PATHTYPE_FILE) {
            ok = extract_image(source) && check_and_copy(game);
        } else if (find_game(source, dir, sizeof dir)) {
            ok = check_and_copy(dir);
        } else {
            say("That folder doesn't hold the game", "there is no default.xbe in it");
            ok = 0;
        }
    } else if (ok && !exists(mark))
        ok = check_and_copy(game);
    if (ok)
        ok = build_game();
    task_ok = ok;
    task_done = 1;
    return 0;
}

static void start(SDL_ThreadFunction fn)
{
    task_done = 0;
    task_ok = 1;
    SDL_Thread *t = SDL_CreateThread(fn, "setup", NULL);
    SDL_DetachThread(t);
}

static int built(void)
{
    char stamp[1024];
    path(stamp, sizeof stamp, user, "bin/VERSION");
    char *s = read_text(stamp);
    int ok = s && !SDL_strcmp(s, version);
    SDL_free(s);
    char g[1024];
    path(g, sizeof g, user, "bin/wreckless");
    return ok && exists(g);
}

static void launch_game(int argc, char **argv)
{
    char g[1024];
    path(g, sizeof g, user, "bin/wreckless");
    const char *args[64];
    int k = 0;
    char gd[1024];
    path(gd, sizeof gd, user, "game");
    args[k++] = g, args[k++] = "--game-dir", args[k++] = gd, args[k++] = "--save-dir", args[k++] = user;
    for (int i = 1; i < argc && k < 62; i++)
        if (SDL_strcmp(argv[i], "--setup"))
            args[k++] = argv[i];
    args[k] = NULL;
    /* (the game's own path to its libraries doesn't reach the libraries' own: SDL2's are there too) */
    char ld[1024];
    path(ld, sizeof ld, user, "bin/lib");
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "LD_LIBRARY_PATH", ld, true);
    SDL_PropertiesID pr = SDL_CreateProperties();
    SDL_SetPointerProperty(pr, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
    SDL_SetBooleanProperty(pr, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
    SDL_Process *p = SDL_CreateProcessWithProperties(pr);
    SDL_DestroyProperties(pr);
    if (p)
        SDL_DestroyProcess(p);
}

/* the copy a run before this one kept */
static int detect(char *out, size_t n)
{
    char g[1024];
    path(g, sizeof g, user, "game");
    return find_game(g, out, n);
}

/* ------------------------------------------------------------------ drawing */
static SDL_Window *win;
static SDL_Renderer *ren;
static FT_Library ft;
static FT_Face face;
static float scale = 1;

typedef struct { char text[256]; int size; Uint32 color; SDL_Texture *tex; int w, h; Uint64 used; } TextTex;
static TextTex cache[96];
static Uint64 frame;

static SDL_Texture *text_tex(const char *s, int size, Uint32 rgb, int *w, int *h)
{
    for (int i = 0; i < 96; i++)
        if (cache[i].tex && cache[i].size == size && cache[i].color == rgb && !SDL_strcmp(cache[i].text, s)) {
            cache[i].used = frame;
            *w = cache[i].w, *h = cache[i].h;
            return cache[i].tex;
        }
    int slot = 0;
    for (int i = 1; i < 96; i++)
        if (!cache[i].tex || cache[i].used < cache[slot].used)
            slot = i;
    if (cache[slot].tex)
        SDL_DestroyTexture(cache[slot].tex);
    FT_Set_Pixel_Sizes(face, 0, (FT_UInt)size);
    int asc = (int)(face->size->metrics.ascender >> 6), desc = (int)(-face->size->metrics.descender >> 6);
    int width = 0;
    for (const char *c = s; *c; c++)
        if (!FT_Load_Char(face, (unsigned char)*c, FT_LOAD_DEFAULT))
            width += (int)(face->glyph->advance.x >> 6);
    int ht = asc + desc;
    if (width < 1)
        width = 1;
    Uint8 *px = SDL_calloc((size_t)width * ht, 4);
    int pen = 0;
    for (const char *c = s; *c; c++) {
        if (FT_Load_Char(face, (unsigned char)*c, FT_LOAD_RENDER))
            continue;
        FT_Bitmap *b = &face->glyph->bitmap;
        int x0 = pen + face->glyph->bitmap_left, y0 = asc - face->glyph->bitmap_top;
        for (unsigned r = 0; r < b->rows; r++)
            for (unsigned k = 0; k < b->width; k++) {
                int x = x0 + (int)k, y = y0 + (int)r;
                if (x < 0 || y < 0 || x >= width || y >= ht)
                    continue;
                Uint8 *o = px + ((size_t)y * width + x) * 4;
                Uint8 a = b->buffer[r * (unsigned)b->pitch + k];
                if (a > o[3])
                    o[0] = (Uint8)(rgb >> 16), o[1] = (Uint8)(rgb >> 8), o[2] = (Uint8)rgb, o[3] = a;
            }
        pen += (int)(face->glyph->advance.x >> 6);
    }
    SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, width, ht);
    SDL_UpdateTexture(t, NULL, px, width * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_free(px);
    SDL_strlcpy(cache[slot].text, s, sizeof cache[slot].text);
    cache[slot].size = size, cache[slot].color = rgb, cache[slot].tex = t, cache[slot].w = width, cache[slot].h = ht;
    cache[slot].used = frame;
    *w = width, *h = ht;
    return t;
}

static float text(float x, float y, const char *s, int size, Uint32 rgb)
{
    if (!*s)
        return 0;
    int w, h;
    SDL_Texture *t = text_tex(s, (int)(size * scale), rgb, &w, &h);
    SDL_FRect d = {x * scale, y * scale, (float)w, (float)h};
    SDL_RenderTexture(ren, t, NULL, &d);
    return (float)w / scale;
}

/* words wrapped to width; returns the height used */
static float text_wrapped(float x, float y, float width, const char *s, int size, Uint32 rgb)
{
    char line[512] = "", word[256];
    float yy = y;
    const char *p = s;
    while (*p) {
        size_t k = strcspn(p, " \n");
        if (k >= sizeof word)
            k = sizeof word - 1;
        SDL_memcpy(word, p, k);
        word[k] = 0;
        char test[768];
        SDL_snprintf(test, sizeof test, "%s%s%s", line, *line ? " " : "", word);
        int w, h;
        text_tex(test, (int)(size * scale), rgb, &w, &h);
        if (w / scale > width && *line) {
            text(x, yy, line, size, rgb);
            yy += size * 1.35f;
            SDL_strlcpy(line, word, sizeof line);
        } else
            SDL_strlcpy(line, test, sizeof line);
        p += k;
        if (*p == '\n') {
            text(x, yy, line, size, rgb);
            yy += size * 1.35f;
            line[0] = 0;
        }
        if (*p)
            p++;
    }
    if (*line) {
        text(x, yy, line, size, rgb);
        yy += size * 1.35f;
    }
    return yy - y;
}

static void rect(float x, float y, float w, float h, Uint32 rgb, Uint8 a)
{
    SDL_SetRenderDrawColor(ren, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb, a);
    SDL_FRect r = {x * scale, y * scale, w * scale, h * scale};
    SDL_RenderFillRect(ren, &r);
}

/* ------------------------------------------------------------------ the pages */
enum { P_WELCOME, P_WORK, P_DONE, P_ERROR };
enum { B_FOUND, B_IMAGE, B_FOLDER, B_PLAY, B_QUIT, B_BACK };
static int page = P_WELCOME, sel = 0;
static char found[1024];
static int have_found;
static int buttons[8], nbuttons;
static SDL_FRect button_rects[8];
static volatile int dialog_done;
static char dialog_result[1024];

static void dialog_cb(void *ud, const char *const *files, int filter)
{
    (void)ud, (void)filter;
    if (files && files[0])
        SDL_strlcpy(dialog_result, files[0], sizeof dialog_result);
    dialog_done = 1;
}

static const char *button_label(int b)
{
    switch (b) {
    case B_FOUND: return "Use the copy found here";
    case B_IMAGE: return "Choose your disc image (.iso)";
    case B_FOLDER: return "Choose a folder of the disc's files";
    case B_PLAY: return "Play";
    case B_QUIT: return "Quit";
    case B_BACK: return "Back";
    }
    return "";
}

static void press(int b)
{
    switch (b) {
    case B_FOUND:
        SDL_strlcpy(source, found, sizeof source);
        page = P_WORK;
        start(setup_task);
        break;
    case B_IMAGE: {
        static const SDL_DialogFileFilter f[] = {{"Xbox disc image", "iso;xiso;img"}, {"All files", "*"}};
        dialog_done = 0, dialog_result[0] = 0;
        SDL_ShowOpenFileDialog(dialog_cb, NULL, win, f, 2, NULL, false);
        break;
    }
    case B_FOLDER:
        dialog_done = 0, dialog_result[0] = 0;
        SDL_ShowOpenFolderDialog(dialog_cb, NULL, win, NULL, false);
        break;
    case B_BACK:
        page = P_WELCOME;
        break;
    }
}

static void layout(void)
{
    nbuttons = 0;
    if (page == P_WELCOME) {
        if (have_found)
            buttons[nbuttons++] = B_FOUND;
        buttons[nbuttons++] = B_IMAGE;
        buttons[nbuttons++] = B_FOLDER;
        buttons[nbuttons++] = B_QUIT;
    } else if (page == P_DONE) {
        buttons[nbuttons++] = B_PLAY;
        buttons[nbuttons++] = B_QUIT;
    } else if (page == P_ERROR) {
        buttons[nbuttons++] = B_BACK;
        buttons[nbuttons++] = B_QUIT;
    }
    if (sel >= nbuttons)
        sel = nbuttons - 1;
    if (sel < 0)
        sel = 0;
}

static void draw(void)
{
    frame++;
    int ww, wh;
    SDL_GetWindowSizeInPixels(win, &ww, &wh);
    scale = (float)wh / 600.0f;
    float W = ww / scale;
    SDL_SetRenderDrawColor(ren, 10, 14, 28, 255);
    SDL_RenderClear(ren);
    rect(0, 0, W, 4, 0xf0d020, 255);
    float tw = text(48, 36, "Wreckless: Recompiled", 40, 0xf8d830);
    text(48 + tw + 18, 56, release, 18, 0x9098b0);
    char st[256], de[1024];
    double pr;
    SDL_LockMutex(lock);
    SDL_strlcpy(st, status, sizeof st);
    SDL_strlcpy(de, detail, sizeof de);
    pr = progress;
    SDL_UnlockMutex(lock);
    float y = 112;
    if (page == P_WELCOME) {
        y += text_wrapped(48, y, W - 96,
                          "The Xbox game from 2002, rebuilt as a native Linux program. It needs your own copy of the "
                          "US disc: this builds the game from it, once, on this computer.",
                          20, 0xe8e8f0);
        y += 12;
        if (have_found) {
            char m[1200];
            SDL_snprintf(m, sizeof m, "Found: %s", found);
            y += text_wrapped(48, y, W - 96, m, 16, 0x9ad0ff);
        } else
            y += text_wrapped(48, y, W - 96, "Show it where your copy is: a disc image of the game, or a folder of the "
                                             "disc's files. It takes about 2 GB and a few minutes.", 18, 0xb8b8c8);
    } else if (page == P_WORK || page == P_DONE || page == P_ERROR) {
        text(48, y, st, 26, page == P_ERROR ? 0xff7060 : 0xffffff);
        y += 44;
        y += text_wrapped(48, y, W - 96, de, 17, 0xb8b8c8);
        if (page == P_WORK && pr >= 0) {
            rect(48, 300, W - 96, 14, 0x303850, 255);
            rect(48, 300, (float)((W - 96) * pr), 14, 0xf0d020, 255);
        } else if (page == P_WORK) {
            float t = (float)(SDL_GetTicks() % 1600) / 1600.0f;
            rect(48, 300, W - 96, 14, 0x303850, 255);
            rect(48 + (W - 96 - 120) * t, 300, 120, 14, 0xf0d020, 255);
        }
    }
    layout();
    float by = 600 - 64.0f * nbuttons - 24;
    for (int i = 0; i < nbuttons; i++) {
        int on = i == sel;
        rect(48, by, W - 96, 52, on ? 0x2a3860 : 0x161c34, 255);
        if (on)
            rect(48, by, 6, 52, 0xf0d020, 255);
        text(72, by + 12, button_label(buttons[i]), 22, on ? 0xf8d830 : 0xe0e0e8);
        button_rects[i] = (SDL_FRect){48, by, W - 96, 52};
        by += 64;
    }
    SDL_RenderPresent(ren);
}

/* ------------------------------------------------------------------ main */
static void find_folders(void)
{
    const char *b = SDL_getenv("WRECKLESS_BUNDLE");
    if (b)
        SDL_strlcpy(bundle, b, sizeof bundle);
    else
        SDL_snprintf(bundle, sizeof bundle, "%s../lib/wreckless-recompiled", SDL_GetBasePath());
    const char *u = SDL_getenv("WRECKLESS_USER");
    if (u)
        SDL_strlcpy(user, u, sizeof user);
    else {
        const char *x = SDL_getenv("XDG_DATA_HOME");
        if (x && *x)
            SDL_snprintf(user, sizeof user, "%s/wreckless", x);
        else
            SDL_snprintf(user, sizeof user, "%s/.local/share/wreckless", SDL_getenv("HOME"));
    }
    make_dirs(user);
    char vp[1024];
    path(vp, sizeof vp, bundle, "VERSION");
    char *v = read_text(vp);
    SDL_strlcpy(version, v ? v : "unknown", sizeof version);
    SDL_free(v);
    for (char *c = version; *c; c++)
        if (*c == '\n' || *c == '\r')
            *c = 0;
    const char *r = SDL_strchr(version, ' ');
    if (r && r[1] >= '0' && r[1] <= '9') {
        SDL_strlcpy(release, r + 1, sizeof release);
        char *e = SDL_strchr(release, ' ');
        if (e)
            *e = 0;
    }
}

int main(int argc, char **argv)
{
    lock = SDL_CreateMutex();
    find_folders();
    int force = 0, no_launch = 0;
    const char *from = NULL;
    for (int i = 1; i < argc; i++) {
        if (!SDL_strcmp(argv[i], "--setup"))
            force = 1;
        else if (!SDL_strcmp(argv[i], "--setup-from") && i + 1 < argc)
            from = argv[++i];
        else if (!SDL_strcmp(argv[i], "--no-launch"))
            no_launch = 1;
    }
    if (from) {                          /* in the terminal */
        headless = 1;
        SDL_strlcpy(source, from, sizeof source);
        setup_task(NULL);
        if (!task_ok) {
            SDL_Log("setup failed: %s: %s", status, detail);
            return 1;
        }
        if (!no_launch)
            launch_game(1, argv);
        return 0;
    }
    if (!force && built()) {
        launch_game(argc, argv);
        return 0;
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL: %s", SDL_GetError());
        return 1;
    }
    if (FT_Init_FreeType(&ft) || FT_New_Memory_Face(ft, k_overlay_font_ui, (FT_Long)sizeof k_overlay_font_ui, 0, &face)) {
        SDL_Log("FreeType could not open the font");
        return 1;
    }
    win = SDL_CreateWindow("Wreckless: Recompiled", 1000, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    ren = SDL_CreateRenderer(win, NULL);
    SDL_SetRenderVSync(ren, 1);
    have_found = detect(found, sizeof found);
    int quit = 0, was_working = 0;
    /* WRECKLESS_LAUNCHER_SHOT=FILE saves each page it shows as FILE-<page>.bmp and quits on the first, or with
       WRECKLESS_LAUNCHER_PRESS=N (the Nth button of the first page) when the work it starts has ended. */
    const char *shot = SDL_getenv("WRECKLESS_LAUNCHER_SHOT"), *auto_press = SDL_getenv("WRECKLESS_LAUNCHER_PRESS");
    int shot_page = -1, page_frames = 0;
    while (!quit) {
        SDL_Event e;
        if (auto_press && frame == 5) {
            SDL_Event a = {0};
            a.type = SDL_EVENT_USER;
            SDL_PushEvent(&a);
        }
        while (SDL_PollEvent(&e)) {
            int act = -1;
            if (e.type == SDL_EVENT_USER && auto_press)
                act = SDL_atoi(auto_press);
            switch (e.type) {
            case SDL_EVENT_QUIT: quit = 1; break;
            case SDL_EVENT_GAMEPAD_ADDED: SDL_OpenGamepad(e.gdevice.which); break;
            case SDL_EVENT_KEY_DOWN:
                if (e.key.key == SDLK_UP) sel--;
                else if (e.key.key == SDLK_DOWN) sel++;
                else if (e.key.key == SDLK_RETURN || e.key.key == SDLK_SPACE) act = sel;
                else if (e.key.key == SDLK_ESCAPE && page != P_WORK) quit = 1;
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_UP) sel--;
                else if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_DOWN) sel++;
                else if (e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH || e.gbutton.button == SDL_GAMEPAD_BUTTON_START) act = sel;
                break;
            case SDL_EVENT_MOUSE_MOTION:
                for (int i = 0; i < nbuttons; i++) {
                    SDL_FRect r = button_rects[i];
                    float x = e.motion.x / scale, y = e.motion.y / scale;
                    if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
                        sel = i;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                float x = e.button.x / scale, y = e.button.y / scale;
                for (int i = 0; i < nbuttons; i++) {
                    SDL_FRect r = button_rects[i];
                    if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
                        act = i;
                }
                break;
            }
            }
            if (act >= 0 && act < nbuttons && page != P_WORK) {
                int b = buttons[act];
                if (b == B_QUIT)
                    quit = 1;
                else if (b == B_PLAY) {
                    launch_game(argc, argv);
                    quit = 1;
                } else
                    press(b);
            }
        }
        if (dialog_done) {                /* a file or folder was chosen */
            dialog_done = 0;
            if (dialog_result[0]) {
                SDL_strlcpy(source, dialog_result, sizeof source);
                page = P_WORK;
                start(setup_task);
            }
        }
        if (page == P_WORK) {
            was_working = 1;
            if (task_done)
                page = task_ok ? P_DONE : P_ERROR, sel = 0;
        } else if (was_working && page != P_WORK)
            was_working = 0;
        draw();
        if (shot) {
            if (page != shot_page)
                shot_page = page, page_frames = 0;
            if (++page_frames == (SDL_getenv("WRECKLESS_LAUNCHER_SHOT_AT") ? SDL_atoi(SDL_getenv("WRECKLESS_LAUNCHER_SHOT_AT")) : 20)) {
                char f[1100];
                SDL_snprintf(f, sizeof f, "%s-%d.bmp", shot, page);
                SDL_Surface *sf = SDL_RenderReadPixels(ren, NULL);
                if (sf) {
                    SDL_SaveBMP(sf, f);
                    SDL_DestroySurface(sf);
                }
                if (!auto_press || page == P_DONE || page == P_ERROR)
                    quit = 1;
            }
        }
    }
    SDL_Quit();
    return 0;
}
