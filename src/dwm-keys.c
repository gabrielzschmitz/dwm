/*
 * Generate dwm-keys(1) from the Key and Button arrays in config.h.
 *
 * The configuration is preprocessed so helper macros such as TAGKEYS and
 * SHCMD are expanded, while X keysym names stay symbolic.  The resulting
 * token stream is parsed structurally; action descriptions are read from the
 * Doxygen @brief comments in the C sources.
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
  char* s;
  size_t len;
  size_t cap;
} Buf;

typedef struct {
  char** v;
  size_t n;
  size_t cap;
} StrList;

typedef struct {
  char* mod;
  char* key;
  char* fn;
  char* arg;
  char* group;
} KeyEnt;

typedef struct {
  char* click;
  char* mask;
  char* button;
  char* fn;
  char* arg;
} ButtonEnt;

typedef struct {
  char* name;
  char* brief;
} FuncBrief;

static KeyEnt* keys;
static size_t nkeys, capkeys;
static ButtonEnt* buttons;
static size_t nbuttons, capbuttons;
static char** tag_labels;
static size_t ntags, captags;
static char** layout_symbol;
static char** layout_func;
static size_t nlayouts, caplayouts;
static char** scratch_name;
static size_t nscratch, capscratch;
static FuncBrief* briefs;
static size_t nbriefs, capbriefs;

static void die(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fputs("dwm-keys: ", stderr);
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  va_end(ap);
  exit(1);
}

static void* xmalloc(size_t n) {
  void* p = malloc(n ? n : 1);
  if (!p) die("out of memory");
  return p;
}

static void* xrealloc(void* p, size_t n) {
  void* q = realloc(p, n ? n : 1);
  if (!q) die("out of memory");
  return q;
}

static char* xstrdup(const char* s) {
  size_t n = strlen(s) + 1;
  char* p = xmalloc(n);
  memcpy(p, s, n);
  return p;
}

static char* xstrndup(const char* s, size_t n) {
  char* p = xmalloc(n + 1);
  memcpy(p, s, n);
  p[n] = '\0';
  return p;
}

static void buf_init(Buf* b) {
  b->cap = 256;
  b->len = 0;
  b->s = xmalloc(b->cap);
  b->s[0] = '\0';
}

static void buf_reserve(Buf* b, size_t extra) {
  if (b->len + extra + 1 <= b->cap) return;
  while (b->len + extra + 1 > b->cap) b->cap *= 2;
  b->s = xrealloc(b->s, b->cap);
}

static void buf_addn(Buf* b, const char* s, size_t n) {
  buf_reserve(b, n);
  memcpy(b->s + b->len, s, n);
  b->len += n;
  b->s[b->len] = '\0';
}

static void buf_add(Buf* b, const char* s) { buf_addn(b, s, strlen(s)); }

static void buf_addc(Buf* b, char c) { buf_addn(b, &c, 1); }

static void buf_clear(Buf* b) {
  b->len = 0;
  b->s[0] = '\0';
}

static void buf_free(Buf* b) {
  free(b->s);
  b->s = NULL;
  b->len = b->cap = 0;
}

static void sl_push(StrList* l, char* s) {
  if (l->n == l->cap) {
    l->cap = l->cap ? l->cap * 2 : 8;
    l->v = xrealloc(l->v, l->cap * sizeof(*l->v));
  }
  l->v[l->n++] = s;
}

static void sl_free(StrList* l) {
  size_t i;
  for (i = 0; i < l->n; i++) free(l->v[i]);
  free(l->v);
  l->v = NULL;
  l->n = l->cap = 0;
}

static char* read_file(const char* path) {
  FILE* f = fopen(path, "rb");
  Buf b;
  char tmp[8192];
  size_t n;

  if (!f) die("cannot open %s: %s", path, strerror(errno));
  buf_init(&b);
  while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0) buf_addn(&b, tmp, n);
  if (ferror(f)) die("cannot read %s: %s", path, strerror(errno));
  fclose(f);
  return b.s;
}

static char* slurp_stream(FILE* f) {
  Buf b;
  char tmp[8192];
  size_t n;

  buf_init(&b);
  while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0) buf_addn(&b, tmp, n);
  if (ferror(f)) die("read error: %s", strerror(errno));
  return b.s;
}

static char* trim(char* s) {
  char* end;
  while (*s && isspace((unsigned char)*s)) s++;
  end = s + strlen(s);
  while (end > s && isspace((unsigned char)end[-1])) end--;
  *end = '\0';
  return s;
}

static char* ucfirst(char* s) {
  if (s && *s) *s = (char)toupper((unsigned char)*s);
  return s;
}

static char* esc(const char* s) {
  Buf b;
  const char* p;
  buf_init(&b);
  for (p = s; *p; p++) {
    if (*p == '\\') buf_addc(&b, '\\');
    buf_addc(&b, *p);
  }
  return b.s;
}

static bool starts_with(const char* s, const char* prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

static char* defname(const char* line) {
  const char* p;
  char *s, *t;

  s = xstrdup(line);
  t = trim(s);
  if (*t == '#' || *t == '\0' || strchr(t, '(') == NULL) {
    free(s);
    return NULL;
  }
  t = t + strlen(t);
  while (t > s && isspace((unsigned char)t[-1])) t--;
  if (t > s && t[-1] == ';') {
    free(s);
    return NULL;
  }

  t = trim(s);
  for (p = t; *p; p++) {
    if (isalpha((unsigned char)*p) || *p == '_') {
      const char* start = p;
      char* name;
      while (isalnum((unsigned char)*p) || *p == '_') p++;
      name = xstrndup(start, (size_t)(p - start));
      while (*p && isspace((unsigned char)*p)) p++;
      if (*p == '(') {
        free(s);
        return name;
      }
      free(name);
      if (!*p) break;
      p--;
    }
  }
  free(s);
  return NULL;
}

static void brief_add(const char* name, const char* brief) {
  size_t i;
  for (i = 0; i < nbriefs; i++)
    if (strcmp(briefs[i].name, name) == 0) return;
  if (nbriefs == capbriefs) {
    capbriefs = capbriefs ? capbriefs * 2 : 16;
    briefs = xrealloc(briefs, capbriefs * sizeof(*briefs));
  }
  briefs[nbriefs].name = xstrdup(name);
  briefs[nbriefs].brief = xstrdup(brief);
  nbriefs++;
}

static const char* brief_for(const char* name) {
  size_t i;
  for (i = 0; i < nbriefs; i++)
    if (strcmp(briefs[i].name, name) == 0) return briefs[i].brief;
  return "";
}

static bool is_blank(const char* line) {
  while (*line) {
    if (!isspace((unsigned char)*line)) return false;
    line++;
  }
  return true;
}

static void parse_source_text(const char* text) {
  bool in_doc = false, in_brief = false;
  char* brief = xstrdup("");
  char* pending = xstrdup("");
  const char* line = text;

  while (*line) {
    const char* end = strchr(line, '\n');
    size_t len = end ? (size_t)(end - line) : strlen(line);
    char* raw = xstrndup(line, len);
    char* doc = strstr(raw, "/**");
    char* brief_mark;

    if (!in_doc) {
      if (doc) {
        in_doc = true;
        free(brief);
        brief = xstrdup("");
        in_brief = false;
      } else if (*pending) {
        if (!is_blank(raw)) {
          char* name = defname(raw);
          if (name) {
            brief_add(name, pending);
            free(name);
          }
          free(pending);
          pending = xstrdup("");
        }
      }
      free(raw);
      line = end ? end + 1 : line + len;
      continue;
    }

    brief_mark = strstr(raw, "@brief");
    if (brief_mark &&
        (brief_mark[6] == '\0' || isspace((unsigned char)brief_mark[6]))) {
      char* p = brief_mark + 6;
      char* close;
      while (*p && isspace((unsigned char)*p)) p++;
      close = strstr(p, "*/");
      if (close) *close = '\0';
      {
        char* q = p;
        while (*q && isspace((unsigned char)*q)) q++;
        if (*q == '*') {
          q++;
          while (*q && isspace((unsigned char)*q)) q++;
        }
        {
          char* val = trim(xstrdup(q));
          free(brief);
          brief = xstrdup(val);
          free(val);
        }
      }
      in_brief = true;
    } else if (in_brief) {
      char* at = strchr(raw, '@');
      if ((at && isalpha((unsigned char)at[1])) || strstr(raw, "*/")) {
        in_brief = false;
      } else {
        char* p = trim(raw);
        if (*p == '*') {
          p++;
          while (*p && isspace((unsigned char)*p)) p++;
        }
        p = trim(p);
        if (*p) {
          Buf nb;
          buf_init(&nb);
          buf_add(&nb, brief);
          buf_addc(&nb, ' ');
          buf_add(&nb, p);
          free(brief);
          brief = nb.s;
        }
      }
    }

    if (strstr(raw, "*/")) {
      in_doc = false;
      in_brief = false;
      free(pending);
      pending = xstrdup(brief);
    }
    free(raw);
    line = end ? end + 1 : line + len;
  }

  free(brief);
  free(pending);
}

static int cmp_str(const void* a, const void* b) {
  const char* const* sa = a;
  const char* const* sb = b;
  return strcmp(*sa, *sb);
}

static char* dirname_of(const char* path) {
  const char* slash = strrchr(path, '/');
  if (!slash) return xstrdup(".");
  if (slash == path) return xstrdup("/");
  return xstrndup(path, (size_t)(slash - path));
}

static char* join_path(const char* dir, const char* name) {
  Buf b;
  buf_init(&b);
  buf_add(&b, dir);
  if (b.len && b.s[b.len - 1] != '/') buf_addc(&b, '/');
  buf_add(&b, name);
  return b.s;
}

static void load_source_briefs(const char* dir) {
  DIR* d = opendir(dir);
  struct dirent* de;
  StrList files = {0};
  size_t i;

  if (!d) die("cannot open directory %s: %s", dir, strerror(errno));
  while ((de = readdir(d)) != NULL) {
    size_t n = strlen(de->d_name);
    if (n > 2 && strcmp(de->d_name + n - 2, ".c") == 0)
      sl_push(&files, xstrdup(de->d_name));
  }
  closedir(d);
  qsort(files.v, files.n, sizeof(*files.v), cmp_str);
  for (i = 0; i < files.n; i++) {
    char* path = join_path(dir, files.v[i]);
    char* text = read_file(path);
    parse_source_text(text);
    free(text);
    free(path);
  }
  sl_free(&files);
}

static char* read_version(const char* dir) {
  char* path = join_path(dir, "config.mk");
  char* text = read_file(path);
  const char* line = text;
  char* result = NULL;

  while (*line) {
    const char* end = strchr(line, '\n');
    size_t len = end ? (size_t)(end - line) : strlen(line);
    char* raw = xstrndup(line, len);
    char* p = trim(raw);
    if (starts_with(p, "VERSION")) {
      p += 7;
      while (*p && isspace((unsigned char)*p)) p++;
      if (*p == '=') {
        p++;
        result = xstrdup(trim(p));
        free(raw);
        break;
      }
    }
    free(raw);
    line = end ? end + 1 : line + len;
  }
  free(text);
  free(path);
  return result;
}

static char* strip_includes(const char* text) {
  Buf out;
  const char* line = text;

  buf_init(&out);
  while (*line) {
    const char* end = strchr(line, '\n');
    size_t len = end ? (size_t)(end - line) : strlen(line);
    char* raw = xstrndup(line, len);
    char* p = raw;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '#') {
      p++;
      while (*p == ' ' || *p == '\t') p++;
      if (starts_with(p, "include")) goto skip;
    }
    buf_addn(&out, line, len);
    buf_addc(&out, '\n');
  skip:
    free(raw);
    line = end ? end + 1 : line + len;
  }
  return out.s;
}

static char* preprocess_config(const char* config, const char* dir,
                               const char* cc) {
  char inpath[] = "/tmp/dwm-keys-in.XXXXXX";
  char outpath[] = "/tmp/dwm-keys-out.XXXXXX";
  int infd, outfd;
  FILE* f;
  char* stripped = strip_includes(config);
  char* result;
  pid_t pid;
  int status;
  Buf cmd;

  (void)dir;
  infd = mkstemp(inpath);
  if (infd < 0) die("cannot create temporary file: %s", strerror(errno));
  outfd = mkstemp(outpath);
  if (outfd < 0) die("cannot create temporary file: %s", strerror(errno));
  f = fdopen(infd, "wb");
  if (!f) die("fdopen: %s", strerror(errno));
  fwrite(stripped, 1, strlen(stripped), f);
  fflush(f);
  fseek(f, 0, SEEK_SET);
  free(stripped);

  pid = fork();
  if (pid < 0) die("fork: %s", strerror(errno));
  if (pid == 0) {
    if (dup2(infd, STDIN_FILENO) < 0) _exit(1);
    if (dup2(outfd, STDOUT_FILENO) < 0) _exit(1);
    close(infd);
    close(outfd);
    buf_init(&cmd);
    buf_add(&cmd, cc ? cc : "cc");
    buf_add(&cmd, " -E -C -nostdinc -x c -");
    execl("/bin/sh", "sh", "-c", cmd.s, (char*)NULL);
    _exit(127);
  }
  fclose(f);
  close(outfd);
  if (waitpid(pid, &status, 0) < 0) die("waitpid: %s", strerror(errno));
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    die("C preprocessor failed");
  f = fopen(outpath, "rb");
  if (!f) die("cannot open %s: %s", outpath, strerror(errno));
  {
    char* pre = slurp_stream(f);
    const char* line = pre;
    Buf filtered;
    buf_init(&filtered);
    while (*line) {
      const char* end = strchr(line, '\n');
      size_t len = end ? (size_t)(end - line) : strlen(line);
      const char* p = line;
      while (*p == ' ' || *p == '\t') p++;
      if (!(*p == '#' && (isdigit((unsigned char)p[1]) ||
                          (p[1] == ' ' && isdigit((unsigned char)p[2]))))) {
        buf_addn(&filtered, line, len);
        buf_addc(&filtered, '\n');
      }
      line = end ? end + 1 : line + len;
    }
    free(pre);
    result = filtered.s;
  }
  fclose(f);
  unlink(inpath);
  unlink(outpath);
  buf_free(&cmd);
  return result;
}

static const char* find_array_start(const char* text, const char* name) {
  const char* p = text;
  size_t n = strlen(name);

  while ((p = strstr(p, name)) != NULL) {
    const char* q;
    bool left_ok =
      (p == text || !(isalnum((unsigned char)p[-1]) || p[-1] == '_'));
    if (!left_ok) {
      p += n;
      continue;
    }
    q = p + n;
    while (*q && isspace((unsigned char)*q)) q++;
    if (*q != '[') {
      p += n;
      continue;
    }
    q++;
    while (*q && isspace((unsigned char)*q)) q++;
    if (*q != ']') {
      p += n;
      continue;
    }
    q++;
    while (*q && isspace((unsigned char)*q)) q++;
    if (*q != '=') {
      p += n;
      continue;
    }
    q++;
    while (*q && isspace((unsigned char)*q)) q++;
    if (*q != '{') {
      p += n;
      continue;
    }
    return q;
  }
  return NULL;
}

static void split_fields(const char* row, StrList* out) {
  size_t i, len = strlen(row);
  int depth = 0;
  int state = 0; /* 0 none, 1 string, 2 char */
  Buf fld;

  buf_init(&fld);
  for (i = 0; i < len; i++) {
    char ch = row[i];
    if (state == 1) {
      buf_addc(&fld, ch);
      if (ch == '\\' && i + 1 < len)
        buf_addc(&fld, row[++i]);
      else if (ch == '"')
        state = 0;
      continue;
    }
    if (state == 2) {
      buf_addc(&fld, ch);
      if (ch == '\\' && i + 1 < len)
        buf_addc(&fld, row[++i]);
      else if (ch == '\'')
        state = 0;
      continue;
    }
    if (ch == '"') {
      state = 1;
      buf_addc(&fld, ch);
      continue;
    }
    if (ch == '\'') {
      state = 2;
      buf_addc(&fld, ch);
      continue;
    }
    if (ch == '{') {
      depth++;
      buf_addc(&fld, ch);
      continue;
    }
    if (ch == '}') {
      depth--;
      buf_addc(&fld, ch);
      continue;
    }
    if (ch == ',' && depth == 0) {
      sl_push(out, xstrdup(trim(fld.s)));
      buf_clear(&fld);
      continue;
    }
    buf_addc(&fld, ch);
  }
  if (fld.s[0]) sl_push(out, xstrdup(trim(fld.s)));
  buf_free(&fld);
}

static size_t extract_strings(const char* s, StrList* out) {
  size_t i, len = strlen(s);
  int state = 0;
  Buf str;

  buf_init(&str);
  for (i = 0; i < len; i++) {
    char ch = s[i];
    if (!state) {
      if (ch == '"') {
        state = 1;
        buf_clear(&str);
      }
    } else if (ch == '\\') {
      if (++i < len) {
        char c = s[i];
        if (c == 'n')
          c = '\n';
        else if (c == 't')
          c = '\t';
        else if (c == 'r')
          c = '\r';
        buf_addc(&str, c);
      }
    } else if (ch == '"') {
      sl_push(out, xstrdup(str.s));
      state = 0;
    } else {
      buf_addc(&str, ch);
    }
  }
  buf_free(&str);
  return out->n;
}

static char* unquote(const char* s) {
  StrList l = {0};
  char* result;
  extract_strings(s, &l);
  if (l.n > 0) {
    result = xstrdup(l.v[0]);
  } else {
    char* tmp = xstrdup(s);
    result = xstrdup(trim(tmp));
    free(tmp);
  }
  sl_free(&l);
  return result;
}

static char* clean_group(const char* s) {
  Buf b;
  const char* p;
  char *tmp, *t;

  buf_init(&b);
  for (p = s; *p; p++) {
    if (*p == ' ' || *p == '\t') {
      if (b.len && b.s[b.len - 1] != ' ') buf_addc(&b, ' ');
    } else {
      buf_addc(&b, *p);
    }
  }
  tmp = xstrdup(trim(b.s));
  buf_free(&b);
  t = trim(tmp);
  return t;
}

static void record_row(const char* row, const char* group, const char* kind) {
  StrList f = {0};
  split_fields(row, &f);
  if (strcmp(kind, "keys") == 0) {
    if (f.n >= 3) {
      if (nkeys == capkeys) {
        capkeys = capkeys ? capkeys * 2 : 32;
        keys = xrealloc(keys, capkeys * sizeof(*keys));
      }
      keys[nkeys].mod = xstrdup(f.v[0]);
      keys[nkeys].key = xstrdup(f.v[1]);
      keys[nkeys].fn = xstrdup(f.v[2]);
      keys[nkeys].arg = xstrdup(f.n >= 4 ? f.v[3] : "");
      keys[nkeys].group = xstrdup(group);
      nkeys++;
    }
  } else if (strcmp(kind, "buttons") == 0) {
    if (f.n >= 4) {
      if (nbuttons == capbuttons) {
        capbuttons = capbuttons ? capbuttons * 2 : 16;
        buttons = xrealloc(buttons, capbuttons * sizeof(*buttons));
      }
      buttons[nbuttons].click = xstrdup(f.v[0]);
      buttons[nbuttons].mask = xstrdup(f.v[1]);
      buttons[nbuttons].button = xstrdup(f.v[2]);
      buttons[nbuttons].fn = xstrdup(f.v[3]);
      buttons[nbuttons].arg = xstrdup(f.n >= 5 ? f.v[4] : "");
      nbuttons++;
    }
  } else if (strcmp(kind, "layouts") == 0) {
    if (f.n >= 1) {
      if (nlayouts == caplayouts) {
        caplayouts = caplayouts ? caplayouts * 2 : 8;
        layout_symbol =
          xrealloc(layout_symbol, caplayouts * sizeof(*layout_symbol));
        layout_func = xrealloc(layout_func, caplayouts * sizeof(*layout_func));
      }
      layout_symbol[nlayouts] = unquote(f.v[0]);
      layout_func[nlayouts] = f.n >= 2 ? xstrdup(f.v[1]) : xstrdup("");
      nlayouts++;
    }
  } else if (strcmp(kind, "scratchpads") == 0) {
    if (f.n >= 1) {
      if (nscratch == capscratch) {
        capscratch = capscratch ? capscratch * 2 : 8;
        scratch_name =
          xrealloc(scratch_name, capscratch * sizeof(*scratch_name));
      }
      scratch_name[nscratch++] = unquote(f.v[0]);
    }
  }
  sl_free(&f);
}

static void parse_array(const char* text, const char* name, const char* kind) {
  const char* start = find_array_start(text, name);
  const char* p;
  int depth = 0;
  int state = 0;
  int blank = 0;
  Buf row, group;
  const char* row_group = "";

  if (!start) return;
  buf_init(&row);
  buf_init(&group);
  for (p = start + 1; *p; p++) {
    char ch = *p;
    if (state == 1) {
      if (depth > 0) buf_addc(&row, ch);
      if (ch == '\\' && p[1]) {
        if (depth > 0)
          buf_addc(&row, *++p);
        else
          p++;
      } else if (ch == '"')
        state = 0;
      continue;
    }
    if (state == 2) {
      if (depth > 0) buf_addc(&row, ch);
      if (ch == '\\' && p[1]) {
        if (depth > 0)
          buf_addc(&row, *++p);
        else
          p++;
      } else if (ch == '\'')
        state = 0;
      continue;
    }
    if (ch == '"') {
      state = 1;
      if (depth > 0) buf_addc(&row, ch);
      continue;
    }
    if (ch == '\'') {
      state = 2;
      if (depth > 0) buf_addc(&row, ch);
      continue;
    }
    if (ch == '/' && p[1] == '/') {
      const char* nl = strchr(p + 2, '\n');
      if (depth == 0) {
        char* comment =
          xstrndup(p + 2, (size_t)((nl ? nl : p + strlen(p)) - (p + 2)));
        char* g = clean_group(comment);
        buf_clear(&group);
        buf_add(&group, g);
        free(g);
        free(comment);
        blank = 0;
      }
      if (!nl) break;
      p = nl - 1;
      continue;
    }
    if (ch == '/' && p[1] == '*') {
      const char* close = strstr(p + 2, "*/");
      if (!close) break;
      p = close + 1;
      continue;
    }
    if (depth == 0) {
      if (ch == '{') {
        depth = 1;
        buf_clear(&row);
        row_group = group.s;
        blank = 0;
      } else if (ch == '}') {
        break;
      } else if (ch == '\n') {
        blank++;
        if (blank >= 2) buf_clear(&group);
      } else if (ch != ' ' && ch != '\t' && ch != '\r') {
        blank = 0;
      }
    } else {
      if (ch == '{') {
        depth++;
        buf_addc(&row, ch);
      } else if (ch == '}') {
        depth--;
        if (depth > 0) buf_addc(&row, ch);
        if (depth == 0) {
          record_row(row.s, row_group, kind);
          buf_clear(&row);
          blank = 0;
        }
      } else {
        buf_addc(&row, ch);
      }
    }
  }
  buf_free(&row);
  buf_free(&group);
}

static void add_tag_field(const char* row) {
  char* label = unquote(row);
  if (*label) {
    if (ntags == captags) {
      captags = captags ? captags * 2 : 16;
      tag_labels = xrealloc(tag_labels, captags * sizeof(*tag_labels));
    }
    tag_labels[ntags++] = label;
  } else {
    free(label);
  }
}

static void parse_flat_array(const char* text, const char* name,
                             const char* kind) {
  const char* start = find_array_start(text, name);
  const char* p;
  int depth = 0;
  int state = 0;
  Buf row;

  (void)kind;
  if (!start) return;
  buf_init(&row);
  for (p = start + 1; *p; p++) {
    char ch = *p;
    if (state == 1) {
      buf_addc(&row, ch);
      if (ch == '\\' && p[1])
        buf_addc(&row, *++p);
      else if (ch == '"')
        state = 0;
      continue;
    }
    if (ch == '"') {
      state = 1;
      buf_addc(&row, ch);
      continue;
    }
    if (ch == '/' && p[1] == '/') {
      const char* nl = strchr(p + 2, '\n');
      if (!nl) break;
      p = nl - 1;
      continue;
    }
    if (ch == '/' && p[1] == '*') {
      const char* close = strstr(p + 2, "*/");
      if (!close) break;
      p = close + 1;
      continue;
    }
    if (ch == '{') {
      depth++;
      buf_addc(&row, ch);
      continue;
    }
    if (ch == '}') {
      if (depth == 0) {
        if (row.len) add_tag_field(row.s);
        buf_clear(&row);
        break;
      }
      depth--;
      buf_addc(&row, ch);
      continue;
    }
    if (ch == ',' && depth == 0) {
      add_tag_field(row.s);
      buf_clear(&row);
      continue;
    }
    buf_addc(&row, ch);
  }
  if (row.len) add_tag_field(row.s);
  buf_free(&row);
}

/* Return the value assigned to .key= inside arg, or an empty string. */
static char* scalar_arg(const char* arg, const char* key) {
  char* needle;
  const char* p;
  Buf nb;
  char* result = NULL;

  buf_init(&nb);
  buf_addc(&nb, '.');
  buf_add(&nb, key);
  needle = nb.s;

  p = arg;
  while ((p = strstr(p, needle)) != NULL) {
    const char* q = p + strlen(needle);
    while (*q && isspace((unsigned char)*q)) q++;
    if (*q == '=') {
      const char* end;
      q++;
      while (*q && isspace((unsigned char)*q)) q++;
      end = strchr(q, '}');
      result = xstrndup(q, end ? (size_t)(end - q) : strlen(q));
      trim(result);
      break;
    }
    p += strlen(needle);
  }
  buf_free(&nb);
  return result ? result : xstrdup("");
}

static bool zero_arg(const char* arg) {
  Buf b;
  const char* p;
  bool result;

  buf_init(&b);
  for (p = arg; *p; p++)
    if (*p != '{' && *p != '}' && *p != ' ' && *p != '\t' && *p != '\r' &&
        *p != '\n')
      buf_addc(&b, *p);
  result = strcmp(b.s, "0") == 0;
  buf_free(&b);
  return result;
}

static const char* tag_label(int index) {
  static char fallback[64];
  if (index >= 0 && (size_t)index < ntags) return tag_labels[index];
  snprintf(fallback, sizeof(fallback), "tag %d", index + 1);
  return fallback;
}

static bool tilde_zero(const char* s) {
  const char* p;
  for (p = s; *p; p++) {
    if (*p == '~') {
      p++;
      while (*p == ' ' || *p == '\t') p++;
      if (*p == '0') return true;
      if (!*p) return false;
    }
  }
  return false;
}

static char* tag_detail(const char* arg, const char* kind, const char* click) {
  char *v, *result = NULL;
  Buf b;

  if (tilde_zero(arg)) return xstrdup("all tags");
  v = scalar_arg(arg, "ui");
  if (!*v || strcmp(v, "0") == 0) {
    free(v);
    if (strcmp(kind, "buttons") == 0 && strcmp(click, "ClkTagBar") == 0)
      return xstrdup("clicked tag");
    return xstrdup("previous tag");
  }
  {
    const char* p = strstr(v, "<<");
    if (p) {
      p += 2;
      while (*p == ' ' || *p == '\t') p++;
      if (isdigit((unsigned char)*p)) {
        int idx = atoi(p);
        buf_init(&b);
        buf_add(&b, "tag ");
        buf_add(&b, tag_label(idx));
        result = b.s;
      }
    }
  }
  if (!result) {
    bool digits = *v != '\0';
    const char* p;
    for (p = v; *p; p++)
      if (!isdigit((unsigned char)*p)) {
        digits = false;
        break;
      }
    if (digits) {
      buf_init(&b);
      buf_add(&b, "tag ");
      buf_add(&b, tag_label(atoi(v)));
      result = b.s;
    }
  }
  free(v);
  return result ? result : xstrdup("");
}

static char* dir_word(const char* arg, const char* noun) {
  char* v = scalar_arg(arg, "i");
  char* result = NULL;
  Buf b;
  if (*v == '-' || (*v == '+' && isdigit((unsigned char)v[1])) ||
      isdigit((unsigned char)*v)) {
    buf_init(&b);
    buf_add(&b, *v == '-' ? "previous " : "next ");
    buf_add(&b, noun);
    result = b.s;
  }
  free(v);
  return result ? result : xstrdup("");
}

static char* sign_word(const char* v) {
  if (!*v) return xstrdup("");
  if (strtod(v, NULL) == 0.0) return xstrdup("reset");
  return xstrdup(*v == '-' ? "decrease" : "increase");
}

static char* concat3(const char* a, const char* b, const char* c) {
  Buf out;
  buf_init(&out);
  buf_add(&out, a);
  buf_add(&out, b);
  buf_add(&out, c);
  return out.s;
}

static char* arg_detail(const char* fn, const char* arg, const char* kind,
                        const char* click) {
  char *v, *s;

  if (strcmp(fn, "view") == 0 || strcmp(fn, "tag") == 0 ||
      strcmp(fn, "toggleview") == 0 || strcmp(fn, "toggletag") == 0)
    return tag_detail(arg, kind, click);

  if (strcmp(fn, "setlayout") == 0) {
    const char* p = strstr(arg, "layouts[");
    if (p) {
      p += 8;
      if (isdigit((unsigned char)*p)) {
        int idx = atoi(p);
        if (idx >= 0 && (size_t)idx < nlayouts)
          return concat3("layout ", layout_symbol[idx], "");
      }
    }
    if (zero_arg(arg)) return xstrdup("toggle layouts");
    return xstrdup("");
  }
  if (strcmp(fn, "cyclelayout") == 0) return dir_word(arg, "layout");
  if (strcmp(fn, "focusmon") == 0) return dir_word(arg, "monitor");
  if (strcmp(fn, "tagmon") == 0) {
    s = dir_word(arg, "monitor");
    if (!*s) {
      free(s);
      return xstrdup("");
    }
    v = concat3("move to ", s, "");
    free(s);
    return v;
  }
  if (strcmp(fn, "focusstack") == 0) return dir_word(arg, "client");
  if (strcmp(fn, "movestack") == 0) {
    v = scalar_arg(arg, "i");
    s = xstrdup(*v == '-' ? "backward" : "forward");
    free(v);
    return s;
  }
  if (strcmp(fn, "shiftview") == 0) {
    v = scalar_arg(arg, "i");
    s = xstrdup(*v == '-' ? "left" : "right");
    free(v);
    return s;
  }
  if (strcmp(fn, "incnmaster") == 0) {
    v = scalar_arg(arg, "i");
    s = sign_word(v);
    free(v);
    return s;
  }
  if (strcmp(fn, "setmfact") == 0) {
    v = scalar_arg(arg, "f");
    s = sign_word(v);
    free(v);
    if (!*s) {
      free(s);
      return xstrdup("");
    }
    v = concat3(s, " master area factor", "");
    free(s);
    return v;
  }
  if (strcmp(fn, "setcfact") == 0) {
    v = scalar_arg(arg, "f");
    s = sign_word(v);
    free(v);
    if (!*s) {
      free(s);
      return xstrdup("");
    }
    v = concat3(s, " client area factor", "");
    free(s);
    return v;
  }
  if (strcmp(fn, "setborderpx") == 0) {
    v = scalar_arg(arg, "i");
    if (strcmp(v, "default_border") == 0) {
      free(v);
      return xstrdup("restore default border width");
    }
    s = sign_word(v);
    free(v);
    if (!*s) {
      free(s);
      return xstrdup("");
    }
    v = concat3(s, " border width", "");
    free(s);
    return v;
  }
  if (strcmp(fn, "incrgaps") == 0 || strcmp(fn, "incrigaps") == 0 ||
      strcmp(fn, "incrogaps") == 0 || strcmp(fn, "incrihgaps") == 0 ||
      strcmp(fn, "incrivgaps") == 0 || strcmp(fn, "incrohgaps") == 0 ||
      strcmp(fn, "incrovgaps") == 0) {
    v = scalar_arg(arg, "i");
    s = sign_word(v);
    free(v);
    return s;
  }
  if (strcmp(fn, "tabmode") == 0) return xstrdup("cycle modes");
  if (strcmp(fn, "focuswin") == 0 && strcmp(kind, "buttons") == 0 &&
      strcmp(click, "ClkTabBar") == 0)
    return xstrdup("clicked tab");
  return xstrdup("");
}

static char* spawn_command(const char* arg) {
  StrList l = {0};
  size_t i;
  char* result = xstrdup("");
  extract_strings(arg, &l);
  for (i = 0; i + 1 < l.n; i++) {
    if (strcmp(l.v[i], "-c") == 0) {
      free(result);
      result = xstrdup(l.v[i + 1]);
      sl_free(&l);
      return result;
    }
  }
  if (l.n >= 1) {
    free(result);
    result = xstrdup(l.v[l.n - 1]);
  }
  sl_free(&l);
  return result;
}

static char* humanize(const char* s) {
  Buf out;
  size_t i, len = strlen(s);
  char prev = '\0';

  buf_init(&out);
  for (i = 0; i < len; i++) {
    char ch = s[i] == '_' ? ' ' : s[i];
    if (i > 0 && isupper((unsigned char)ch) && prev != '\0' &&
        !isupper((unsigned char)prev) && prev != ' ')
      buf_addc(&out, ' ');
    buf_addc(&out, ch);
    prev = ch;
  }
  if (out.s[0]) out.s[0] = (char)toupper((unsigned char)out.s[0]);
  return out.s;
}

static char* action_text(const char* fn, const char* arg, const char* kind,
                         const char* click) {
  const char* d;
  char* detail;
  Buf out;

  if (strcmp(fn, "spawn") == 0) {
    char* cmd = spawn_command(arg);
    if (*cmd) {
      buf_init(&out);
      buf_add(&out, "Execute command: ");
      buf_add(&out, cmd);
      free(cmd);
      return out.s;
    }
    free(cmd);
  }
  if (strcmp(fn, "togglescratch") == 0) {
    char* v = scalar_arg(arg, "ui");
    if (*v && atoi(v) >= 0 && (size_t)atoi(v) < nscratch) {
      buf_init(&out);
      buf_add(&out, "Toggle scratchpad ");
      buf_add(&out, scratch_name[atoi(v)]);
      buf_addc(&out, '.');
      free(v);
      return out.s;
    }
    free(v);
    return xstrdup("Toggle a scratchpad.");
  }

  d = brief_for(fn);
  if (!*d) d = NULL;
  buf_init(&out);
  if (d)
    buf_add(&out, d);
  else {
    char* h = humanize(fn);
    buf_add(&out, h);
    free(h);
  }
  detail = arg_detail(fn, arg, kind, click);
  if (*detail) {
    if (out.len && out.s[out.len - 1] == '.') out.s[--out.len] = '\0';
    buf_add(&out, " (");
    buf_add(&out, detail);
    buf_addc(&out, ')');
  }
  free(detail);
  if (!out.len || !strchr(".!?", out.s[out.len - 1])) buf_addc(&out, '.');
  return out.s;
}

static char* format_mod(const char* mod) {
  const char* p = mod;
  const char* start = mod;
  Buf other;
  bool sup = false, shf = false, ctl = false, alt = false;
  char* result;

  buf_init(&other);
  for (;;) {
    if (*p == '|' || *p == '\0') {
      char* tok = xstrndup(start, (size_t)(p - start));
      char* t = trim(tok);
      if (*t && strcmp(t, "0") != 0) {
        if (strcmp(t, "Mod4Mask") == 0)
          sup = true;
        else if (strcmp(t, "ShiftMask") == 0)
          shf = true;
        else if (strcmp(t, "ControlMask") == 0)
          ctl = true;
        else if (strcmp(t, "Mod1Mask") == 0)
          alt = true;
        else {
          size_t n = strlen(t);
          if (n >= 4 && strcmp(t + n - 4, "Mask") == 0) t[n - 4] = '\0';
          if (other.len) buf_addc(&other, '+');
          buf_add(&other, t);
        }
      }
      free(tok);
      if (*p == '\0') break;
      start = p + 1;
    }
    p++;
  }

  {
    Buf out;
    buf_init(&out);
    if (other.len) buf_add(&out, other.s);
    if (sup) {
      if (out.len) buf_addc(&out, '+');
      buf_add(&out, "SUPER");
    }
    if (shf) {
      if (out.len) buf_addc(&out, '+');
      buf_add(&out, "SHIFT");
    }
    if (ctl) {
      if (out.len) buf_addc(&out, '+');
      buf_add(&out, "CTRL");
    }
    if (alt) {
      if (out.len) buf_addc(&out, '+');
      buf_add(&out, "ALT");
    }
    result = out.s;
  }
  buf_free(&other);
  return result;
}

static char* format_key(const char* k) {
  char* s = xstrdup(k);
  char* t = trim(s);
  char* result;

  if (starts_with(t, "XF86XK_")) {
    Buf b;
    buf_init(&b);
    buf_add(&b, "XF86");
    buf_add(&b, t + 7);
    free(s);
    return b.s;
  }
  if (starts_with(t, "XK_")) {
    t += 3;
    if (strcmp(t, "space") == 0)
      result = xstrdup("Space");
    else if (strcmp(t, "slash") == 0)
      result = xstrdup("/");
    else if (strcmp(t, "question") == 0)
      result = xstrdup("?");
    else if (strcmp(t, "comma") == 0)
      result = xstrdup(",");
    else if (strcmp(t, "period") == 0)
      result = xstrdup(".");
    else if (strcmp(t, "minus") == 0)
      result = xstrdup("-");
    else if (strcmp(t, "equal") == 0)
      result = xstrdup("=");
    else
      result = xstrdup(t);
    free(s);
    return result;
  }
  result = xstrdup(t);
  free(s);
  return result;
}

static const char* click_label(const char* c) {
  if (strcmp(c, "ClkTagBar") == 0) return "tag bar";
  if (strcmp(c, "ClkLtSymbol") == 0) return "layout symbol";
  if (strcmp(c, "ClkStatusText") == 0) return "status text";
  if (strcmp(c, "ClkWinTitle") == 0) return "window title";
  if (strcmp(c, "ClkClientWin") == 0) return "client window";
  if (strcmp(c, "ClkRootWin") == 0) return "root window";
  if (strcmp(c, "ClkTabBar") == 0) return "tab bar";
  if (strcmp(c, "ClkTabPrev") == 0) return "previous tab";
  if (strcmp(c, "ClkTabNext") == 0) return "next tab";
  if (strcmp(c, "ClkTabClose") == 0) return "close tab";
  return NULL;
}

static void print_keys(void) {
  const char* current_group = "\001";
  size_t i;

  printf(".SH KEYBOARD SHORTCUTS\n");
  printf(".P\n");
  printf(
    "Keyboard shortcuts are listed in the order in which they appear in\n");
  printf(".IR config.h .\n");
  for (i = 0; i < nkeys; i++) {
    if (strcmp(keys[i].group, current_group) != 0) {
      char* g;
      if (*keys[i].group)
        g = ucfirst(xstrdup(keys[i].group));
      else
        g = xstrdup("Other bindings");
      printf(".SS %s\n", esc(g));
      free(g);
      current_group = keys[i].group;
    }
    {
      char* mod = format_mod(keys[i].mod);
      char* key = format_key(keys[i].key);
      char* action = action_text(keys[i].fn, keys[i].arg, "keys", "");
      Buf label;
      buf_init(&label);
      if (*mod) {
        buf_add(&label, mod);
        buf_addc(&label, '+');
      }
      buf_add(&label, key);
      printf(".TP\n");
      printf(".B %s\n", esc(label.s));
      printf("\\&%s\n", esc(action));
      buf_free(&label);
      free(mod);
      free(key);
      free(action);
    }
  }
}

static void print_buttons(void) {
  size_t i;

  printf(".SH MOUSE BINDINGS\n");
  printf(".P\n");
  printf("Mouse bindings are listed in the order in which they appear in\n");
  printf(".IR config.h .\n");
  for (i = 0; i < nbuttons; i++) {
    char* mod = format_mod(buttons[i].mask);
    char* action =
      action_text(buttons[i].fn, buttons[i].arg, "buttons", buttons[i].click);
    const char* cl = click_label(buttons[i].click);
    Buf label;
    buf_init(&label);
    if (*mod) {
      buf_add(&label, mod);
      buf_addc(&label, '+');
    }
    buf_add(&label, trim(buttons[i].button));
    buf_add(&label, " on ");
    if (cl) {
      buf_add(&label, cl);
    } else {
      char* h = humanize(buttons[i].click);
      buf_add(&label, h);
      free(h);
    }
    printf(".TP\n");
    printf(".B %s\n", esc(label.s));
    printf("\\&%s\n", esc(action));
    buf_free(&label);
    free(mod);
    free(action);
  }
}

int main(int argc, char** argv) {
  const char* config_path = argc > 1 ? argv[1] : "config.h";
  char *config, *dir, *preprocessed, *version;
  const char* cc = getenv("CC");
  Buf title;

  config = read_file(config_path);
  dir = dirname_of(config_path);
  load_source_briefs(dir);

  version = getenv("VERSION") ? xstrdup(getenv("VERSION")) : NULL;
  if (!version || !*version) {
    char* verpath = join_path(dir, "config.mk");
    if (access(verpath, R_OK) == 0)
      version = read_version(dir);
    else
      version = xstrdup("");
    free(verpath);
  }

  preprocessed = preprocess_config(config, dir, cc);
  free(config);

  parse_flat_array(preprocessed, "tags", "tags");
  parse_array(preprocessed, "layouts", "layouts");
  parse_array(preprocessed, "scratchpads", "scratchpads");
  parse_array(preprocessed, "keys", "keys");
  parse_array(preprocessed, "buttons", "buttons");
  free(preprocessed);

  if (nkeys == 0 || nbuttons == 0)
    die("could not parse keys[] or buttons[] from %s", config_path);

  buf_init(&title);
  buf_add(&title, "dwm");
  if (*version) {
    buf_addc(&title, ' ');
    buf_add(&title, version);
  }
  printf(".TH DWM\\-KEYS 1 \"%s\" \"dwm manual\"\n", title.s);
  printf(".SH NAME\n");
  printf("dwm\\-keys \\- keyboard and mouse bindings for dwm\n");
  printf(".SH DESCRIPTION\n");
  printf("This page lists the keyboard shortcuts and mouse bindings of\n");
  printf(".BR dwm (1).\n");
  printf("It is generated from the\n");
  printf(".B Key\n");
  printf("and\n");
  printf(".B Button\n");
  printf("arrays in\n");
  printf(".IR config.h .\n");
  printf(
    "Modifiers are shown as SUPER, SHIFT, CTRL, and ALT; SUPER is Mod4Mask\n");
  printf("and ALT is Mod1Mask in the source configuration.\n");

  print_keys();
  print_buttons();

  printf(".SH SEE ALSO\n");
  printf(".BR dwm (1),\n");
  printf(".BR man (1)\n");

  free(title.s);
  free(version);
  free(dir);
  return 0;
}
