#!/usr/bin/awk -f
# Generate dwm-keys(1) from the Key and Button arrays in config.h.
#
# The shell wrapper feeds this program a marker line, the preprocessed
# configuration, and then every C source file.  Key/button rows are parsed from
# the configuration while action descriptions are read from the Doxygen
# @brief comments in the sources.

function trim(s) {
  sub(/^[ \t\r\n]+/, "", s)
  sub(/[ \t\r\n]+$/, "", s)
  return s
}

function ucfirst(s) {
  if (s == "")
    return s
  return toupper(substr(s, 1, 1)) substr(s, 2)
}

function esc(s) {
  gsub(/\\/, "\\\\", s)
  return s
}

function unescape_char(c) {
  if (c == "n")
    return "\n"
  if (c == "t")
    return "\t"
  if (c == "r")
    return "\r"
  return c
}

function extract_strings(s, arr,   i, n, ch, state, str) {
  n = 0
  state = 0
  str = ""
  for (i = 1; i <= length(s); i++) {
    ch = substr(s, i, 1)
    if (!state) {
      if (ch == "\"") {
        state = 1
        str = ""
      }
    } else if (ch == "\\") {
      i++
      str = str unescape_char(substr(s, i, 1))
    } else if (ch == "\"") {
      arr[++n] = str
      state = 0
    } else {
      str = str ch
    }
  }
  return n
}

function unquote(s, arr, n) {
  n = extract_strings(s, arr)
  if (n > 0)
    return arr[1]
  return trim(s)
}

function defname(line,   s) {
  s = line
  sub(/^[ \t]+/, "", s)
  if (s ~ /^#/ || s ~ /;[ \t]*$/ || s !~ /\(/)
    return ""
  if (!match(s, /[A-Za-z_][A-Za-z_0-9]*[ \t]*\(/))
    return ""
  s = substr(s, RSTART, RLENGTH)
  sub(/[ \t]*\($/, "", s)
  return s
}

function parse_source_line(line,   s) {
  if (!in_doc) {
    if (line ~ /\/\*\*/) {
      in_doc = 1
      brief = ""
      in_brief = 0
    } else {
      if (pending != "") {
        if (line ~ /^[ \t\r]*$/)
          return
        s = defname(line)
        if (s != "") {
          if (!(s in brief_by_func))
            brief_by_func[s] = pending
          pending = ""
        } else {
          pending = ""
        }
      }
      return
    }
  }

  if (line ~ /@brief([ \t]|$)/) {
    s = line
    sub(/^.*@brief[ \t]+/, "", s)
    sub(/[ \t]*\*\/.*$/, "", s)
    sub(/^[ \t]*\*[ \t]*/, "", s)
    brief = trim(s)
    in_brief = 1
  } else if (in_brief) {
    if (line ~ /@[A-Za-z]+/ || line ~ /\*\//) {
      in_brief = 0
    } else {
      s = line
      sub(/^[ \t]*\*[ \t]*/, "", s)
      s = trim(s)
      if (s != "")
        brief = brief " " s
    }
  }

  if (line ~ /\*\//) {
    in_doc = 0
    in_brief = 0
    pending = brief
  }
}

function clean_group(s) {
  s = trim(s)
  gsub(/[ \t]+/, " ", s)
  return s
}

function split_fields(row, out,   i, n, ch, depth, fld, state) {
  n = 0
  depth = 0
  fld = ""
  state = ""
  for (i = 1; i <= length(row); i++) {
    ch = substr(row, i, 1)
    if (state == "str") {
      fld = fld ch
      if (ch == "\\") {
        i++
        fld = fld substr(row, i, 1)
      } else if (ch == "\"")
        state = ""
      continue
    }
    if (state == "chr") {
      fld = fld ch
      if (ch == "\\") {
        i++
        fld = fld substr(row, i, 1)
      } else if (ch == "'")
        state = ""
      continue
    }
    if (ch == "\"") {
      state = "str"
      fld = fld ch
      continue
    }
    if (ch == "'") {
      state = "chr"
      fld = fld ch
      continue
    }
    if (ch == "{") {
      depth++
      fld = fld ch
      continue
    }
    if (ch == "}") {
      depth--
      fld = fld ch
      continue
    }
    if (ch == "," && depth == 0) {
      out[++n] = trim(fld)
      fld = ""
      continue
    }
    fld = fld ch
  }
  if (fld != "")
    out[++n] = trim(fld)
  return n
}

function parse_flat_array(text, pattern, kind,   pos, len, i, ch, depth, row, state, j) {
  if (!match(text, pattern))
    return
  pos = RSTART + RLENGTH
  len = length(text)
  while (pos <= len && substr(text, pos, 1) != "{")
    pos++
  if (pos > len)
    return

  depth = 0
  row = ""
  state = ""
  for (i = pos + 1; i <= len; i++) {
    ch = substr(text, i, 1)
    if (state == "str") {
      row = row ch
      if (ch == "\\") {
        i++
        row = row substr(text, i, 1)
      } else if (ch == "\"")
        state = ""
      continue
    }
    if (ch == "\"") {
      state = "str"
      row = row ch
      continue
    }
    if (ch == "/" && substr(text, i + 1, 1) == "*") {
      j = index(substr(text, i + 2), "*/")
      if (j == 0)
        i = len
      else
        i = i + j + 2
      continue
    }
    if (ch == "/" && substr(text, i + 1, 1) == "/") {
      while (i <= len && substr(text, i, 1) != "\n")
        i++
      continue
    }
    if (ch == "{") {
      depth++
      row = row ch
      continue
    }
    if (ch == "}") {
      if (depth == 0)
        break
      depth--
      row = row ch
      continue
    }
    row = row ch
  }
  record_row(row, "", kind)
}

function record_row(row, group, kind,   f, n, i) {
  n = split_fields(row, f)
  if (kind == "keys") {
    if (n < 3)
      return
    key_n++
    key_mod[key_n] = f[1]
    key_key[key_n] = f[2]
    key_func[key_n] = f[3]
    key_arg[key_n] = (n >= 4 ? f[4] : "")
    key_group[key_n] = group
  } else if (kind == "buttons") {
    if (n < 4)
      return
    button_n++
    button_click[button_n] = f[1]
    button_mask[button_n] = f[2]
    button_button[button_n] = f[3]
    button_func[button_n] = f[4]
    button_arg[button_n] = (n >= 5 ? f[5] : "")
  } else if (kind == "tags") {
    if (n < 1)
      return
    for (i = 1; i <= n; i++)
      tag_labels[tag_n++] = unquote(f[i])
  } else if (kind == "layouts") {
    if (n < 1)
      return
    layout_n++
    layout_symbol[layout_n - 1] = unquote(f[1])
    layout_func[layout_n - 1] = (n >= 2 ? f[2] : "")
  } else if (kind == "scratchpads") {
    if (n < 1)
      return
    scratch_n++
    scratch_name[scratch_n - 1] = unquote(f[1])
  }
}

function parse_array(text, pattern, kind,   pos, len, i, ch, depth, row, comment, j, group, row_group, blank, state) {
  if (!match(text, pattern))
    return
  pos = RSTART + RLENGTH
  len = length(text)
  while (pos <= len && substr(text, pos, 1) != "{")
    pos++
  if (pos > len)
    return

  depth = 0
  row = ""
  group = ""
  row_group = ""
  blank = 0
  state = ""

  for (i = pos + 1; i <= len; i++) {
    ch = substr(text, i, 1)

    if (state == "str") {
      if (depth > 0)
        row = row ch
      if (ch == "\\") {
        i++
        if (depth > 0)
          row = row substr(text, i, 1)
      } else if (ch == "\"")
        state = ""
      continue
    }
    if (state == "chr") {
      if (depth > 0)
        row = row ch
      if (ch == "\\") {
        i++
        if (depth > 0)
          row = row substr(text, i, 1)
      } else if (ch == "'")
        state = ""
      continue
    }
    if (ch == "\"") {
      state = "str"
      if (depth > 0)
        row = row ch
      continue
    }
    if (ch == "'") {
      state = "chr"
      if (depth > 0)
        row = row ch
      continue
    }

    if (ch == "/" && substr(text, i + 1, 1) == "/") {
      comment = substr(text, i + 2)
      sub(/\n.*$/, "", comment)
      if (depth == 0) {
        group = clean_group(comment)
        blank = 0
      }
      while (i <= len && substr(text, i, 1) != "\n")
        i++
      continue
    }
    if (ch == "/" && substr(text, i + 1, 1) == "*") {
      j = index(substr(text, i + 2), "*/")
      if (j == 0)
        i = len
      else
        i = i + j + 2
      continue
    }

    if (depth == 0) {
      if (ch == "{") {
        depth = 1
        row = ""
        row_group = group
        blank = 0
      } else if (ch == "}") {
        break
      } else if (ch == "\n") {
        blank++
        if (blank >= 2)
          group = ""
      } else if (ch != " " && ch != "\t" && ch != "\r") {
        blank = 0
      }
    } else {
      if (ch == "{") {
        depth++
        row = row ch
      } else if (ch == "}") {
        depth--
        if (depth > 0)
          row = row ch
        if (depth == 0) {
          record_row(row, row_group, kind)
          row = ""
          blank = 0
        }
      } else {
        row = row ch
      }
    }
  }
}

function scalar_arg(arg, key,   s) {
  if (!match(arg, "\\." key "[ \t]*=[ \t]*[^}]+"))
    return ""
  s = substr(arg, RSTART, RLENGTH)
  sub(/^\.[A-Za-z_][A-Za-z_0-9]*[ \t]*=[ \t]*/, "", s)
  sub(/[ \t]*$/, "", s)
  return s
}

function zero_arg(arg,   s) {
  s = arg
  gsub(/[{} \t\r\n]/, "", s)
  return s == "0"
}

function tag_label(i) {
  if (i in tag_labels)
    return tag_labels[i]
  return "tag " (i + 1)
}

function tag_detail(arg, kind, click,   v, s) {
  if (arg ~ /~[ \t]*0/)
    return "all tags"
  v = scalar_arg(arg, "ui")
  if (v == "" || v == "0") {
    if (kind == "buttons" && click == "ClkTagBar")
      return "clicked tag"
    return "previous tag"
  }
  if (match(v, /<<[ \t]*[0-9]+/)) {
    s = substr(v, RSTART, RLENGTH)
    gsub(/[^0-9]/, "", s)
    return "tag " tag_label(s + 0)
  }
  if (v ~ /^[0-9]+$/)
    return "tag " tag_label(v + 0)
  return ""
}

function dir_word(arg, noun,   v) {
  v = scalar_arg(arg, "i")
  if (v ~ /^-/)
    return "previous " noun
  if (v ~ /^\+?[0-9]/)
    return "next " noun
  return ""
}

function sign_word(v) {
  if (v == "")
    return ""
  if (v + 0 == 0)
    return "reset"
  if (v ~ /^-/)
    return "decrease"
  return "increase"
}

function arg_detail(fn, arg, kind, click,   v, s, idx) {
  if (fn == "view" || fn == "tag" || fn == "toggleview" || fn == "toggletag")
    return tag_detail(arg, kind, click)

  if (fn == "setlayout") {
    if (match(arg, /layouts\[[0-9]+\]/)) {
      s = substr(arg, RSTART, RLENGTH)
      sub(/^layouts\[/, "", s)
      sub(/\]$/, "", s)
      idx = s + 0
      if (idx in layout_symbol)
        return "layout " layout_symbol[idx]
      return ""
    }
    if (zero_arg(arg))
      return "toggle layouts"
    return ""
  }
  if (fn == "cyclelayout")
    return dir_word(arg, "layout")
  if (fn == "focusmon")
    return dir_word(arg, "monitor")
  if (fn == "tagmon") {
    s = dir_word(arg, "monitor")
    return (s == "" ? "" : "move to " s)
  }
  if (fn == "focusstack")
    return dir_word(arg, "client")
  if (fn == "movestack") {
    v = scalar_arg(arg, "i")
    return (v ~ /^-/ ? "backward" : "forward")
  }
  if (fn == "shiftview") {
    v = scalar_arg(arg, "i")
    return (v ~ /^-/ ? "left" : "right")
  }
  if (fn == "incnmaster") {
    v = scalar_arg(arg, "i")
    return sign_word(v)
  }
  if (fn == "setmfact") {
    v = scalar_arg(arg, "f")
    s = sign_word(v)
    return (s == "" ? "" : s " master area factor")
  }
  if (fn == "setcfact") {
    v = scalar_arg(arg, "f")
    s = sign_word(v)
    return (s == "" ? "" : s " client area factor")
  }
  if (fn == "setborderpx") {
    v = scalar_arg(arg, "i")
    if (v == "default_border")
      return "restore default border width"
    s = sign_word(v)
    return (s == "" ? "" : s " border width")
  }
  if (fn == "incrgaps" || fn == "incrigaps" || fn == "incrogaps" ||
      fn == "incrihgaps" || fn == "incrivgaps" ||
      fn == "incrohgaps" || fn == "incrovgaps") {
    v = scalar_arg(arg, "i")
    return sign_word(v)
  }
  if (fn == "tabmode")
    return "cycle modes"
  if (fn == "focuswin" && kind == "buttons" && click == "ClkTabBar")
    return "clicked tab"
  return ""
}

function spawn_command(arg, a, n, i) {
  n = extract_strings(arg, a)
  for (i = 1; i < n; i++)
    if (a[i] == "-c")
      return a[i + 1]
  if (n >= 1)
    return a[n]
  return ""
}

function humanize(s,   out, i, ch, prev) {
  gsub(/_/, " ", s)
  out = ""
  prev = ""
  for (i = 1; i <= length(s); i++) {
    ch = substr(s, i, 1)
    if (i > 1 && ch ~ /[A-Z]/ && prev !~ /[A-Z ]/)
      out = out " "
    out = out ch
    prev = ch
  }
  return ucfirst(out)
}

function action_text(fn, arg, kind, click,   d, dt, cmd, v) {
  if (fn == "spawn") {
    cmd = spawn_command(arg)
    if (cmd != "")
      return "Execute command: " cmd
  }
  if (fn == "togglescratch") {
    v = scalar_arg(arg, "ui")
    if (v != "" && (v + 0) in scratch_name)
      return "Toggle scratchpad " scratch_name[v + 0] "."
    return "Toggle a scratchpad."
  }

  d = brief_by_func[fn]
  if (d == "")
    d = humanize(fn)
  dt = arg_detail(fn, arg, kind, click)
  if (dt != "") {
    sub(/[.]$/, "", d)
    d = d " (" dt ")"
  }
  if (d !~ /[.!?]$/)
    d = d "."
  return d
}

function format_mod(mod,   n, i, parts, tok, out, sup, shf, ctl, alt, other) {
  if (mod == "" || mod == "0")
    return ""
  n = split(mod, parts, /\|/)
  sup = shf = ctl = alt = other = 0
  out = ""
  for (i = 1; i <= n; i++) {
    tok = trim(parts[i])
    if (tok == "" || tok == "0")
      continue
    if (tok == "Mod4Mask")
      sup = 1
    else if (tok == "ShiftMask")
      shf = 1
    else if (tok == "ControlMask")
      ctl = 1
    else if (tok == "Mod1Mask")
      alt = 1
    else {
      sub(/Mask$/, "", tok)
      out = out (out == "" ? "" : "+") tok
      other = 1
    }
  }
  if (sup)
    out = "SUPER" (out == "" ? "" : "+" out)
  if (shf)
    out = (out == "" ? "" : out "+") "SHIFT"
  if (ctl)
    out = (out == "" ? "" : out "+") "CTRL"
  if (alt)
    out = (out == "" ? "" : out "+") "ALT"
  return out
}

function format_key(k,   s) {
  s = trim(k)
  if (s ~ /^XF86XK_/)
    return "XF86" substr(s, 8)
  if (s ~ /^XK_/) {
    s = substr(s, 4)
    if (s == "space")
      return "Space"
    if (s == "slash")
      return "/"
    if (s == "question")
      return "?"
    if (s == "comma")
      return ","
    if (s == "period")
      return "."
    if (s == "minus")
      return "-"
    if (s == "equal")
      return "="
    return s
  }
  return s
}

function format_button(b) {
  return trim(b)
}

function click_label(c) {
  if (c == "ClkTagBar")
    return "tag bar"
  if (c == "ClkLtSymbol")
    return "layout symbol"
  if (c == "ClkStatusText")
    return "status text"
  if (c == "ClkWinTitle")
    return "window title"
  if (c == "ClkClientWin")
    return "client window"
  if (c == "ClkRootWin")
    return "root window"
  if (c == "ClkTabBar")
    return "tab bar"
  if (c == "ClkTabPrev")
    return "previous tab"
  if (c == "ClkTabNext")
    return "next tab"
  if (c == "ClkTabClose")
    return "close tab"
  return humanize(c)
}

/^###CONFIG###$/ {
  mode = "config"
  next
}
/^###SOURCE / {
  mode = "source"
  next
}
mode == "config" {
  cfg = cfg $0 "\n"
  next
}
mode == "source" {
  parse_source_line($0)
  next
}

END {
  parse_flat_array(cfg, "tags\\[\\][ \t]*=", "tags")
  parse_array(cfg, "layouts\\[\\][ \t]*=", "layouts")
  parse_array(cfg, "scratchpads\\[\\][ \t]*=", "scratchpads")
  parse_array(cfg, "keys\\[\\][ \t]*=", "keys")
  parse_array(cfg, "buttons\\[\\][ \t]*=", "buttons")

  if (key_n == 0 || button_n == 0) {
    print "dwm-keys: could not parse keys[] or buttons[] from config.h" > "/dev/stderr"
    exit 1
  }

  print ".TH DWM\\-KEYS 1 \"" (version == "" ? "dwm" : "dwm " version) "\" \"dwm manual\""
  print ".SH NAME"
  print "dwm\\-keys \\- keyboard and mouse bindings for dwm"
  print ".SH DESCRIPTION"
  print "This page lists the keyboard shortcuts and mouse bindings of"
  print ".BR dwm (1)."
  print "It is generated from the"
  print ".B Key"
  print "and"
  print ".B Button"
  print "arrays in"
  print ".IR config.h ."
  print "Modifiers are shown as SUPER, SHIFT, CTRL, and ALT; SUPER is Mod4Mask"
  print "and ALT is Mod1Mask in the source configuration."

  print ".SH KEYBOARD SHORTCUTS"
  print ".P"
  print "Keyboard shortcuts are listed in the order in which they appear in"
  print ".IR config.h ."
  current_group = "\001"
  for (i = 1; i <= key_n; i++) {
    if (key_group[i] != current_group) {
      g = key_group[i]
      if (g == "")
        g = "Other bindings"
      else
        g = ucfirst(g)
      print ".SS " esc(g)
      current_group = key_group[i]
    }
    mod = format_mod(key_mod[i])
    key = format_key(key_key[i])
    label = (mod == "" ? "" : mod "+") key
    print ".TP"
    print ".B " esc(label)
    print "\\&" esc(action_text(key_func[i], key_arg[i], "keys", ""))
  }

  print ".SH MOUSE BINDINGS"
  print ".P"
  print "Mouse bindings are listed in the order in which they appear in"
  print ".IR config.h ."
  for (i = 1; i <= button_n; i++) {
    mod = format_mod(button_mask[i])
    label = (mod == "" ? "" : mod "+") format_button(button_button[i])
    label = label " on " click_label(button_click[i])
    print ".TP"
    print ".B " esc(label)
    print "\\&" esc(action_text(button_func[i], button_arg[i], "buttons", button_click[i]))
  }

  print ".SH SEE ALSO"
  print ".BR dwm (1),"
  print ".BR man (1)"
}
